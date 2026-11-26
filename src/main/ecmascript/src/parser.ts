/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT) */

/**
 * The xmq tokenizer/parser, a port of the Java XMQParser (with XMQParseCallbacks), which
 * itself is a port of the C xmq_parser.c. The parser calls a do_ method for every token;
 * XMQParseIntoTokens prints them and XMQParseIntoDOM builds the document.
 */

import { generate_state_error_message, ParseErrorCode, XMQParseError } from "./errors.js";
import { is_lowercase_hex, is_xml_whitespace } from "./text.js";
import { unComment } from "./trim.js";

/** XMQLevel: where a quote, entity or text value occurs. */
export enum XMQLevel
{
    LEVEL_XMQ,
    LEVEL_ELEMENT_VALUE,
    LEVEL_ELEMENT_VALUE_COMPOUND,
    LEVEL_ATTR_VALUE,
    LEVEL_ATTR_VALUE_COMPOUND,
}

/** Util.enter_compound_level: the level of the quotes and entities inside a compound. */
function enter_compound_level(l: XMQLevel): XMQLevel
{
    if (l === XMQLevel.LEVEL_ELEMENT_VALUE) return XMQLevel.LEVEL_ELEMENT_VALUE_COMPOUND;
    if (l === XMQLevel.LEVEL_ATTR_VALUE) return XMQLevel.LEVEL_ATTR_VALUE_COMPOUND;
    throw new Error("internal error: enter_compound_level " + l);
}

/** XMQParser.is_xmq_token_whitespace */
function is_xmq_token_whitespace(c: string): boolean
{
    return c === " " || c === "\n" || c === "\r";
}

/** XMQParser.is_xmq_text_name */
function is_xmq_text_name(c: string): boolean
{
    if (c >= "a" && c <= "z") return true;
    if (c >= "A" && c <= "Z") return true;
    if (c >= "0" && c <= "9") return true;
    return c === "-" || c === "_" || c === "." || c === ":" || c === "#";
}

/** XMQParser.is_xmq_attribute_key_start */
function is_xmq_attribute_key_start(c: string): boolean
{
    return !(c === "'" || c === "\"" || c === "(" || c === ")" || c === "{" || c === "}" ||
             c === "/" || c === "=" || c === "&");
}

/** XMQParser.unsafe_value_start: a value cannot start with these two characters. */
function unsafe_value_start(c: string, cc: string): boolean
{
    return c === "&" || c === "=" || (c === "/" && (cc === "/" || cc === "*"));
}

/** XMQParser.is_xmq_element_start */
function is_xmq_element_start(c: string): boolean
{
    return (c >= "a" && c <= "z") || (c >= "A" && c <= "Z") || c === "_";
}

/** XMQParser.is_xmq_comment_start */
function is_xmq_comment_start(c: string, cc: string): boolean
{
    return c === "/" && (cc === "/" || cc === "*");
}

/**
 * The base class of the xmq parsers (Java XMQParser). A subclass implements the token
 * callbacks. The callbacks get the line and column where the token starts, its start and
 * stop index in the buffer and the stop of its suffix.
 */
export abstract class XMQParser
{
    /**
     * The parsers recurse for every nested xmq body, deeper nesting is an error
     * (XMQ_ERROR_TOO_DEEP) instead of a stack overflow. Like XMQ_MAX_NESTING_DEPTH in C.
     */
    static readonly MAX_NESTING_DEPTH = 10000;

    /** The depth of the xmq bodies being parsed. */
    protected nesting_depth_ = 0;

    protected source_ = ""; // Used for generating error messages.
    protected buffer_ = ""; // The xmq source to parse.
    protected buffer_len_ = 0;

    // These are only updated using the increment function.
    protected i_ = 0; // Current parsing position.
    protected line_start_ = 0; // Position after the most recent \n found.
    protected line_ = 1; // Current line.
    protected col_ = 1; // Current col.

    protected last_body_start_ = 0;
    protected last_body_start_line_ = 0;
    protected last_body_start_col_ = 0;

    protected last_attr_start_ = 0;
    protected last_attr_start_line_ = 0;
    protected last_attr_start_col_ = 0;

    protected last_quote_start_ = 0;
    protected last_quote_start_line_ = 0;
    protected last_quote_start_col_ = 0;

    protected last_compound_start_ = 0;
    protected last_compound_start_line_ = 0;
    protected last_compound_start_col_ = 0;

    protected last_comment_start_ = 0;
    protected last_comment_start_line_ = 0;
    protected last_comment_start_col_ = 0;

    protected last_equals_start_ = 0;
    protected last_equals_start_line_ = 0;
    protected last_equals_start_col_ = 0;

    // A quote that perhaps ended too early, eg 'There's a man.', -1 if none (from C).
    protected last_suspicios_quote_end_ = -1;
    protected last_suspicios_quote_end_line_ = 0;
    protected last_suspicios_quote_end_col_ = 0;

