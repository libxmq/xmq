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
