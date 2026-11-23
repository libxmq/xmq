import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.TreeSet;

/**
 * Generate the unicode category tables in the C library (src/main/c/parts/text.c)
 * from the Unicode DerivedGeneralCategory data file.
 *
 * <p>Java port of the former scripts/gen_unicode_categories.py.</p>
 *
 * <p>Each table is the sorted list of the ranges of code points of the category, as
 * {@code {from,to}} pairs with from and to inclusive, adjacent code points are merged into
 * one range. The tables are looked up with a binary search (see category_has_code in text.c),
 * which requires the ranges to be sorted and not overlapping.</p>
 *
 * <p>Categories not present in DerivedGeneralCategory.txt are derived here:
 * Cs (surrogates) U+D800..U+DFFF, Co (private use) U+E000..U+F8FF and
 * U+F0000..U+10FFFF, and Cn (unassigned) from the noncharacters not
 * assigned to any other category. Compound categories (L, M, N, P, S, Z, C, LC)
 * are kept as {@code { {-1,-1} }} placeholders: they are resolved at runtime through
 * unicode_lookup_category_parts().</p>
 *
 * <p>Usage: GenUnicodeCategories [DerivedGeneralCategory.txt] [src/main/c/parts/text.c]</p>
 */
public class GenUnicodeCategories
{

    /**
     * All two-letter categories that get their own C array in text.c.
     */
    private static final String[] CATEGORIES =
        {
            "Lu", "Ll", "Lt", "Lm", "Lo",
            "Mn", "Mc", "Me",
            "Nd", "Nl", "No",
            "Pc", "Pd", "Ps", "Pe", "Pf", "Pi", "Po",
            "Sc", "Sk", "Sm", "So",
            "Zs", "Zl", "Zp",
            "Cc", "Cf", "Cs", "Co", "Cn"
        };

    /**
     * Runs the generator.
     * @param args Optional: [input DerivedGeneralCategory.txt] [output src/main/c/parts/text.c].
     * @throws IOException In case of I/O error.
     * @throws InterruptedException Not expected (unused).
     */
    public static void main(String[] args) throws IOException, InterruptedException
    {
        Path input = args.length > 0 ? Path.of(args[0]) : Path.of("DerivedGeneralCategory.txt");
        Path output = args.length > 1 ? Path.of(args[1]) : Path.of("src/main/c/parts/text.c");

        run(input, output);
    }

    /**
     * Parses the Unicode DerivedGeneralCategory.txt file into per-category code-point sets.
     * @param path Path to the DerivedGeneralCategory.txt file.
     * @return A map from category name to (sorted) code point set, for all two-letter categories.
     * @throws IOException In case of I/O error.
     */
    public static Map<String, TreeSet<Integer>> parseDerived(Path path) throws IOException
    {
        Map<String, TreeSet<Integer>> cats = new LinkedHashMap<>();
        for (String cat : CATEGORIES)
            cats.put(cat, new TreeSet<>());

        for (String line : Files.readAllLines(path, StandardCharsets.UTF_8))
        {
            if (line.isBlank() || line.startsWith("#"))
                continue;
            int si = line.indexOf(';');
            if (si < 0)
                continue;
            String left = line.substring(0, si);
            String right = line.substring(si + 1);
            int hi = right.indexOf('#');
            if (hi >= 0)
                right = right.substring(0, hi);
            String name = right.trim();
            if (!cats.containsKey(name))
                continue;
            for (String tok : left.split("\\s+"))
            {
                if (tok.isEmpty())
                    continue;
                int dot = tok.indexOf("..");
                if (dot < 0)
                {
                    cats.get(name).add(parseInt(tok));
                }
                else
                {
                    int a = parseInt(tok.substring(0, dot));
                    int b = parseInt(tok.substring(dot + 2));
                    for (int cp = a; cp <= b; cp++)
                        cats.get(name).add(cp);
                }
            }
        }
        return cats;
    }

    /**
     * Parses a hexadecimal code point (no 0x prefix).
     * @param s Hex string like "0041" or "10ABCD".
     * @return The parsed integer value.
     */
    public static int parseInt(String s)
    {
        return Integer.parseInt(s, 16);
    }

