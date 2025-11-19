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

import org.libxmq.*;
import java.util.Stack;
import org.w3c.dom.Node;

class XMQPrintState
{
    int current_indent;
    int line_indent;
    char last_char;
    String replay_active_color_pre;
    String restart_line;
    String last_namespace;
    Stack<Node> pre_nodes; // Used to remember leading comments/doctype when printing json.
    int pre_post_num_comments_total; // Number of comments outside of the root element.
    int pre_post_num_comments_used; // Active number of comment outside of the root element.
    Stack<Node> post_nodes; // Used to remember ending comments when printing json.
    OutputSettings output_settings;

    StringBuilder buffer = new StringBuilder();


    void indent()
    {
        int n = current_indent;
        while (n > 0)
        {
            buffer.append(' ');
            n--;
        }
    }
}
