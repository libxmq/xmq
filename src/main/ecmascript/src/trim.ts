/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

/**
 * Removing the incidental indentation of quotes and comments: a port of the Java
 * XmqTrim (itself an exact port of xmq_un_quote, xmq_un_comment, xmq_trim_quote and
 * their helpers in the C xmq.c and text.c, rules r6-r10 in doc/xmq.tex) and of
 * UtilParseQuote.findQuoteStartStop. The trimming depends only on the content, not on
 * the column where the quote starts.
 */

import { is_xml_whitespace } from "./text.js";

/** In C the incidental indentation is a size_t where -1 is the largest value. */
const SIZE_MAX = Number.MAX_SAFE_INTEGER;

/**
 * XmqTrim.unQuote (C xmq_un_quote): optionally remove the surrounding quotes and then
 * remove the incidental indentation.
 */
export function unQuote(b: string, start: number, stop: number, remove_qs: boolean, is_xmq: boolean): string
{
    let j = 0;
    if (remove_qs && start < stop)
    {
        const q = b.charAt(start);
        if (q === "'" || q === "\"")
        {
            while (start + j < stop - j && b.charAt(start + j) === q && b.charAt(stop - j - 1) === q) j++;
        }
    }
    return trimQuote(b, start + j, stop - j, is_xmq, false);
}

/**
 * XmqTrim.unComment (C xmq_un_comment): remove the comment markers, the incidental
 * indentation and trailing spaces.
 */
export function unComment(b: string, start: number, stop: number): string
{
    let i = start;
    while (i < stop && b.charAt(i) === "/") i++;
    if (i === stop || b.charAt(i) !== "*")
    {
        // A single line comment //.....
        i = start + 2;
        if (i < stop && b.charAt(i) === " ") i++;
        while (i < stop && b.charAt(stop - 1) === " ") stop--;
        if (i > stop) i = stop;
        return trimQuote(b, i, stop, true, true);
    }
    while (i < stop && b.charAt(i) === "/") i++;
    if (i === start)
    {
        // The asterisk is first, a comment continuation. Remove the ending slashes.
        while (stop > start && b.charAt(stop - 1) === "/") stop--;
    }
    else
    {
        // A standard /* */ comment, remove the surrounding slashes.
        let j = 0;
        while (start + j < stop - j && b.charAt(start + j) === "/" && b.charAt(stop - j - 1) === "/") j++;
        start += j;
        stop -= j;
    }
    // Skip the asterisks.
    start++;
    stop--;
    // Skip a single space after the starting asterisk and before the ending asterisk.
    if (start < stop && b.charAt(start) === " ") start++;
    if (stop - 1 >= start && b.charAt(stop - 1) === " ") stop--;
    if (start > stop) start = stop;
    return trimQuote(b, start, stop, true, true);
}

/** XmqTrim.check_leading_space_nl */
function check_leading_space_nl(b: string, start: number, stop: number): boolean
{
    while (start < stop && b.charAt(start) === " ") start++;
    if (start === stop) return false;
    return b.charAt(start) === "\n";
}

/**
 * XmqTrim.calculate_incidental_indent: the smallest indentation of the lines after the
 * first (with content), -1 if there is no newline.
 */
function calculate_incidental_indent(b: string, start: number, stop: number): number
{
    let indent = SIZE_MAX;
    let i = start;
    while (i < stop && b.charAt(i) !== "\n") i++;
    if (i >= stop) return -1;
    for (;;)
    {
        i++;
        const line_start = i;
        while (i < stop && b.charAt(i) === " ") i++;
        if (i >= stop) break;
        if (b.charAt(i) !== "\n" && b.charAt(i) !== "\r")
        {
            const ind = i - line_start;
            if (ind < indent) indent = ind;
        }
        while (i < stop && b.charAt(i) !== "\n") i++;
        if (i >= stop) break;
    }
    // In C both no newline and no line with content are (size_t)-1, then nothing is
    // trimmed.
    return indent === SIZE_MAX ? -1 : indent;
}

/** XmqTrim.has_leading_space_nl (has_leading_space_nl in text.c): a newline before the content. */
function has_leading_space_nl(b: string, start: number, stop: number): boolean
{
    let i = start;
    let found_nl = false;
    while (i < stop && b.charAt(i) === "\n")
    {
        i++;
        found_nl = true;
    }
    while (i < stop)
    {
        const c = b.charAt(i);
        if (c === "\n") found_nl = true;
        if (!is_xml_whitespace(c)) break;
        i++;
    }
    return found_nl;
}

/** XmqTrim.has_ending_nl_space (has_ending_nl_space in text.c): a newline after the content. */
function has_ending_nl_space(b: string, start: number, stop: number): boolean
{
    let i = stop - 1;
    let found_nl = false;
    while (i >= start && b.charAt(i) === "\n")
    {
        i--;
        found_nl = true;
    }
    while (i >= start)
    {
        const c = b.charAt(i);
        if (c === "\n") found_nl = true;
        if (!is_xml_whitespace(c)) break;
        i--;
    }
    return found_nl;
}

