#!/usr/bin/env python3
# Regenerate the Unicode general category codepoint tables in
# src/main/c/parts/text.c from UCD DerivedGeneralCategory.txt.
#
# The tables are looked up with binary_search_int (see category_has_code in
# text.c), which requires the arrays to be sorted and duplicate-free.
#
# Usage: scripts/gen_unicode_categories.py [DerivedGeneralCategory.txt] [src/main/c/parts/text.c]
#
# Categories not present in DerivedGeneralCategory.txt are derived here:
#   Cs (surrogates)      U+D800..U+DFFF
#   Co (private use)     U+E000..U+F8FF, U+F0000..U+10FFFF
#   Cn (unassigned)      present in the UCD file as " General_Category=Unassigned"
# Compound categories (L, M, N, P, S, Z, C, LC) are kept as {-1} placeholders:
# they are resolved at runtime through unicode_lookup_category_parts().

import re
import sys

CATS = ["Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Mc", "Me",
        "Nd", "Nl", "No", "Pc", "Pd", "Pe", "Pf", "Pi", "Po",
        "Sc", "Sk", "Sm", "So", "Zs", "Zl", "Zp",
        "Cc", "Cf", "Cs", "Co", "Cn"]

NONCHARS = set(range(0xFDD0, 0xFDE0)).union(set(range(0xFFD0, 0xFFE0)))


def noncharacters(max_cp):
    out = set()
    base = 0
    while base <= max_cp:
        for n in range(0x100):
            cp = base + 0xFF0 + n
            if cp <= max_cp:
                out.add(cp)
        base += 0x10000
    return out


def parse_derived(path):
    cats = {c: set() for c in CATS}
    max_cp = 0
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            left, rest = line.split(";", 1)
            cat = rest.split("#", 1)[0].strip()
            if cat not in cats:
                continue
            for rng in left.split():
                a, b = (rng.split("..", 1) + [None])[:2]
                a = int(a, 16)
                b = int(b, 16) if b else a
                if b > max_cp:
                    max_cp = b
                cats[cat].update(range(a, b + 1))
    return cats, max_cp


def add_category(cats, name, rngs):
    s = cats.setdefault(name, set())
    for a, b in rngs:
        s.update(range(a, b + 1))


def format_array(values):
    # Emit as a flat, sorted, duplicate-free list (required for
    # binary search / linear scan in text.c).
    items = [f"0x{v:04x}" if v <= 0xFFFF else f"0x{v:05x}" for v in sorted(set(values))]
    return " { " + ", ".join(items) + " };"


def main():
    dgc = sys.argv[1] if len(sys.argv) > 1 else "DerivedGeneralCategory.txt"
    textc = sys.argv[2] if len(sys.argv) > 2 else "src/main/c/parts/text.c"

    cats, max_cp = parse_derived(dgc)
    add_category(cats, "Cs", [(0xD800, 0xDFFF)])
    add_category(cats, "Co", [(0xE000, 0xF8FF), (0xF0000, 0x10FFFF)])
    add_category(cats, "Cn", sorted(noncharacters(0x10FFFF) - set().union(*cats.values())))

    src = open(textc, encoding="utf-8").read()
    crlf = "\r\n" in src
    nl = "\r\n" if crlf else "\n"
    changed = 0
    for cat in CATS:
        new_arr = format_array(cats[cat])
        # Replace the body of: int unicode_<cat>[] = ... ;  (not the size_t len line)
        pat = re.compile(r"(int unicode_" + cat + r"\[\] = )[^;]*;")
        src, n = pat.subn(lambda m: m.group(1) + new_arr, src, count=1)
        if n != 1:
            sys.exit(f"ERROR: could not find `int unicode_{cat}[]` in {textc}")
        changed += 1

    open(textc, "w", encoding="utf-8").write(src)
    print(f"Updated {changed} category tables in {textc} from {dgc}")
    for cat in CATS:
        print(f"  {cat:3s}: {len(cats[cat]):6d} codepoints")


if __name__ == "__main__":
    main()
