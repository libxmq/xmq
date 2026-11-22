# Parsing ixml fast: characters as tokens

How the C ixml engine (the ixml grammar translated to a yaep grammar) turns the
unicode characters of the input into tokens, why the tokens are limited to the
characters actually used in the input, and what is still slow.

## The problem

An ixml grammar works on characters, and a charset can name a huge number of them:
`[L]` is every letter (about 130 000 code points), `~["a"]` is every code point
except `a`, more than a million. The Earley parser in yaep tracks the possible next
tokens of every state in a bitset with one bit per terminal (`terminal_bitset_t`,
sized by `num_terminals`, see `terminal_bitset_create` in
`src/main/c/parts/yaep_terminal_bitset.c`). With one terminal per possible code
point, 0x110000 of them, every bitset would be 136 KB and the grammar would have
a rule alternative for every character of every charset.

## The solution: only the characters of the input are tokens

A parse only ever sees the characters in its input. So before each parse the
grammar is narrowed down to those characters.

### 1. Scan the input

`xmqParseBufferWithIXML` (`src/main/c/xmq.c`) first checks that the input is valid
UTF-8, then calls `scan_content_fixup_charsets` (`src/main/c/parts/ixml.c`), which
decodes the input once:

- every code point seen for the first time gets a terminal named `#hex`, eg `#61`
  for `a`, with the code point as its code, unless the grammar already has it (a
  literal like `'a'`). This also gives better errors: a character that is in no rule
  is a syntax error at line:col instead of an unknown token.
- `state->used_unicodes` remembers the code points seen by earlier parses with the
  same grammar. With `--lines` every line is a parse, and only the characters that
  are new on a line are added.

### 2. Expand the charsets to the used characters

Every charset in the grammar is a nonterminal. After the scan it gets one rule
alternative per used character it contains (`add_single_char_rule`):

- an included range, `["a"-"z"]`, gets the used characters within the range.
- an included category, `[L]`, gets the used characters of the category.
- an excluded charset, `~["a"; L]`, gets the used characters that are in none of its
  parts.

So for the input `Hej aab!` the charset `[L]` becomes the choice of `H e j a b`.

### 3. Dense terminal numbers and small bitsets

The grammar is then read into yaep (`yaep_read_grammar` with
`ixml_to_yaep_read_terminal`). yaep numbers the terminals densely, `term_id`
0..N-1, where N is the number of literal characters in the grammar plus the
characters of the input. The lookahead bitsets have N bits. For a normal text
input N is a few hundred, not a million.

### 4. From input bytes to tokens

During the parse `default_read_token` (`src/main/c/parts/yaep.c`) decodes the next
UTF-8 character and returns its code point as the token code. yaep maps the code to
its terminal with `symb_find_by_code` (`src/main/c/parts/yaep_symbols.c`):

- normally through `symb_code_trans_vect`, an array indexed by `code - min_code`,
  built by `symb_finish_adding_terms`. Its size is the range of the codes, max-min,
  not the number of terminals.
- if the codes are too far apart, more than `MAX_SYMB_CODE_TRANS_VECT_SIZE` (200000),
  eg an input with `a` (0x61) and a tag character (0xe0001), the array is not built
  and the terminal is found in the hash table `map_code_to_symb`. (This used to be an
  assert that aborted the parse, see `tests/ixml_parse_066_far_apart_code_points.test`.)

## Only the new characters are scanned

`scan_content_fixup_charsets` used to cost about a million steps on every parse, also
on every line with `--lines`: a 1 MB `used` array was allocated and cleared, every
excluded charset `~[...]` looped over all 0x110000 code points and every category
charset, eg `[L]`, over all members of the category (about 130 000).

Now it collects the code points that are new in this parse into a small sorted
vector, and tests each one against the parts of each charset (a range test, or a
binary search in the sorted category table with `category_has_code`). If no code point
is new, which is the common case with `--lines` after the first lines, the charsets are
not touched at all, the rules added by the earlier parses are already complete. The
rules are still added in the same order as before, so the grammar and the traces
(`--traceixml`) are unchanged.

The benchmark `ixml_lines` (`make run_benchmarks`, 3000 lines parsed with `--lines`)
went from 1.365 s to 0.912 s, the scan went from 23% of the run time to 0.1%.

## Future performance improvements

Measured with `perf` on the benchmark `ixml_lines`, where C (0.91 s) is still slower
than xmqj (0.50 s). Most of the time is now spent inside yaep.

- **The per parse caches in yaep are created too big.** At the start of every parse
  (`src/main/c/parts/yaep.c`, where `cache_stateset_cores` etc are created) the hash
  tables `cache_stateset_matched_lengths`, `cache_stateset_core_matched_lengths` and
  `cache_stateset_term_lookahead` get at least 20000, 20000 and 30000 entries, whatever
  the length of the input. For a line of 55 characters that is about 70 000 slots
  calloced and freed per parse: `create_hash_table` is 10% of the run time. Sizing them
  from the input length (the tables grow when needed), or clearing and reusing them
  between parses, should remove most of it.
- **The hash table lookups.** `find_hash_table_entry` is 20% of the run time, used for
  the state sets, the cores and the lookahead caches of the Earley parser. Cheaper hash
  functions, or fewer lookups (eg caching the last lookup), would help every ixml parse,
  not only `--lines`.
- **The grammar is read into yaep again on every parse** (`yaep_read_grammar` in
  `xmqParseBufferWithIXML`), with the yaep symbol tables, the terminal bitsets and the
  `map_core_symb_to_predcomps` caches (`yaep_cspc.c`) rebuilt from scratch. When no code
  point is new the grammar is unchanged and all of it could be kept. When a code point is
  new only its terminal and the rules of the charsets that contain it are added.
- **The translation from code point to terminal** (`symb_code_trans_vect`) is an array
  over the range of the code points, or a hash lookup when they are too far apart. A
  two level table (plane, then offset) mapping directly to the dense `term_id` would be
  fast and small for any input.
- **`used_unicodes` is 1 MB per grammar**, a byte per code point. A bitset (136 KB), or a
  hash set of the used code points, is enough.
- **The UTF-8 check and the scan read the input twice.** `ixml_reject_invalid_utf8` and
  `scan_content_fixup_charsets` could be one pass, but both are below 0.1% of the run
  time even for a single 166 KB parse, so this only matters as a cleanup.
