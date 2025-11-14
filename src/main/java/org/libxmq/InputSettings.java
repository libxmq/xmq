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

/**
 * Used to configure the parser behaviour when parsing XMQ.
 *
 * <p>Example:</p>
 * <pre>{@code
 * InputSettings is = new InputSettings().setImplicitRoot("config").setTrimNone();
 * }</pre>
 */

public class InputSettings
{
    // Do not merge adjacent text nodes.
    private boolean no_merge_;
    // If the first element in the XMQ file is not the same as the
    // implicit root, then add the implicit root. This allows for
    // XMQ files without an explicit root.
    private String implicit_root_;
    // Do not trim the whitespace in xml/html.
    private boolean trim_none_;

    /**
     * Set the implicit root when parsing XMQ. Normally XML requires an explicit
     * root element. However when loading an expected configuration file in XMQ
     * the root can be explicit (eg "config"). This means that an XMQ config
     * file can be as simple as a list of key=value pairs.
     *
     * <p>Example:</p>
     * <pre>{@code
     * speed=123
     * type=car
     * }</pre>
     *
     * @return this {@code InputSettings} instance, for method chaining
     */
    public InputSettings setImplicitRoot(String r)
    {
        implicit_root_ = r;
        return this;
    }

    /**
     * Returns the previously set trim none.
     * @return the trim none status
     */
    public String getImplicitRoot()
    {
        return implicit_root_;
    }

    /**
     * Disable whitespace trimming using the xmq heuristic when loading xml/html.
     * </p>
     *
     * @return this {@code OutputSettings} instance, for method chaining
     */
    public InputSettings setTrimNone(boolean b)
    {
        trim_none_ = b;
        return this;
    }

    /**
     * Returns the previously set trim none.
     * @return the trim none status
     */
    public boolean getTrimNone()
    {
        return trim_none_;
    }

    /**
     * Disable automatic merging of text noes when loading document.
     * </p>
     *
     * @return this {@code OutputSettings} instance, for method chaining
     */
    public InputSettings setNoMerge(boolean b)
    {
        no_merge_ = b;
        return this;
    }

    /**
     * Returns the previously set no merge.
     * @return the no merge status
     */
    public boolean getNoMerge()
    {
        return no_merge_;
    }

}
