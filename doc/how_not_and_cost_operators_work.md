# How the not operator and the cost operator work in xmq's ixml engine

xmq extends ixml with two operators that are not in the ixml standard:

* `!factor`, the not operator (negative lookahead): the alternative continues only if
  the input ahead does not match the factor. It consumes no input.
* `=<`, the cost operator: `name =< alts.` gives the rule a cost of 1, each extra `<`
  adds 1 (`=<<` costs 2). When the input is ambiguous, the tree with the lowest total
  cost is chosen.

`library/core/ixmlnotcost.ixml` parses grammars that use them, it gives a `not` element
for `!` and a `cost` attribute for `=<`.

## The not operator `!name`

When the grammar is read, `!name` is turned into an empty marker rule. While parsing, the
engine checks the input ahead against the alternatives of the rule `name`, without
consuming anything. There are three stages.

### 1. Reading the grammar (`src/main/c/parts/ixml.c`)

`parse_ixml_not` reads the `!`. If a name follows, `add_not_rule_nonterminal` builds the
name `|!N` + rule name, eg `|!Nkw_else`. It then:

* creates an empty rule with that name (no right hand side),
* adds it as a nonterminal and puts it where the `!kw_else` stood,
* records it in `ixml_non_terminals_map`, so every `!kw_else` in the grammar shares one
  `|!Nkw_else`.

The grammar of `tests/ixml_not_035_not_rule_sequence.test` therefore ends up internally as:

```
if   = -'if ', cond, stmt, (-'else ', stmt | |!Nkw_else).
call = |!Nkw_else, ['a'-'z']+, -';'.
|!Nkw_else = .                 { empty, only a marker }
kw_else = 'else', ~['a'-'z'].  { unchanged, also usable as a normal rule }
```

Strings and charsets work the same way with other prefixes: `!'in'` becomes `|!Sin` and
`![L]` becomes `|![L]`.

### 2. Checking the rule (`check_not_rule_nonterminals` in `ixml.c`)

Once the whole grammar is read, each `|!N...` rule is checked. The rule after the `!` must
be defined, and `not_rule_kind` decides whether it is allowed:

* **kind 1:** every alternative is exactly one character: a character, a charset, or a
  call of a kind 1 rule. Eg `namechar` in test 032: `-namechar = ['a'-'z'; '0'-'9'; '-'].`
* **kind 2:** a single alternative that is a fixed sequence of characters: strings,
  charsets, or calls of kind 1 or 2 rules. Eg `-kw_else = 'else', ~['a'-'z'].`
* **kind 3:** several alternatives, each a fixed sequence. Eg `wordish` in test 033,
  `-wordish = letter | digit | '_'.`, or a keyword list `-kw = 'if', ~[L] | 'else', ~[L].`

Repetitions, options and empty alternatives are rejected with "the rule X after not !
must match fixed sequences of characters". Without them the lookahead has a fixed length
and needs no real parse.

A kind 3 rule is only allowed directly after the `!`, not called from inside a sequence.
The matcher tries alternatives only at the top level. Inside a sequence it takes the first
alternative of a called rule that matches and does not go back to try another.

### 3. Parsing (`src/main/c/parts/yaep.c`)

To the Earley parser, `|!Nkw_else` looks like an empty nonterminal flagged
`is_not_operator`. Normally the parser steps over an empty symbol for free. For a not
operator it first calls `blocked_by_lookahead`, which calls `has_lookahead`:

* `has_lookahead` strips the `|!N` and finds the real `kw_else` symbol with
  `symb_find_by_repr`.
* `match_not_rule` then matches it against the input from the current position: a
  terminal must equal the next input character, a charset is tested against its first
  set, a rule tries each alternative in turn, walking its terms one input position at a
  time.
* If any alternative matches, the lookahead exists. The marker is blocked and that dotted
  rule dies.
* If none matches, the parser moves the dot past the marker without consuming input.

`nullable_here` covers a rule that becomes empty only through a not operator, eg
`G = 'a' | !'x'.` is empty unless the next character is `x`.

### Example: test 035

Input: `if a if b x;else y;elsewhere;`

