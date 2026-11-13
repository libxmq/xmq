/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

package org.libxmq.imp.xml;

/**
 * Thrown when an xpath expression cannot be compiled or evaluated.
 *
 * The error codes and messages are the same as the libxml2 xmlXPathError
 * codes, since the semantics of the xpath engine are ported from libxml2
 * xpath.c (Copyright (C) 1998-2012 Daniel Veillard, MIT license).
 */
public class XPathException extends Exception
{
    private static final long serialVersionUID = 1L;

    /** Number encoding error. */
    public static final int NUMBER_ERROR = 1;
    /** Unfinished literal. */
    public static final int UNFINISHED_LITERAL_ERROR = 2;
    /** Start of literal expected. */
    public static final int START_LITERAL_ERROR = 3;
    /** Expected $ for variable reference. */
    public static final int VARIABLE_REF_ERROR = 4;
    /** Undefined variable. */
    public static final int UNDEF_VARIABLE_ERROR = 5;
    /** Invalid predicate. */
    public static final int INVALID_PREDICATE_ERROR = 6;
    /** Invalid expression. */
    public static final int EXPR_ERROR = 7;
    /** Missing closing parenthesis/brace. */
    public static final int UNCLOSED_ERROR = 8;
    /** Unregistered function. */
    public static final int UNKNOWN_FUNC_ERROR = 9;
    /** Invalid operand. */
    public static final int INVALID_OPERAND = 10;
    /** Invalid type. */
    public static final int INVALID_TYPE = 11;
    /** Invalid number of arguments. */
    public static final int INVALID_ARITY = 12;
    /** Invalid context size, ie last() used outside of a predicate. */
    public static final int INVALID_CTXT_SIZE = 13;
    /** Invalid context position, ie position() used outside of a predicate. */
    public static final int INVALID_CTXT_POSITION = 14;
    /** Memory error, used when a traversal would grow a node-set without limit. */
    public static final int MEMORY_ERROR = 15;
    /** Undefined namespace prefix. */
    public static final int UNDEF_PREFIX_ERROR = 19;
    /** Recursion limit exceeded. */
    public static final int RECURSION_LIMIT_EXCEEDED = 26;

    private static final String[] MESSAGES = {
        "Ok",
        "Number encoding",
        "Unfinished literal",
        "Start of literal",
        "Expected $ for variable reference",
        "Undefined variable",
        "Invalid predicate",
        "Invalid expression",
        "Missing closing curly brace",
        "Unregistered function",
        "Invalid operand",
        "Invalid type",
        "Invalid number of arguments",
        "Invalid context size",
        "Invalid context position",
        "Memory allocation error",
        "Syntax error",
        "Resource error",
        "Sub resource error",
        "Undefined namespace prefix",
        "Encoding error",
        "Char out of XML range",
        "Invalid or incomplete context",
        "Stack usage error",
        "Forbidden variable",
        "Operation limit exceeded",
        "Recursion limit exceeded",
    };

    /** The libxml2 compatible error code. */
    private final int code_;
    /** The expression that failed, or null. */
    private final String expr_;
    /** The offset in the expression where the error was detected, or -1. */
    private final int offset_;

    /**
     * Create an xpath exception.
     * @param code The libxml2 compatible error code, eg {@link #EXPR_ERROR}.
     * @param expr The expression that failed, or null.
     * @param offset The offset in the expression where the error was detected, or -1.
     */
    public XPathException(int code, String expr, int offset)
    {
        super(buildMessage(code, expr, offset));
        code_ = code;
        expr_ = expr;
        offset_ = offset;
    }

    /**
     * Returns the libxml2 compatible error code.
     * @return The error code.
     */
    public int code()
    {
        return code_;
    }

    /**
     * Returns the expression that failed.
     * @return The expression or null.
     */
    public String expression()
    {
        return expr_;
    }

    /**
     * Returns the offset into the expression where the error was detected.
     * @return The offset or -1 if unknown.
     */
    public int offset()
    {
        return offset_;
    }

    /**
     * Returns the libxml2 message for an error code.
     * @param code The error code.
     * @return The message, eg "Invalid expression".
     */
    public static String messageFor(int code)
    {
        if (code < 0 || code >= MESSAGES.length) return "?? Unknown error ??";
        return MESSAGES[code];
    }

    private static String buildMessage(int code, String expr, int offset)
    {
        String m = messageFor(code);
        if (expr == null) return m;
        if (offset < 0) return m + " in " + expr;
        return m + " in " + expr + " at offset " + offset;
    }
}