    /**
     * XMQParser.parse: parse an xmq buffer, the callbacks are invoked for every token.
     * @param buf The xmq source.
     * @param source A description of where the buffer comes from, for error messages.
     * @throws XMQParseError If the input cannot be parsed.
     */
    parse(buf: string, source: string): void
    {
        this.source_ = source;
        this.buffer_ = buf;
        this.buffer_len_ = buf.length;
        this.i_ = 0;
        this.line_ = 1;
        this.col_ = 1;
        this.line_start_ = 0;
        this.nesting_depth_ = 0;
        this.last_suspicios_quote_end_ = -1;
        this.setup();
        this.parse_xmq();
        if (this.i_ < this.buffer_len_)
        {
            // Like xmqTokenizeBuffer in C: parse_xmq returned at a } on the top level.
            throw this.error_here(ParseErrorCode.XMQ_ERROR_UNEXPECTED_CLOSING_BRACE);
        }
    }

    /** XMQParser.setup: invoked before the parse starts. */
    protected setup(): void
    {
    }

    protected abstract do_whitespace(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_quote(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_element_value_quote(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_element_value_compound_quote(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_attr_value_quote(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_attr_value_compound_quote(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_entity(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_element_value_entity(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_element_value_compound_entity(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_attr_value_entity(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_attr_value_compound_entity(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_comment(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_comment_continuation(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_element_key(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_element_name(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_element_ns(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_apar_left(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_apar_right(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_cpar_left(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_cpar_right(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_brace_left(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_brace_right(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_equals(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_equals_done(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_attr_value_text(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_element_value_text(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_ns_declaration(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_attr_key(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_ns_colon(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;
    protected abstract do_attr_ns(start_line: number, start_col: number, start: number, stop: number, stop_suffix: number): void;

    /**
     * XMQParser.do_embedded_xml: an embedded xml fragment <...> was found, by default it
     * is ignored.
     */
    protected do_embedded_xml(start: number, stop: number): void
    {
    }

    /** A parse error at the current position. */
    protected error_here(code: ParseErrorCode): XMQParseError
    {
        return this.error_at(code, this.i_, this.line_, this.col_);
    }

    /**
     * A parse error at a position, the message like the C xmq (xmqTokenizeBuffer and
     * generate_state_error_message): an unexpected character after a quote that perhaps
     * ended too early first gets a warning about it.
     */
    protected error_at(code: ParseErrorCode, pos: number, line: number, col: number): XMQParseError
    {
        let msg = generate_state_error_message(this.buffer_, this.source_, code, pos, line, col);
        if (code === ParseErrorCode.XMQ_ERROR_INVALID_CHAR && this.last_suspicios_quote_end_ >= 0)
        {
            msg = generate_state_error_message(this.buffer_, this.source_, ParseErrorCode.XMQ_WARNING_QUOTES_NEEDED,
                                               this.last_suspicios_quote_end_, this.last_suspicios_quote_end_line_,
                                               this.last_suspicios_quote_end_col_) + "\n" + msg;
        }
        return new XMQParseError(code, msg, line, col, this.source_);
    }

    /**
     * XMQParser.increment: step over the current character. Like C the column counts
     * characters (code points), the second half of a surrogate pair does not count.
     */
    protected increment(c: string): void
    {
        const u = c.charCodeAt(0);
        if (!(u >= 0xdc00 && u <= 0xdfff)) this.col_++;
        if (c === "\n")
        {
            this.line_start_ = this.i_ + 1;
            this.line_++;
            this.col_ = 1;
        }
        this.i_++;
    }

    /** XMQParser.currentChar: the current character or "" at the end. */
    protected currentChar(): string
    {
        return this.i_ < this.buffer_len_ ? this.buffer_.charAt(this.i_) : "";
    }

    /** XMQParser.nextChar: the character after the current one or "" at the end. */
    protected nextChar(): string
    {
        return this.i_ + 1 < this.buffer_len_ ? this.buffer_.charAt(this.i_ + 1) : "";
    }

    /** XMQParser.count_whitespace: 1 if the current character is any kind of whitespace. */
    private count_whitespace(): number
    {
        const c = this.currentChar();
        if (c === " " ||
            c === "\n" ||
            c === "\t" ||
            c === "\r" ||
            c === " " || // Unbreakable space
            c === " " || // EN quad.
            c === " " || // EM quad.
            c === " " || // EN space.
            c === " ") // EM space.
        {
            return 1;
        }
        return 0;
    }

    /** XMQParser.is_safe_value_char */
    private is_safe_value_char(): boolean
    {
        const c = this.currentChar();
        const is_ws = this.count_whitespace() > 0 ||
            c === "\n" || c === "\t" || c === "\r" ||
            c === "(" || c === ")" || c === "{" || c === "}" ||
            c === "'" || c === "\"";
        return !is_ws;
    }

    /** XMQParser.eat_xmq_doctype: the name includes the bang, ie !DOCTYPE. */
    private eat_xmq_doctype(): [number, number]
    {
        const start = this.i_;
        this.increment(this.currentChar());
        while (this.i_ < this.buffer_len_)
        {
            const c = this.currentChar();
            if (!is_xmq_text_name(c)) break;
            this.increment(c);
        }
        return [start, this.i_];
    }

    /** XMQParser.eat_xmq_pi: the token includes the leading ?, that marks a processing instruction. */
    private eat_xmq_pi(): [number, number]
    {
        const start = this.i_;
        this.increment(this.currentChar());
        while (this.i_ < this.buffer_len_)
        {
            const c = this.currentChar();
            if (!is_xmq_text_name(c)) break;
            this.increment(c);
        }
        return [start, this.i_];
    }

    /**
     * XMQParser.eat_xmq_token_whitespace: like C, a run of whitespace that starts with xmq
     * token whitespace (space, newline, cr) continues over any whitespace, eg nbsp, but not
     * a tab.
     */
    private eat_xmq_token_whitespace(): [number, number]
    {
        const start = this.i_;
        while (this.i_ < this.buffer_len_)
        {
            const c = this.currentChar();
            if (this.count_whitespace() === 0 || c === "\t") break;
            this.increment(c);
        }
        return [start, this.i_];
    }

    /** XMQParser.parse_xmq_whitespace */
    protected parse_xmq_whitespace(): void
    {
        const start_line = this.line_;
        const start_col = this.col_;
        const [start, stop] = this.eat_xmq_token_whitespace();
        this.do_whitespace(start_line, start_col, start, stop, stop);
    }

    /** XMQParser.count_xmq_quotes */
    private count_xmq_quotes(q: string): number
    {
        let i = this.i_;
        while (i < this.buffer_len_ && this.buffer_.charAt(i) === q) i++;
        return i - this.i_;
    }

    /** XMQParser.findNLBackwards */
    protected findNLBackwards(i: number, max: number): number
    {
        // An error at the end of the buffer starts looking at the last char.
        if (i >= this.buffer_len_) i = this.buffer_len_ - 1;
        if (i < 0) return 0;
        while (max > 0 && i > 0 && this.buffer_.charAt(i) !== "\n") { i--; max--; }
        if (this.buffer_.charAt(i) === "\n") i++;
        return i;
    }

    /** XMQParser.findNL */
    protected findNL(i: number, max: number): number
    {
        while (max > 0 && i < this.buffer_len_ && this.buffer_.charAt(i) !== "\n") { i++; max--; }
        return i;
    }

    /** XMQParser.eat_xmq_quote */
    private eat_xmq_quote(): [number, number]
    {
        // Grab ' or " to know which kind of quote it is.
        const q = this.currentChar();
        let depth = this.count_xmq_quotes(q);
        let count = depth;

        this.last_quote_start_ = this.i_;
        this.last_quote_start_line_ = this.line_;
        this.last_quote_start_col_ = this.col_;

        const start = this.i_;
        let stop = this.i_;

        while (count > 0)
        {
            this.increment(q);
            count--;
        }

        if (depth === 2)
        {
            // The empty quote ''
            return [start, this.i_];
        }

        while (this.i_ < this.buffer_len_)
        {
            const c = this.currentChar();
            if (c !== q)
            {
                this.increment(c);
                continue;
            }
            count = this.count_xmq_quotes(q);
            if (count > depth)
            {
                // Like C the error is reported where the quote starts.
                throw this.error_at(ParseErrorCode.XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES, this.last_quote_start_,
                                    this.last_quote_start_line_, this.last_quote_start_col_);
            }
            else if (count < depth)
            {
                while (count > 0)
                {
                    this.increment(q);
                    count--;
                }
                continue;
            }
            else
            {
                while (count > 0)
                {
                    this.increment(q);
                    count--;
                }
                depth = 0;
                stop = this.i_;
                break;
            }
        }

        if (depth !== 0)
        {
            throw this.error_at(ParseErrorCode.XMQ_ERROR_QUOTE_NOT_CLOSED, this.last_quote_start_,
                                this.last_quote_start_line_, this.last_quote_start_col_);
        }
        if (this.possibly_need_more_quotes())
        {
            this.last_suspicios_quote_end_ = this.i_ - 1;
            this.last_suspicios_quote_end_line_ = this.line_;
            this.last_suspicios_quote_end_col_ = this.col_ - 1;
        }
        return [start, stop];
    }

    /**
     * XMQParser.possibly_need_more_quotes (possibly_need_more_quotes in C): after a quote,
     * a quote that ends inside a word, eg the s in 'There's a man.', perhaps needed more
     * quotes. It is remembered and reported as a warning if the parse then fails with an
     * unexpected character.
     */
    private possibly_need_more_quotes(): boolean
    {
        if (this.i_ - 2 < 0 || this.i_ >= this.buffer_len_) return false;
        const c0 = this.buffer_.charAt(this.i_ - 2);
        const c1 = this.buffer_.charAt(this.i_ - 1); // This is the apostrophe.
        const c2 = this.currentChar();
        return c0 !== "'" && c0 !== " " && c1 === "'" && c2 !== "'" && c2 !== "" && c2 !== "\0" &&
            " \t\r\n(){}".indexOf(c2) < 0;
    }

    /** XMQParser.parse_xmq_quote */
    private parse_xmq_quote(level: XMQLevel): void
    {
        const start_line = this.line_;
        const start_col = this.col_;
        const [start, stop] = this.eat_xmq_quote();

        switch (level)
        {
        case XMQLevel.LEVEL_XMQ:
            this.do_quote(start_line, start_col, start, stop, stop);
            break;
        case XMQLevel.LEVEL_ELEMENT_VALUE:
            this.do_element_value_quote(start_line, start_col, start, stop, stop);
            break;
        case XMQLevel.LEVEL_ELEMENT_VALUE_COMPOUND:
            this.do_element_value_compound_quote(start_line, start_col, start, stop, stop);
            break;
        case XMQLevel.LEVEL_ATTR_VALUE:
            this.do_attr_value_quote(start_line, start_col, start, stop, stop);
            break;
        case XMQLevel.LEVEL_ATTR_VALUE_COMPOUND:
            this.do_attr_value_compound_quote(start_line, start_col, start, stop, stop);
            break;
        }
    }

    /** XMQParser.eat_xmq_entity */
    private eat_xmq_entity(): void
    {
        // Like C an error is reported where the entity starts.
        const start = this.i_, start_line = this.line_, start_col = this.col_;
        this.increment("&");
        let c = "";
        let expect_semicolon = false;

        while (this.i_ < this.buffer_len_)
        {
            c = this.currentChar();
            if (!is_xmq_text_name(c)) break;
            if (!is_lowercase_hex(c)) expect_semicolon = true;
            this.increment(c);
        }
        if (c === ";")
        {
            this.increment(c);
            expect_semicolon = false;
        }
        if (expect_semicolon)
        {
            throw this.error_at(ParseErrorCode.XMQ_ERROR_ENTITY_NOT_CLOSED, start, start_line, start_col);
        }
    }

    /** XMQParser.parse_xmq_entity */
    private parse_xmq_entity(level: XMQLevel): void
    {
        const start_line = this.line_;
        const start_col = this.col_;
        const start = this.i_;

        this.eat_xmq_entity();

        const stop = this.i_;

        switch (level)
        {
        case XMQLevel.LEVEL_XMQ:
            this.do_entity(start_line, start_col, start, stop, stop);
            break;
        case XMQLevel.LEVEL_ELEMENT_VALUE:
            this.do_element_value_entity(start_line, start_col, start, stop, stop);
            break;
        case XMQLevel.LEVEL_ELEMENT_VALUE_COMPOUND:
            this.do_element_value_compound_entity(start_line, start_col, start, stop, stop);
            break;
        case XMQLevel.LEVEL_ATTR_VALUE:
            this.do_attr_value_entity(start_line, start_col, start, stop, stop);
            break;
        case XMQLevel.LEVEL_ATTR_VALUE_COMPOUND:
            this.do_attr_value_compound_entity(start_line, start_col, start, stop, stop);
            break;
        }
    }

    /**
     * XMQParser.parse_xmq_compound: a compound value ( '   ' &#10; '  info ' ), it can
     * only occur after an equals sign.
     */
    private parse_xmq_compound(level: XMQLevel): void
    {
        let start = this.i_;
        let start_line = this.line_;
        let start_col = this.col_;

        this.last_compound_start_ = start;
        this.last_compound_start_line_ = start_line;
        this.last_compound_start_col_ = start_col;
        this.increment("(");
        let stop = this.i_;

        this.do_cpar_left(start_line, start_col, start, stop, stop);

        this.parse_xmq_compound_children(enter_compound_level(level));

        let c = this.currentChar();
        if (is_xmq_token_whitespace(c)) { this.parse_xmq_whitespace(); c = this.currentChar(); }

        if (c !== ")")
        {
            // Like C, the error is reported where the compound should have been closed.
            throw this.error_at(ParseErrorCode.XMQ_ERROR_COMPOUND_NOT_CLOSED, this.i_, this.line_, this.col_);
        }

        start = this.i_;
        start_line = this.line_;
        start_col = this.col_;
        this.increment(")");
        stop = this.i_;

        this.do_cpar_right(start_line, start_col, start, stop, stop);
    }

    /** XMQParser.parse_xmq_compound_children: quotes and entities until the end or a ). */
    private parse_xmq_compound_children(level: XMQLevel): void
    {
        while (this.i_ < this.buffer_len_)
        {
            const c = this.currentChar();
            if (is_xmq_token_whitespace(c)) this.parse_xmq_whitespace();
            else if (c === ")") break;
            else if (c === "'" || c === "\"") this.parse_xmq_quote(level);
            else if (c === "&") this.parse_xmq_entity(level);
            else
            {
                throw this.error_at(ParseErrorCode.XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN, this.i_, this.line_, this.col_);
            }
        }
    }

    /** XMQParser.count_xmq_slashes: the number of slashes and whether an asterisk follows. */
    private count_xmq_slashes(): [number, boolean]
    {
        let i = this.i_;
        const start = this.i_;
        while (i < this.buffer_len_ && this.buffer_.charAt(i) === "/") i++;
        const fa = this.buffer_.charAt(i) === "*"; // Found asterisk?
        return [i - start, fa];
    }

    /** XMQParser.eat_xmq_comment_to_eol */
    private eat_xmq_comment_to_eol(): void
    {
        this.increment("/");
        this.increment("/");
        let c = "";
        while (this.i_ < this.buffer_len_ && c !== "\n")
        {
            c = this.currentChar();
            this.increment(c);
        }
    }

    /** XMQParser.eat_xmq_comment_to_close: returns true if a continuation follows. */
    private eat_xmq_comment_to_close(num_slashes: number): boolean
    {
        // Like C an error is reported where this part of the comment starts.
        const part_start = this.i_, part_line = this.line_, part_col = this.col_;
        let n = num_slashes;

        if (this.buffer_.charAt(this.i_) === "/")
        {
            // Comment starts from the beginning ////* ....
            // Otherwise this is a continuation and *i == '*'
            while (n > 0)
            {
                this.increment("/");
                n--;
            }
        }

        this.increment("*");

        let c = "";
        let cc = "";
        n = 0;
        while (this.i_ < this.buffer_len_)
        {
            cc = c;
            c = this.currentChar();
            if (cc !== "*" || c !== "/")
            {
                // Not a possible end marker */ or *///// continue eating.
                this.increment(c);
                continue;
            }
            // We have found */ or *//// now count the number of slashes.
            const [slashes, found_asterisk] = this.count_xmq_slashes();
            n = slashes;

            if (n < num_slashes) continue; // Not a balanced end marker continue eating,

            if (n > num_slashes)
            {
                // Oups, too many slashes.
                throw this.error_at(ParseErrorCode.XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES, part_start, part_line, part_col);
            }

            // Found the ending slashes!
            while (n > 0)
            {
                cc = c;
                c = this.currentChar();
                this.increment("/");
                n--;
            }
            return found_asterisk;
        }
        // We reached the end of the xmq and no */ was found!
        throw this.error_at(ParseErrorCode.XMQ_ERROR_COMMENT_NOT_CLOSED, part_start, part_line, part_col);
    }

    /** XMQParser.parse_xmq_comment */
    private parse_xmq_comment(): void
    {
        let start = this.i_;
        let start_line = this.line_;
        let start_col = this.col_;

        this.last_comment_start_ = start;
        this.last_comment_start_line_ = this.line_;
        this.last_comment_start_col_ = this.col_;

        const [n, asterisk] = this.count_xmq_slashes();
        let found_asterisk = asterisk;
        if (!found_asterisk)
        {
            // This is a single line comment.
            this.eat_xmq_comment_to_eol();
            const stop = this.i_;
            this.do_comment(start_line, start_col, start, stop, stop);
        }
        else
        {
            // This is a /* ... */ or ////*  ... *//// comment.
            found_asterisk = this.eat_xmq_comment_to_close(n);
            let stop = this.i_;
            this.do_comment(start_line, start_col, start, stop, stop);

            while (found_asterisk)
            {
                // Aha, this is a comment continuation /* ... */* ...
                start = this.i_;
                start_line = this.line_;
                start_col = this.col_;
                found_asterisk = this.eat_xmq_comment_to_close(n);
                stop = this.i_;
                this.do_comment_continuation(start_line, start_col, start, stop, stop);
            }
        }
    }

    /**
     * XMQParser.eat_xmq_text_name: a name, with the namespace prefix before a colon.
     * @return text_start, text_stop, namespace_start, namespace_stop (0 and 0 without prefix).
     */
    private eat_xmq_text_name(): [number, number, number, number]
    {
        let text_start = this.i_;
        let colon = 0;

        while (this.i_ < this.buffer_len_)
        {
            const c = this.currentChar();
            if (!is_xmq_text_name(c)) break;
            if (c === ":") colon = this.i_;
            this.increment(c);
        }

        let namespace_start = 0;
        let namespace_stop = 0;
        if (colon > 0)
        {
            namespace_start = text_start;
            namespace_stop = colon;
            text_start = colon + 1;
        }
        return [text_start, this.i_, namespace_start, namespace_stop];
    }

    /** XMQParser.eat_xmq_text_value */
    private eat_xmq_text_value(): void
    {
        while (this.i_ < this.buffer_len_)
        {
            const c = this.currentChar();
            if (!this.is_safe_value_char()) break;
            this.increment(c);
        }
    }

    /** XMQParser.peek_xmq_next_is_equal: the only peek ahead in the whole grammar. */
    private peek_xmq_next_is_equal(): boolean
    {
        let i = this.i_;
        let c = "";
        while (i < this.buffer_len_)
        {
            c = this.buffer_.charAt(i);
            if (!is_xmq_token_whitespace(c)) break;
            i++;
        }
        return c === "=";
    }

    /** XMQParser.parse_xmq_text_value */
    private parse_xmq_text_value(level: XMQLevel): void
    {
        const start = this.i_;
        const start_line = this.line_;
        const start_col = this.col_;

        this.eat_xmq_text_value();

        const stop = this.i_;

        if (level === XMQLevel.LEVEL_ATTR_VALUE)
        {
            this.do_attr_value_text(start_line, start_col, start, stop, stop);
        }
        else
        {
            this.do_element_value_text(start_line, start_col, start, stop, stop);
        }
    }

    /** XMQParser.parse_xmq_value */
    private parse_xmq_value(level: XMQLevel): void
    {
        let c = this.currentChar();

        if (is_xmq_token_whitespace(c)) { this.parse_xmq_whitespace(); c = this.currentChar(); }

        if (c === "'" || c === "\"")
        {
            this.parse_xmq_quote(level);
        }
        else if (c === "&")
        {
            this.parse_xmq_entity(level);
        }
        else if (c === "(")
        {
            this.parse_xmq_compound(level);
        }
        else
        {
            const cc = this.nextChar();
            if (unsafe_value_start(c, cc))
            {
                throw this.error_here(ParseErrorCode.XMQ_ERROR_VALUE_CANNOT_START_WITH);
            }
            this.parse_xmq_text_value(level);
        }
    }

    /** XMQParser.parse_xmq_attributes: key = value, or key, until a ) is found. */
    private parse_xmq_attributes(): void
    {
        while (this.i_ < this.buffer_len_)
        {
            const c = this.currentChar();

            if (is_xmq_token_whitespace(c)) this.parse_xmq_whitespace();
            else if (c === ")") return;
            // A key start that is not a name character (eg ; or å) would give an empty key
            // without advancing, stop here so that the attributes are reported as not closed.
            else if (is_xmq_attribute_key_start(c) && is_xmq_text_name(c)) this.parse_xmq_attribute();
            else break;
        }
    }

    /** XMQParser.parse_xmq_attribute */
    private parse_xmq_attribute(): void
    {
        let start_line = this.line_;
        let start_col = this.col_;

        const [name_start, name_stop, ns_start, ns_stop] = this.eat_xmq_text_name();

        if (ns_stop === 0)
        {
            // No colon found, we have either a normal: key=123
            // or a default namespace declaration xmlns=...
            if (this.buffer_.substring(name_start, name_stop) === "xmlns")
            {
                // A default namespace declaration, eg: xmlns=uri
                this.do_ns_declaration(start_line, start_col, name_start, name_stop, name_stop);
            }
            else
            {
                // A normal attribute key, eg: width=123
                this.do_attr_key(start_line, start_col, name_start, name_stop, name_stop);
            }
        }
        else
        {
            // We have a colon in the attribute key.
            // E.g. alfa:beta where alfa is attr_ns and beta is attr_key
            // However we can also have xmlns:xsl then it gets tokenized as ns_declaration and attr_ns.
            const ns_len = ns_stop - ns_start;
            if (this.buffer_.substring(ns_start, ns_stop) === "xmlns")
            {
                // The xmlns signals a declaration of a namespace.
                this.do_ns_declaration(start_line, start_col, ns_start, ns_stop, name_stop);
                this.do_ns_colon(start_line, start_col + ns_len, ns_stop, ns_stop + 1, ns_stop + 1);
                this.do_attr_ns(start_line, start_col + ns_len + 1, name_start, name_stop, name_stop);
            }
            else
            {
                // A normal namespaced attribute, eg: xlink:href.
                this.do_attr_ns(start_line, start_col, ns_start, ns_stop, ns_stop);
                this.do_ns_colon(start_line, start_col + ns_len, ns_stop, ns_stop + 1, ns_stop + 1);
                this.do_attr_key(start_line, start_col + ns_len + 1, name_start, name_stop, name_stop);
            }
        }

        let c = this.currentChar();
        if (is_xmq_token_whitespace(c)) { this.parse_xmq_whitespace(); c = this.currentChar(); }

        if (c === "=")
        {
            const start = this.i_;
            start_line = this.line_;
            start_col = this.col_;
            this.increment("=");
            const stop = this.i_;
            this.do_equals(start_line, start_col, start, stop, stop);
            this.parse_xmq_value(XMQLevel.LEVEL_ATTR_VALUE);
            this.do_equals_done(this.line_, this.col_, this.i_, this.i_, this.i_);
        }
    }

    /**
     * XMQParser.parse_xmq_element_internal: an element, a doctype or a processing
     * instruction. Unlike Java and C it does not parse the body by recursion: it returns
     * true when it opened a body {, then parse_xmq parses the body and closes it.
     */
    private parse_xmq_element_internal(doctype: boolean, pi: boolean): boolean
    {
        let name_start = 0;
        let name_stop = 0;
        let ns_start = 0;
        let ns_stop = 0;

        let start_line = this.line_;
        let start_col = this.col_;

        if (doctype)
        {
            [name_start, name_stop] = this.eat_xmq_doctype();
        }
        else if (pi)
        {
            [name_start, name_stop] = this.eat_xmq_pi();
        }
        else
        {
            [name_start, name_stop, ns_start, ns_stop] = this.eat_xmq_text_name();
        }
        let stop = this.i_;

        // The only peek ahead in the whole grammar! And its only for syntax coloring. :-)
        // key = 123   vs    name { '123' }
        const is_key = this.peek_xmq_next_is_equal();

        if (ns_stop === 0)
        {
            // Normal key/name element.
            if (is_key) this.do_element_key(start_line, start_col, name_start, name_stop, stop);
            else this.do_element_name(start_line, start_col, name_start, name_stop, stop);
        }
        else
        {
            // We have a namespace prefixed to the element, eg: abc:working
            const ns_len = ns_stop - ns_start;
            this.do_element_ns(start_line, start_col, ns_start, ns_stop, ns_stop);
            this.do_ns_colon(start_line, start_col + ns_len, ns_stop, ns_stop + 1, ns_stop + 1);

            if (is_key) this.do_element_key(start_line, start_col + ns_len + 1, name_start, name_stop, stop);
            else this.do_element_name(start_line, start_col + ns_len + 1, name_start, name_stop, stop);
        }

        let c = this.currentChar();
        if (is_xmq_token_whitespace(c)) { this.parse_xmq_whitespace(); c = this.currentChar(); }

        if (c === "(")
        {
            let start = this.i_;
            this.last_attr_start_ = this.i_;
            this.last_attr_start_line_ = this.line_;
            this.last_attr_start_col_ = this.col_;
            start_line = this.line_;
            start_col = this.col_;
            this.increment("(");
            stop = this.i_;
            this.do_apar_left(start_line, start_col, start, stop, stop);

            this.parse_xmq_attributes();

            c = this.currentChar();
            if (is_xmq_token_whitespace(c)) { this.parse_xmq_whitespace(); c = this.currentChar(); }
            if (c !== ")")
            {
                // Like C, report the error where the attributes started.
                throw this.error_at(ParseErrorCode.XMQ_ERROR_ATTRIBUTES_NOT_CLOSED, this.last_attr_start_,
                                    this.last_attr_start_line_, this.last_attr_start_col_);
            }

            start = this.i_;
            const parentheses_right_start = this.i_;
            const parentheses_right_stop = this.i_ + 1;

            start_line = this.line_;
            start_col = this.col_;
            this.increment(")");
            stop = this.i_;
            this.do_apar_right(start_line, start_col, parentheses_right_start, parentheses_right_stop, stop);
        }

        c = this.currentChar();
        if (is_xmq_token_whitespace(c)) { this.parse_xmq_whitespace(); c = this.currentChar(); }

        if (c === "=")
        {
            this.last_equals_start_ = this.i_;
            this.last_equals_start_line_ = this.line_;
            this.last_equals_start_col_ = this.col_;
            const start = this.i_;
            start_line = this.line_;
            start_col = this.col_;
            this.increment("=");
            stop = this.i_;

            this.do_equals(start_line, start_col, start, stop, stop);
            this.parse_xmq_value(XMQLevel.LEVEL_ELEMENT_VALUE);
            this.do_equals_done(this.line_, this.col_, this.i_, this.i_, this.i_);
            return false;
        }

        if (c === "{")
        {
            let start = this.i_;
            this.last_body_start_ = this.i_;
            this.last_body_start_line_ = this.line_;
            this.last_body_start_col_ = this.col_;
            start_line = this.line_;
            start_col = this.col_;
            this.increment("{");
            stop = this.i_;
            this.do_brace_left(start_line, start_col, start, stop, stop);

            if (++this.nesting_depth_ > XMQParser.MAX_NESTING_DEPTH)
            {
                throw this.error_here(ParseErrorCode.XMQ_ERROR_TOO_DEEP);
            }
            return true;
        }
        return false;
    }

    /** The end of parse_xmq_element_internal in Java: the } of a body. */
    private close_body(): void
    {
        this.nesting_depth_--;
        const start = this.i_;
        const start_line = this.line_;
        const start_col = this.col_;
        this.increment("}");
        const stop = this.i_;
        this.do_brace_right(start_line, start_col, start, stop, stop);
    }

    /**
     * XMQParser.parse_xmq: the content of the document, until the end or a } on the top
     * level. Java and C parse every body by recursion, but the stack of node and the
     * browsers overflows at about 2500 nested bodies (the parsers accept 10000), so here
     * the bodies are parsed in this loop: open counts the bodies that are open, a } closes
     * the innermost one. The tokens and the error positions are the same.
     */
    protected parse_xmq(): void
    {
        let open = 0;
        for (;;)
        {
            if (this.i_ >= this.buffer_len_)
            {
                if (open > 0)
                {
                    throw this.error_at(ParseErrorCode.XMQ_ERROR_BODY_NOT_CLOSED, this.last_body_start_,
                                        this.last_body_start_line_, this.last_body_start_col_);
                }
                return;
            }
            const c = this.currentChar();
            let cc = "";
            if ((c === "/" || c === "(") && this.i_ + 1 < this.buffer_len_) cc = this.buffer_.charAt(this.i_ + 1);
            if (is_xmq_token_whitespace(c)) this.parse_xmq_whitespace();
            else if (c === "'" || c === "\"") this.parse_xmq_quote(XMQLevel.LEVEL_XMQ);
            else if (c === "&") this.parse_xmq_entity(XMQLevel.LEVEL_XMQ);
            else if (is_xmq_comment_start(c, cc)) this.parse_xmq_comment();
            else if (is_xmq_element_start(c)) { if (this.parse_xmq_element_internal(false, false)) open++; }
            else if (c === "}")
            {
                if (open === 0) return;
                this.close_body();
                open--;
            }
            else if (this.is_xmq_doctype_start()) { if (this.parse_xmq_element_internal(true, false)) open++; }
            else if (this.is_xmq_pi_start()) { if (this.parse_xmq_element_internal(false, true)) open++; }
            else if (c === "<") this.parse_xmq_embedded_xml();
            else if (this.possibly_lost_content_after_equals())
            {
                throw this.error_at(ParseErrorCode.XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS, this.last_equals_start_,
                                    this.last_equals_start_line_, this.last_equals_start_col_);
            }
            else if (c === "\t") throw this.error_here(ParseErrorCode.XMQ_ERROR_UNEXPECTED_TAB);
            else throw this.error_here(ParseErrorCode.XMQ_ERROR_INVALID_CHAR);
        }
    }

    /**
     * XMQParser.is_xmq_doctype_start (is_xmq_doctype_start in C): !DOCTYPE followed by = or whitespace.
     */
    private is_xmq_doctype_start(): boolean
    {
        const b = this.buffer_;
        const i = this.i_;
        if (b.charAt(i) !== "!") return false;
        if (i + 8 > this.buffer_len_) return false;
        if (!b.startsWith("!DOCTYPE", i)) return false;
        // Ooups, !DOCTYPE must have some value.
        if (i + 8 === this.buffer_len_) return false;
        const c = b.charAt(i + 8);
        // !DOCTYPE= or !DOCTYPE = etc
        return c === "=" || c === " " || c === "\t" || c === "\n" || c === "\r";
    }

    /** XMQParser.is_xmq_pi_start (is_xmq_pi_start in C): a ? followed by at least one character, eg ?x */
    private is_xmq_pi_start(): boolean
    {
        return this.buffer_.charAt(this.i_) === "?" && this.i_ + 2 <= this.buffer_len_;
    }

    /**
     * XMQParser.possibly_lost_content_after_equals (in C too): an unexpected { ( or = after
     * a name on a line that follows a line ending with an equals sign, eg
     *     alfa =
     *     beta { 'more' }
     * where beta became the value of alfa.
     */
    private possibly_lost_content_after_equals(): boolean
    {
        const c = this.currentChar();
        if (!(c === "{" || c === "(" || c === "=")) return false;
        const b = this.buffer_;
        // Nothing before, eg the buffer starts with ( or {.
        if (this.i_ <= 0) return false;
        let i = this.i_ - 1;
        // Scan backwards for newline accepting only texts and xml whitespace.
        while (i > 0 && b.charAt(i) !== "\n" && (is_xmq_text_name(b.charAt(i)) || is_xml_whitespace(b.charAt(i)))) i--;
        if (i === 0 || b.charAt(i) !== "\n") return false;
        // We found the newline, see if the next character backwards is an equals sign.
        while (i > 0 && is_xml_whitespace(b.charAt(i))) i--;
        return b.charAt(i) === "=";
    }

    /**
     * XMQParser.find_xmq_embedded_xml_stop (find_xmq_embedded_xml_stop in C): the end of
     * an embedded xml fragment, a single element (possibly self closing) ending with its
     * matching closing tag. Returns -1 if it is not closed.
     */
    private find_xmq_embedded_xml_stop(start: number): number
    {
        const b = this.buffer_;
        const end = this.buffer_len_;
        let i = start;
        let depth = 0;
        while (i < end)
        {
            const c = b.charAt(i);
            if (c === "\"" || c === "'")
            {
                // A quoted string, skip it.
                i++;
                while (i < end && b.charAt(i) !== c) i++;
                if (i < end) i++;
                continue;
            }
            if (c !== "<") { i++; continue; }
            if (i + 1 >= end) return -1;
            const nc = b.charAt(i + 1);
            if (nc === "!")
            {
                if (i + 4 < end && b.startsWith("<!--", i))
                {
                    // An xml comment, skip to -->
                    while (i + 3 < end && !b.startsWith("-->", i)) i++;
                    if (i + 3 >= end) return -1;
                    i += 3;
                    continue;
                }
                // Cdata or doctype, skip to >
                while (i < end && b.charAt(i) !== ">") i++;
                if (i >= end) return -1;
                i++;
                continue;
            }
            if (nc === "?" || nc === "/")
            {
                // A processing instruction or a closing tag, skip to >
                while (i < end && b.charAt(i) !== ">") i++;
                if (i >= end) return -1;
                i++;
                if (nc === "/")
                {
                    depth--;
                    if (depth <= 0) return i;
                }
                continue;
            }
            // An opening tag <name ...>
            const tag_start = i;
            while (i < end && b.charAt(i) !== ">") i++;
            if (i >= end) return -1;
            let k = i - 1;
            while (k > tag_start && (b.charAt(k) === " " || b.charAt(k) === "\t" || b.charAt(k) === "\n" || b.charAt(k) === "\r")) k--;
            const self_closing = k > tag_start && b.charAt(k) === "/";
            i++;
            if (self_closing && depth === 0) return i;
            if (!self_closing) depth++;
        }
        return -1;
    }

    /** XMQParser.parse_xmq_embedded_xml (parse_xmq_embedded_xml in C). */
    private parse_xmq_embedded_xml(): void
    {
        const start = this.i_;
        const stop = this.find_xmq_embedded_xml_stop(start);
        if (stop < 0)
        {
            throw this.error_here(ParseErrorCode.XMQ_ERROR_XML_FRAGMENT_NOT_CLOSED);
        }
        this.do_embedded_xml(start, stop);
        while (this.i_ < stop) this.increment(this.buffer_.charAt(this.i_));
    }

    /** XMQParser.xmq_un_comment */
    protected xmq_un_comment(start: number, stop: number): string
    {
        return unComment(this.buffer_, start, stop);
    }
}
