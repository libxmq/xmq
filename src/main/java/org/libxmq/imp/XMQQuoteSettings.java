/* libxmq - Copyright (C) 2023-2024 Fredrik Öhrström (spdx: MIT)

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

package org.libxmq.imp;

class XMQQuoteSettings
{
    public boolean force; // Always add single quotes. More quotes if necessary.
    public boolean compact; // Generate compact quote on a single line. Using &#10; and no superfluous whitespace.
    public boolean value_after_key; // If enties are introduced by the quoting, then use compound ( ) around the content.

    public String indentation_space;  // Use this as the indentation character. If NULL default to " ".
    public String explicit_space;  // Use this as the explicit space character inside quotes. If NULL default to " ".
    public String explicit_nl;      // Use this as the newline character. If NULL default to "\n".
    public String explicit_tab;      // Use this as the tab character. If NULL default to "\t".
    public String explicit_cr;      // Use this as the cr character. If NULL default to "\r".
    public String prefix_line;  // Prepend each line with this. If NULL default to empty string.
    public String postfix_line; // Append each line whith this, before any newline.
    public String prefix_entity;  // Prepend each entity with this. If NULL default to empty string.
    public String postfix_entity; // Append each entity whith this. If NULL default to empty string.
    public String prefix_doublep;  // Prepend each ( ) with this. If NULL default to empty string.
    public String postfix_doublep; // Append each ( ) whith this. If NULL default to empty string.
}