    /**
     * Computes the noncharacters of all planes up to and including the plane
     * containing max_cp (U+FF0..U+FFF of each plane).
     * @param max_cp Highest code point to consider.
     * @return Set of noncharacters.
     */
    public static TreeSet<Integer> noncharacters(int max_cp)
    {
        TreeSet<Integer> nc = new TreeSet<>();
        for (int base = 0; base <= max_cp; base += 0x10000)
        {
            for (int n = 0; n < 0x100; n++)
            {
                int cp = base + 0xFF0 + n;
                if (cp <= max_cp)
                    nc.add(cp);
            }
        }
        return nc;
    }

    /**
     * Adds the derived categories: Cs (surrogates), Co (private use), and Cn (unassigned).
     * @param cats The category map to update in place.
     */
    public static void addDerived(Map<String, TreeSet<Integer>> cats)
    {
        for (int cp = 0xD800; cp <= 0xDFFF; cp++)
            cats.get("Cs").add(cp);

        for (int cp = 0xE000; cp <= 0xF8FF; cp++)
            cats.get("Co").add(cp);
        for (int cp = 0xF0000; cp <= 0x10FFFF; cp++)
            cats.get("Co").add(cp);

        TreeSet<Integer> all = new TreeSet<>();
        for (TreeSet<Integer> set : cats.values())
            all.addAll(set);

        for (int cp : noncharacters(0x10FFFF))
        {
            if (!all.contains(cp))
                cats.get("Cn").add(cp);
        }
    }

    /**
     * Formats a code point as hex, eg 0x0041 or 0x1f600.
     * @param v The code point.
     * @return The hex text.
     */
    public static String hex(int v)
    {
        return v <= 0xFFFF ? String.format("0x%04x", v) : String.format("0x%05x", v);
    }

    /**
     * Formats a set of code points as a C array initializer of sorted ranges, eight per line,
     * e.g. {@code {\n    {0x0041,0x005a}, {0x00c0,0x00d6}\n};}
     * @param values The code points.
     * @return The C array initializer text.
     */
    public static String formatRanges(Collection<Integer> values)
    {
        List<String> ranges = new ArrayList<>();
        int from = -1;
        int to = -1;
        for (int v : new TreeSet<>(values))
        {
            if (from >= 0 && v == to + 1)
            {
                to = v;
                continue;
            }
            if (from >= 0)
                ranges.add("{" + hex(from) + "," + hex(to) + "}");
            from = to = v;
        }
        if (from >= 0)
            ranges.add("{" + hex(from) + "," + hex(to) + "}");

        StringBuilder sb = new StringBuilder("{\n");
        for (int i = 0; i < ranges.size(); i += 8)
        {
            sb.append("    ").append(String.join(", ", ranges.subList(i, Math.min(i + 8, ranges.size()))));
            sb.append(i + 8 < ranges.size() ? ",\n" : "\n");
        }
        return sb.append("};").toString();
    }

    /**
     * Replaces the C arrays in text.c source for all categories.
     * @param cats The category map (values as code point sets).
     * @param source The full C source of text.c.
     * @return The updated C source.
     * @throws RuntimeException In case a category array is missing.
     */
    public static String generate(Map<String, TreeSet<Integer>> cats, String source)
    {
        for (String cat : CATEGORIES)
        {
            String label = "UnicodeCategoryRange unicode_" + cat + "[] = ";
            int idx = source.indexOf(label);
            if (idx < 0)
                throw new RuntimeException("ERROR: could not find `UnicodeCategoryRange unicode_" + cat + "[]`");
            int end = source.indexOf("};", idx);
            if (end < 0)
                throw new RuntimeException("ERROR: could not find end of `UnicodeCategoryRange unicode_" + cat + "[]`");
            end++;
            String newArr = formatRanges(cats.get(cat));
            source = source.substring(0, idx + label.length()) + newArr + source.substring(end + 1);
            System.out.println(String.format("  %3s: %6d codepoints", cat, cats.get(cat).size()));
        }
        return source;
    }

    /**
     * Runs the full pipeline: parse the data file, add the derived categories,
     * and rewrite the arrays in the C source file.
     * @param input Path to DerivedGeneralCategory.txt.
     * @param output Path to src/main/c/parts/text.c.
     * @return The updated C source text.
     * @throws IOException In case of I/O error.
     */
    public static String run(Path input, Path output) throws IOException
    {
        Map<String, TreeSet<Integer>> cats = parseDerived(input);
        addDerived(cats);

        String source = Files.readString(output, StandardCharsets.UTF_8);
        System.out.println("Updated " + CATEGORIES.length + " category tables in " + output + " from " + input);
        source = generate(cats, source);

        Files.writeString(output, source, StandardCharsets.UTF_8);
        return source;
    }

}
