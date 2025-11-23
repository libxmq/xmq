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

import org.w3c.dom.Document;

/**
 * The {@code OutputSettings} class is used to configure the output behavior
 * when printing XMQ. It provides options for controlling
 * pretty printing, indentation, and other formatting settings that affect
 * how XMQ content is presented as plain text, ansi-colored text, HTML or TeX.
 *
 * <p>Typical usage involves enabling or disabling pretty printing, setting
 * the indentation width, and adjusting other output-related preferences
 * to produce human-readable or compact XMQ output.</p>
 *
 * <p>Example:</p>
 * <pre>{@code
 * OutputSettings settings = new OutputSettings()
 *     .setPrettyPrint(true)
 *     .setIndentAmount(4);
 * }</pre>
 */

public class OutputSettings
{
    private boolean compact_;
    private int indent_amount_;

    /**
     * Create default output settings.
     */
    public OutputSettings()
    {
    }

    /**
     * Set if printing compact XMQ or not. Compact XMQ uses no newlines
     * for the entire document.
     *
     * @param c If true, then print XMQ on a single line, ie no newlines.
     * @return this {@code OutputSettings} instance, for method chaining
     */
    public OutputSettings setCompact(boolean c)
    {
        compact_ = c;
        return this;
    }

    /**
     * Returns the previously set compact setting.
     * @return the compact setting
     */
    public boolean compact()
    {
        return compact_;
    }

    /**
     * Sets the number of spaces to use for each level of indentation when
     * pretty printing XMQ output.
     * <p>
     * This setting is only applied when pretty printing is enabled.
     * Increasing the indent amount makes the output more readable, while
     * setting a smaller value produces more compact output.
     * </p>
     *
     * @param amount the number of spaces to use per indentation level;
     *               must be zero or a positive integer, negative values
     *               are interpreted as zero.
     * @return this {@code OutputSettings} instance, for method chaining
     */
    public OutputSettings setIndentAmount(int amount)
    {
        if (amount < 0) amount = 0;
        indent_amount_ = amount;
        return this;
    }

    /**
     * Returns the previously set indentation amount.
     * @return the indentation amount
     */
    public int indentAmount()
    {
        return indent_amount_;
    }


}
