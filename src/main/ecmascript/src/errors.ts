/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT) */

/**
 * The parse errors of the xmq parser, the same codes as XMQStatus in the C xmq.h and
 * ParseErrorCode in Java, and the same messages as xmqParseErrorToString in C.
 */
export enum ParseErrorCode
{
    XMQ_ERROR_NONE = 0,
    XMQ_ERROR_QUOTE_NOT_CLOSED = 4,
    XMQ_ERROR_ENTITY_NOT_CLOSED = 5,
    XMQ_ERROR_COMMENT_NOT_CLOSED = 6,
    XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES = 7,
    XMQ_ERROR_BODY_NOT_CLOSED = 8,
    XMQ_ERROR_ATTRIBUTES_NOT_CLOSED = 9,
    XMQ_ERROR_COMPOUND_NOT_CLOSED = 10,
    XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN = 11,
    XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES = 12,
    XMQ_ERROR_UNEXPECTED_CLOSING_BRACE = 13,
    XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS = 14,
    XMQ_ERROR_UNEXPECTED_TAB = 15,
    XMQ_ERROR_INVALID_CHAR = 16,
    XMQ_ERROR_BAD_DOCTYPE = 17,
    XMQ_ERROR_PARSING_XML = 28,
    XMQ_ERROR_VALUE_CANNOT_START_WITH = 30,
    XMQ_ERROR_XML_FRAGMENT_NOT_CLOSED = 37,
    XMQ_ERROR_TOO_DEEP = 40,
    XMQ_WARNING_QUOTES_NEEDED = 100,
}

/** xmqParseErrorToString in C (src/main/c/parts/xmq_internals.c). */
export function parseErrorToString(code: ParseErrorCode): string
{
    switch (code)
    {
    case ParseErrorCode.XMQ_ERROR_NONE: return "no warning, no error";
    case ParseErrorCode.XMQ_ERROR_QUOTE_NOT_CLOSED: return "quote is not closed";
    case ParseErrorCode.XMQ_ERROR_ENTITY_NOT_CLOSED: return "entity is not closed";
    case ParseErrorCode.XMQ_ERROR_COMMENT_NOT_CLOSED: return "comment is not closed";
    case ParseErrorCode.XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES: return "comment closed with too many slashes";
    case ParseErrorCode.XMQ_ERROR_BODY_NOT_CLOSED: return "body is not closed";
    case ParseErrorCode.XMQ_ERROR_ATTRIBUTES_NOT_CLOSED: return "attributes are not closed";
    case ParseErrorCode.XMQ_ERROR_COMPOUND_NOT_CLOSED: return "compound is not closed";
    case ParseErrorCode.XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN: return "compound may only contain quotes and entities";
    case ParseErrorCode.XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES: return "quote closed with too many quotes";
    case ParseErrorCode.XMQ_ERROR_UNEXPECTED_CLOSING_BRACE: return "unexpected closing brace";
    case ParseErrorCode.XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS: return "expected content after equals";
    case ParseErrorCode.XMQ_ERROR_UNEXPECTED_TAB: return "invalid tab character found, remember that tab is not allowed as a field separator, to store tab as content it must be quoted";
    case ParseErrorCode.XMQ_ERROR_INVALID_CHAR: return "unexpected character";
    case ParseErrorCode.XMQ_ERROR_BAD_DOCTYPE: return "doctype could not be parsed";
    case ParseErrorCode.XMQ_ERROR_PARSING_XML: return "error parsing xml";
    case ParseErrorCode.XMQ_ERROR_VALUE_CANNOT_START_WITH: return "value cannot start with = /* or //";
    case ParseErrorCode.XMQ_ERROR_XML_FRAGMENT_NOT_CLOSED: return "embedded xml fragment is not closed";
    case ParseErrorCode.XMQ_ERROR_TOO_DEEP: return "nesting is too deep";
    case ParseErrorCode.XMQ_WARNING_QUOTES_NEEDED: return "perhaps you need more quotes to quote this quote";
    }
    return "unknown error";
}

/**
 * A parse error. The message is the same text as the C xmq prints
 * (generate_state_error_message in C): source:line:col: error: what, followed by the line
 * of the input and a ^ under the error. A warning that C prints before the error (perhaps
 * you need more quotes) is part of the message.
 */
