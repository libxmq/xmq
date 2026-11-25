/* libxmq - Copyright (C) 2025-2026 Fredrik Öhrström (spdx: MIT) */

/**
 * The tokens of an xmq text as printed by xmq tokenize --type=debugtokens and
 * --type=debugcontent: a port of the Java XMQParseIntoTokens.
 */

import { XMQParser } from "./parser.js";
import { xmq_quote_as_c } from "./text.js";
import { findQuoteStartStop, trimQuoteContent } from "./trim.js";

/** The kinds of token output. */
export type TokenizeType = "debugtokens" | "debugcontent";

/** XMQParseIntoTokens: collects the token output in a string instead of printing it. */
export class XMQParseIntoTokens extends XMQParser
{
    private debug_tokens_ = false;
    private debug_content_ = false;
    private out_ = "";

    constructor(type: TokenizeType)
    {
        super();
        if (type === "debugtokens") this.debug_tokens_ = true;
        else this.debug_content_ = true;
    }

    /** The output so far. */
    output(): string
    {
        return this.out_;
    }

    protected setup(): void
    {
        this.out_ = "";
    }

    /** A token as [name "text" line:col]. */
    private token(name: string, start_line: number, start_col: number, start: number, stop: number): void
    {
        if (this.debug_tokens_)
        {
            this.out_ += "[" + name + " " + xmq_quote_as_c(this.buffer_, start, stop, true) + " " + start_line + ":" + start_col + "]";
        }
    }

    /** The trimmed content of a quote as {quote "text"}. */
    private quote_content(start: number, stop: number): void
    {
        if (this.debug_content_)
        {
            // First remove the surrounding ' or ", then the incidental whitespace.
            const [s, e] = findQuoteStartStop(this.buffer_, start, stop);
            const q = trimQuoteContent(this.buffer_, s, e);
            this.out_ += "{quote " + xmq_quote_as_c(q, 0, q.length, true) + "}";
        }
    }

    /** A text value as {value "text"}. */
    private value_content(start: number, stop: number): void
    {
        if (this.debug_content_)
        {
            this.out_ += "{value " + xmq_quote_as_c(this.buffer_, start, stop, true) + "}";
        }
    }

    protected do_whitespace(l: number, c: number, start: number, stop: number): void { this.token("whitespace", l, c, start, stop); }

    protected do_quote(l: number, c: number, start: number, stop: number): void
    {
        this.token("quote", l, c, start, stop);
        this.quote_content(start, stop);
    }

    protected do_element_value_quote(l: number, c: number, start: number, stop: number): void
    {
        this.token("element_value_quote", l, c, start, stop);
        this.quote_content(start, stop);
    }

    protected do_element_value_compound_quote(l: number, c: number, start: number, stop: number): void
    {
        this.token("element_value_compound_quote", l, c, start, stop);
        // Like C debug_content_quote, also used for compound quotes.
        this.quote_content(start, stop);
    }

    protected do_attr_value_quote(l: number, c: number, start: number, stop: number): void
    {
        this.token("attr_value_quote", l, c, start, stop);
        this.quote_content(start, stop);
    }

    protected do_attr_value_compound_quote(l: number, c: number, start: number, stop: number): void
    {
        this.token("attr_value_compound_quote", l, c, start, stop);
        this.quote_content(start, stop);
    }

    protected do_entity(l: number, c: number, start: number, stop: number): void { this.token("entity", l, c, start, stop); }
    protected do_element_value_entity(l: number, c: number, start: number, stop: number): void { this.token("element_value_entity", l, c, start, stop); }
    protected do_element_value_compound_entity(l: number, c: number, start: number, stop: number): void { this.token("element_value_compound_entity", l, c, start, stop); }
    protected do_attr_value_entity(l: number, c: number, start: number, stop: number): void { this.token("attr_value_entity", l, c, start, stop); }
    protected do_attr_value_compound_entity(l: number, c: number, start: number, stop: number): void { this.token("attr_value_compound_entity", l, c, start, stop); }

    protected do_comment(l: number, c: number, start: number, stop: number): void
    {
        this.token("comment", l, c, start, stop);
        if (this.debug_content_)
        {
            // Like C debug_content_comment, xmq_un_comment removes the comment markers
            // and the incidental whitespace.
            const q = this.xmq_un_comment(start, stop);
            this.out_ += "{comment " + xmq_quote_as_c(q, 0, q.length, true) + "}";
        }
    }

    protected do_comment_continuation(l: number, c: number, start: number, stop: number): void
    {
        this.token("comment_continuation", l, c, start, stop);
        if (this.debug_content_)
        {
            const q = this.xmq_un_comment(start, stop);
            this.out_ += "{comment_continuation " + xmq_quote_as_c(q, 0, q.length, true) + "}";
        }
    }

    protected do_element_key(l: number, c: number, start: number, stop: number): void { this.token("element_key", l, c, start, stop); }
    protected do_element_name(l: number, c: number, start: number, stop: number): void { this.token("element_name", l, c, start, stop); }
    protected do_element_ns(l: number, c: number, start: number, stop: number): void { this.token("element_ns", l, c, start, stop); }
    protected do_apar_left(l: number, c: number, start: number, stop: number): void { this.token("apar_left", l, c, start, stop); }
    protected do_apar_right(l: number, c: number, start: number, stop: number): void { this.token("apar_right", l, c, start, stop); }
    protected do_cpar_left(l: number, c: number, start: number, stop: number): void { this.token("cpar_left", l, c, start, stop); }
    protected do_cpar_right(l: number, c: number, start: number, stop: number): void { this.token("cpar_right", l, c, start, stop); }
    protected do_brace_left(l: number, c: number, start: number, stop: number): void { this.token("brace_left", l, c, start, stop); }
    protected do_brace_right(l: number, c: number, start: number, stop: number): void { this.token("brace_right", l, c, start, stop); }
    protected do_equals(l: number, c: number, start: number, stop: number): void { this.token("equals", l, c, start, stop); }

    protected do_equals_done(): void
    {
        // An empty token, only used for popping in the DOM parser.
    }

    protected do_attr_value_text(l: number, c: number, start: number, stop: number): void
    {
        this.token("attr_value_text", l, c, start, stop);
        this.value_content(start, stop);
    }

    protected do_element_value_text(l: number, c: number, start: number, stop: number): void
    {
        this.token("element_value_text", l, c, start, stop);
        this.value_content(start, stop);
    }

    protected do_ns_declaration(l: number, c: number, start: number, stop: number): void { this.token("ns_declaration", l, c, start, stop); }
    protected do_attr_key(l: number, c: number, start: number, stop: number): void { this.token("attr_key", l, c, start, stop); }
    protected do_ns_colon(l: number, c: number, start: number, stop: number): void { this.token("ns_colon", l, c, start, stop); }
    protected do_attr_ns(l: number, c: number, start: number, stop: number): void { this.token("attr_ns", l, c, start, stop); }
}

/**
 * The token output of xmq tokenize --type=debugtokens or --type=debugcontent, with the
 * final newline that the command line prints.
 * @throws XMQParseError If the xmq cannot be parsed.
 */
export function tokenize(xmq: string, type: TokenizeType, source: string = "-"): string
{
    const t = new XMQParseIntoTokens(type);
    t.parse(xmq, source);
    return t.output() + "\n";
}
