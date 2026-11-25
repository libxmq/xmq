/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

/**
 * Small text helpers, ported from the Java Util and Text classes.
 */

/** Java Util.xmq_quote_as_c (C xmq_quote_as_c): the text as the contents of a C string. */
export function xmq_quote_as_c(s: string, start: number, stop: number, add_quotes: boolean): string
{
    if (start === -1 && stop === -1)
    {
        start = 0;
        stop = s.length;
    }
    if (start === stop)
    {
        // The empty string.
        return add_quotes ? "\"\"" : "";
    }
    if (stop > s.length) stop = s.length;
    let out = add_quotes ? "\"" : "";
    for (let i = start; i < stop; ++i)
    {
        const c = s.charAt(i);
        if (c === "\\") out += "\\\\";
        else if (c === "\"") out += "\\\"";
        else if (c === "\n") out += "\\n";
        else if (c === "\t") out += "\\t";
        else if (c === "\r") out += "\\r";
        else out += c;
    }
    if (add_quotes) out += "\"";
    return out;
}

/** Java Text.is_lowercase_hex: a character that can occur in a lowercase hex number. */
export function is_lowercase_hex(c: string): boolean
{
    return (c >= "0" && c <= "9") || (c >= "a" && c <= "f");
}

/** Java XmqTrim.is_xml_whitespace. */
export function is_xml_whitespace(c: string): boolean
{
    return c === " " || c === "\n" || c === "\t" || c === "\r";
}