export class XMQParseError extends Error
{
    /** The error code. */
    readonly code: ParseErrorCode;
    /** The line number, the first line is 1. */
    readonly line: number;
    /** The column in characters (code points), the first column is 1. */
    readonly column: number;
    /** Where the input came from, eg a file name, "stdin" or "-i argument". */
    readonly source: string;

    constructor(code: ParseErrorCode, message: string, line: number, column: number, source: string)
    {
        super(message);
        this.name = "XMQParseError";
        this.code = code;
        this.line = line;
        this.column = column;
        this.source = source;
    }
}

/** The number of bytes of the text in UTF-8, the C positions are in bytes. */
function utf8_length(s: string): number
{
    let n = 0;
    for (let i = 0; i < s.length; i++)
    {
        const c = s.charCodeAt(i);
        if (c < 0x80) n += 1;
        else if (c < 0x800) n += 2;
        else if (c >= 0xd800 && c <= 0xdbff && i + 1 < s.length) { n += 4; i++; }
        else n += 3;
    }
    return n;
}

/**
 * generate_state_error_message in C (src/main/c/parts/xmq_internals.c): the message of an
 * error at position pos (line, col) of the buffer. The line of the input is shown when the
 * position is inside the buffer, with the ^ after as many spaces as there are bytes before
 * the position in the line (like C). Lines longer than 1024 bytes are cut.
 */
export function generate_state_error_message(buffer: string, source: string, code: ParseErrorCode,
                                             pos: number, line: number, col: number): string
{
    const error = parseErrorToString(code);
    let line_length = 0;
    let indent = 0;
    let line_start = pos;
    while (line_start > 0 && buffer.charAt(line_start - 1) !== "\n" && line_length < 1024)
    {
        // Step back one code point, a surrogate pair is 4 bytes in UTF-8.
        let k = line_start - 1;
        const lo = buffer.charCodeAt(k);
        if (lo >= 0xdc00 && lo <= 0xdfff && k > 0 && buffer.charCodeAt(k - 1) >= 0xd800 && buffer.charCodeAt(k - 1) <= 0xdbff) k--;
        const n = utf8_length(buffer.substring(k, line_start));
        line_start = k;
        line_length += n;
        indent += n;
    }
    let line_stop = pos;
    while (line_stop < buffer.length && buffer.charAt(line_stop) !== "\0" && buffer.charAt(line_stop) !== "\n" && line_length < 1024)
    {
        const cp = buffer.codePointAt(line_stop) ?? 0;
        const w = cp > 0xffff ? 2 : 1;
        line_length += utf8_length(buffer.substring(line_stop, line_stop + w));
        line_stop += w;
    }

    let char_error = "";
    if (code === ParseErrorCode.XMQ_ERROR_INVALID_CHAR)
    {
        const cp = pos < buffer.length ? (buffer.codePointAt(pos) ?? 0) : 0;
        char_error = " \"" + (cp === 0 ? "" : String.fromCodePoint(cp)) + "\" U+" + cp.toString(16).toUpperCase();
    }

    let line_error = "";
    if (pos < buffer.length)
    {
        line_error = "\n" + buffer.substring(line_start, line_stop) + "\n " + " ".repeat(Math.max(0, indent - 1)) + "^";
        // C prints it into a buffer of 1024 bytes, a long line loses its end and the ^.
        line_error = cut_utf8(line_error, 1023);
    }
    const e_or_w = code >= ParseErrorCode.XMQ_WARNING_QUOTES_NEEDED ? "warning" : "error";
    // And the whole message into a buffer of 2048 bytes.
    return cut_utf8(source + ":" + line + ":" + col + ": " + e_or_w + ": " + error + char_error + line_error, 2047);
}

/** The longest start of s that is at most max bytes in UTF-8, like a C snprintf into max+1 bytes. */
function cut_utf8(s: string, max: number): string
{
    if (s.length * 3 <= max || utf8_length(s) <= max) return s;
    let n = 0;
    let i = 0;
    while (i < s.length)
    {
        const cp = s.codePointAt(i) ?? 0;
        const w = cp > 0xffff ? 2 : 1;
        const b = utf8_length(s.substring(i, i + w));
        if (n + b > max) break;
        n += b;
        i += w;
    }
    return s.substring(0, i);
}