* After `x;` the inner `if` could take the "no else" branch `|!Nkw_else`. The input ahead
  is `else ` and the space is in `~['a'-'z']`, so `kw_else` matches. That branch is
  blocked, the inner `if` must take `-'else ', stmt`, and the dangling else binds to the
  closest `if`.
* The outer `if`'s "no else" branch sees `elsewhere`. `'else'` matches but `w` is not in
  `~['a'-'z']`, so `kw_else` fails and the branch goes through.
* `call` on `elsewhere` makes the same check, so `call = elsewhere` is allowed even
  though the word starts with "else".

The lookahead sees `kw_else`'s raw characters, so any `-` marks on its parts make no
difference. `kw_else` itself never appears in the output, because the marker rule is empty.

## The cost operator `=<`

A cost does not change which inputs the grammar accepts, it only chooses between the
trees of an ambiguous parse. The cost of a tree is the sum of the costs of all the rules
used in it, a rule without `<` costs 0. The tree with the lowest cost is chosen, if
several trees share the lowest cost the result is still ambiguous.

### 1. Reading the grammar (`ixml.c`)

After the `=` (or `:`) of a rule, `parse_ixml_rule` counts the `<` into
`IXMLRule.cost` and sets `ixml_costs_enabled`. The cost applies to all the alternatives of
the rule: yaep stores each alternative as a rule of its own, and `parse_ixml_alts` copies
the cost to them, so in `e =< 'q' | 'x'.` both alternatives cost 1 (test 006).
`ixml_to_yaep_read_rule` hands the cost to yaep as the cost of the rule's abstract node
(`anode_cost`).

### 2. Parsing (`xmqParseBufferWithIXML` in `xmq.c`)

When the grammar has a cost, xmq turns off yaep's one parse flag and turns on its cost
flag. The parse then keeps all the parses, as a parse forest: a DAG where an `ALT` node
lists the different trees for the same part of the input, and a node used by several
trees is shared. This is needed since the cheapest tree can only be known once all of
them have been built, and it makes a parse with costs slower on a very ambiguous grammar.

### 3. Choosing the tree (`yaep_tree.c`)

If the parse is ambiguous, `find_minimal_translation` calls `prune_to_minimal`, which
walks the forest bottom up:

* a terminal costs 0,
* a rule node costs its own `anode_cost` plus the costs of its children. A shared node is
  computed once and its cost is remembered, so a later parent gets the same cost,
* an `ALT` node keeps the alternative with the lowest cost and drops the others. With
  equal lowest costs it keeps all of them.

`traverse_pruned_translation` then decides if the pruned tree is still ambiguous: only if
an `ALT` node with several alternatives remains.

The same input position can be used by alternatives with different costs. Since the
losing alternative is dropped, nodes must not be shared between alternatives if they
differ: a terminal deleted with `-` by one rule and kept by another gets a node of its own
(test 008), and so do the marks of a shared nonterminal (test 009).

### Examples

```
p = d | e.
d = ID.
e =< ID.
ID = 'T'.
```

The input `T` is both a `d` and an `e`, `d` costs 0 and wins: `p { d { ID = T } }`
(test 001). With `d =< ID.` and `e =< ID.` both cost 1, the result is ambiguous (test 004).
With `d =<< ID.`, `e =< ID.` and `f =<< ID.`, `e` is the cheapest (test 005).

The costs add up over the tree:

```
p = a | b.
a = c, c.
c =< 'x'.
b =<< 'xx'.
```

For the input `xx` the tree through `a` costs 1 + 1 = 2 and the tree through `b` costs 2,
a tie, so the result is ambiguous. With `b =<<< 'xx'.` the tree through `a` wins.

Test 003 shows a typical use, the C problem whether `T*p` is a declaration or an
expression. The rule for the expression has a cost, so the declaration is chosen when
both parse:

```
p = d | e.
d = t, '*', ID.
t = ID.
e =< x.
x = ID | x, '*', ID.
ID = 'T' | 'p'.
```

### The Java engines

The Java Earley engine (`TreeBuilder.java`) chooses in the same way: the cost of a tree is
the sum of the costs of its rules, the lowest cost wins and equal lowest costs are
ambiguous. The LL(*) parser generator (`IXMLLookahead.java`) carries the cost of the rules
entered in each lookahead configuration, and when two alternatives reach the same
position with the same stack it drops the more expensive one.
