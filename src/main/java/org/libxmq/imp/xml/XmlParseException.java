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
 * Thrown when xml cannot be parsed. The message is formatted like the
 * libxml2 error output, ie:
 * <pre>
 * file.xml:1: parser error : Entity 'foo' not defined
 * &lt;a&gt;&amp;foo;&lt;/a&gt;
 *         ^
 * </pre>
 */
public class XmlParseException extends Exception
{
    private static final long serialVersionUID = 1L;

    /** The line where the error was found, starting at 1. */
    public final int line;

    /** The error message without the source context. */
    public final String error;

    /**
     * Create an exception.
     * @param full The full formatted message, with the source context.
     * @param error The error message itself.
     * @param line The line number.
     */
    public XmlParseException(String full, String error, int line)
    {
        super(full);
        this.error = error;
        this.line = line;
    }
}