/** XmqTrim.find_line (C find_line): has a newline, after the last non space, end of line. */
function find_line(b: string, start: number, stop: number): [boolean, number, number]
{
    let has_nl = false;
    let lnws = start;
    let i = start;
    while (i < stop && (b.charAt(i) === " " || b.charAt(i) === "\t")) i++;
    while (i < stop)
    {
        const c = b.charAt(i);
        if (c === "\n" || c === "\r")
        {
            if (c === "\r" && i + 1 < b.length && b.charAt(i + 1) === "\n") i++;
            i++;
            has_nl = true;
            break;
        }
        if (c !== " " && c !== "\t") lnws = i + 1;
        i++;
    }
    return [has_nl, lnws, i];
}

/**
 * XmqTrim.trimQuote (C xmq_trim_quote): remove the incidental indentation of a quote or
 * comment.
 * @param b The buffer.
 * @param start Start of the content.
 * @param stop End of the content.
 * @param is_xmq The last line indentation counts.
 * @param is_comment The content is a comment.
 */
export function trimQuote(b: string, start: number, stop: number, is_xmq: boolean, is_comment: boolean): string
{
    let append_newlines = 0;
    let last_line_spaces = -1;

    if (has_ending_nl_space(b, start, stop))
    {
        while (stop > start)
        {
            const c = b.charAt(stop - 1);
            if (c === "\n") append_newlines++;
            if (c === " " && append_newlines === 0)
            {
                if (last_line_spaces === -1) last_line_spaces = 0;
                last_line_spaces++;
            }
            if (c !== " " && c !== "\t" && c !== "\n" && c !== "\r") break;
            stop--;
        }
    }
    // If there are ending newlines, one is dropped, the rest are kept.
    if (append_newlines > 0) append_newlines--;

    if (stop === start) return "\n".repeat(append_newlines);

    const leads_space_nl = check_leading_space_nl(b, start, stop);

    let incidental = calculate_incidental_indent(b, start, stop);
    const incidental_cmp = incidental === -1 ? SIZE_MAX : incidental;
    const last_cmp = last_line_spaces === -1 ? SIZE_MAX : last_line_spaces;
    if (is_xmq && last_cmp < incidental_cmp)
    {
        incidental = last_line_spaces;
        if (!leads_space_nl)
        {
            incidental++;
            if (is_comment) incidental += 2;
        }
    }

    if (incidental === -1)
    {
        // No newline was found, do not trim, but re-add the ending newlines.
        return b.substring(start, stop) + "\n".repeat(append_newlines);
    }

    let prepend_newlines = 0;
    let first_line = true;
    let line = find_line(b, start, stop);

    if (has_leading_space_nl(b, start, stop))
    {
        // There is no first line, start removing incidental indentation immediately.
        first_line = false;
        start = line[2];
        let i = start;
        while (i < stop)
        {
            const c = b.charAt(i);
            if (c === "\n")
            {
                start = i + 1;
                prepend_newlines++;
            }
            else if (c !== "\r") break;
            i++;
        }
    }

    let o = "\n".repeat(prepend_newlines);
    let i = start;
    while (i < stop)
    {
        line = find_line(b, i, stop);
        const has_nl = line[0];
        const after_last_non_space = line[1];
        const eol = line[2];
        if (first_line)
        {
            first_line = false;
        }
        else if (b.charAt(i) === " ")
        {
            // Skip the incidental indentation, empty lines do not count.
            let n = incidental;
            while (n > 0 && i < stop && b.charAt(i) === " ")
            {
                i++;
                n--;
            }
        }
        // Copy the content, but not line ending whitespace.
        if (i < after_last_non_space) o += b.substring(i, after_last_non_space);
        if (after_last_non_space > i) i = after_last_non_space;
        if (has_nl)
        {
            o += "\n";
        }
        else
        {
            // The final line has no newline, copy any ending spaces as well.
            if (i < eol) o += b.substring(i, eol);
        }
        i = eol;
    }
    o += "\n".repeat(append_newlines);
    return o;
}

/**
 * UtilParseQuote.findQuoteStartStop: the start and stop of the content of an xmq quote,
 * without the surrounding single or double quotes, eg '' gives (1,1).
 */
export function findQuoteStartStop(b: string, start: number, stop: number): [number, number]
{
    let i = start;
    let j = stop - 1;
    const qc = b.charAt(i);
    while (b.charAt(i) === qc && b.charAt(j) === qc && i < j)
    {
        i++;
        j--;
    }
    return [i, j + 1];
}

/** UtilParseQuote.trimQuote: the content of a quote trimmed with the xmq rules. */
export function trimQuoteContent(b: string, start: number, stop: number): string
{
    return trimQuote(b, start, stop, true, false);
}
