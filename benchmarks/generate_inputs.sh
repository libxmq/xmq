#!/bin/sh
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Generate the input files for the benchmarks into a directory (default build/benchmarks).
# The content is made with the Park-Miller random generator, whose products stay below
# 2^53 so they are exact in awk, the files are identical on every computer (awk rand()
# differs between awk implementations).
#
# Usage: benchmarks/generate_inputs.sh [dir]

DIR=${1:-build/benchmarks}
mkdir -p "$DIR"
export LC_ALL=C

# words.txt: 3000 lines of 10 words, some with non-ascii letters.
awk 'BEGIN {
    split("hej världen über naïve abc déjà 1234 ok. x,y", w, " ");
    seed = 1;
    for (l = 0; l < 3000; l++) {
        line = "";
        for (i = 0; i < 10; i++) {
            seed = (seed * 16807) % 2147483647;
            line = line (i ? " " : "") w[1 + int(seed / 65536) % 9];
        }
        printf "%s%s", (l ? "\n" : ""), line;
    }
}' > "$DIR/words.txt"

# items.xml and items.xmq: 20000 items with attributes, numbers and text.
awk -v xmq="$DIR/items.xmq" 'BEGIN {
    split("alfa beta gamma delta epsilon zeta eta theta", n, " ");
    seed = 7;
    print "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    print "<items>";
    print "items {" > xmq;
    for (i = 0; i < 20000; i++) {
        seed = (seed * 16807) % 2147483647;
        name = n[1 + int(seed / 65536) % 8];
        price = int(seed / 4096) % 100000 / 100;
        printf "  <item id=\"%d\" name=\"%s\"><price>%.2f</price><text>The %s item number %d &amp; more</text></item>\n", i, name, price, name, i;
        printf "    item(id = %d name = %s) {\n        price = %.2f\n        text = \x27The %s item number %d & more\x27\n    }\n", i, name, price, name, i > xmq;
    }
    print "</items>";
    print "}" > xmq;
}' > "$DIR/items.xml"
