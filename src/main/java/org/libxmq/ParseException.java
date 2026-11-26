/* libxmq - Copyright (C) 2025 Fredrik Öhrström (spdx: MIT)

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

package org.libxmq;

/** The parse exception contains necessary information
    to understand the parse failure. */
public class ParseException extends Exception
{
    private static final long serialVersionUID = 1L;
    /** The error code. */
    private final ParseErrorCode error_code_;
    // We print the error as:
    // car(speed=//xxx) {
    //           ^
    // Error: Text value cannot start with //.
    /** The text_ is the line that caused the parse error. */
    private final String text_;
    /** The line nr starts with 0. */
    private final int line_;
    /** The column is the offset into the text line. */
    private final int column_;
    /** Source is any informative text, can be a file name
        or "mybuffer" or "stdin" etc. */
    private final String source_;

    /** Construct a parse exception.
     @param ec The error code.
     @param text The line on which the parse failed.
     @param line The line number.
     @param column The column.
     @param source Information about where the xmq originated from.
    */
    public ParseException(ParseErrorCode ec, String text, int line, int column, String source)
    {
        super("Parse error: " + ec);
        error_code_ = ec;
        text_ = text;
        line_ = line;
        column_ = column;
        source_ = source;
        info_ = null;
        char_index_ = column - 1;
        caret_bytes_ = NOT_C_STYLE;
        code_point_ = 0;
        warning_ = null;
    }

    /** Extra information appended to the error message, or null. */
    private final String info_;
    /** The position of the offending char in the text, -1 if at the end of the input. */
    private final int char_index_;

    /** Construct a parse exception with extra information, like the C error_info,
        where the offending char is not at the column (as in the yaml parser).
     @param ec The error code.
     @param text The line on which the parse failed.
     @param line The line number.
     @param column The column that is reported.
     @param source Information about where the xmq originated from.
     @param info Extra information appended to the message, or null.
     @param char_index The position of the offending char in text, -1 if at the end of the input.
    */
    public ParseException(ParseErrorCode ec, String text, int line, int column, String source, String info, int char_index)
    {
        super("Parse error: " + ec);
        error_code_ = ec;
        text_ = text;
        line_ = line;
        column_ = column;
        source_ = source;
        info_ = info;
        char_index_ = char_index;
        caret_bytes_ = NOT_C_STYLE;
        code_point_ = 0;
        warning_ = null;
    }

    /** The caret_bytes of an exception that is not printed like generate_state_error_message. */
    static final int NOT_C_STYLE = Integer.MIN_VALUE;
    /** The number of utf8 bytes before the ^, -1 if the line is not shown, or NOT_C_STYLE. */
    private final int caret_bytes_;
    /** The character at the error, 0 at the end of the input, for an unexpected character. */
    private final int code_point_;
    /** A warning printed before the error, eg perhaps you need more quotes, or null. */
    private final ParseException warning_;

    /** Construct a parse exception of the xmq parser, printed like the C
        generate_state_error_message: the line of the input (at most 1024 bytes) with the ^
        after as many spaces as there are utf8 bytes before the error.
     @param ec The error code.
     @param text The line of the input around the error.
     @param line The line number.
     @param column The column, in characters (code points).
     @param source Information about where the xmq originated from.
     @param caret_bytes The number of utf8 bytes of the line before the error, -1 when the
            error is at the end of the input (the line is not shown).
     @param code_point The character at the error, 0 at the end of the input.
     @param warning A warning printed before the error, or null.
    */
    public ParseException(ParseErrorCode ec, String text, int line, int column, String source, int caret_bytes, int code_point, ParseException warning)
    {
        super("Parse error: " + ec);
        error_code_ = ec;
        text_ = text;
        line_ = line;
        column_ = column;
        source_ = source;
        info_ = null;
        char_index_ = column - 1;
        caret_bytes_ = caret_bytes;
        code_point_ = code_point;
        warning_ = warning;
    }

    /** Is this exception printed like generate_state_error_message in C.
     @return true if it is */
    public boolean cStyle()
    {
        return caret_bytes_ != NOT_C_STYLE;
    }

    /** Get the number of utf8 bytes before the ^, -1 if the line is not shown.
     @return the number of bytes */
    public int caretBytes()
    {
        return caret_bytes_;
    }

    /** Get the character at the error, 0 at the end of the input.
     @return the code point */
    public int codePoint()
    {
        return code_point_;
    }

    /** Get the warning printed before the error, or null.
     @return the warning */
    public ParseException warning()
    {
        return warning_;
    }

    /** Get the extra information, or null.
     @return the information */
    public String info()
    {
        return info_;
    }

    /** Get the position of the offending char in the text, -1 if at the end of the input.
     @return the position */
    public int charIndex()
    {
        return char_index_;
    }

    /** Get the error code.
     @return error code */
    public ParseErrorCode errorCode()
    {
        return error_code_;
    }

    /** Get the line of text.
     @return line of text */
    public String text()
    {
        return text_;
    }

    /** Get the line number.
        @return line number */
    public int line()
    {
        return line_;
    }

    /** Get the column.
        @return column */
    public int column()
    {
        return column_;
    }

    /** Get the source, ie file name, "mbuffer", "stdin" etc.
        @return The source information. */
    public String source()
    {
        return source_;
    }


}
