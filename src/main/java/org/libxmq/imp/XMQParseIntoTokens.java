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

package org.libxmq.imp;

class XMQParseIntoTokens extends XMQParser
{
    protected void do_whitespace(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[whitespace "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[quote "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_element_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_value_quote "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_element_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_value_compound_quote "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_attr_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[attr_value_quote "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_attr_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[attr_value_compound_quote "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[entity "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_element_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_value_entity "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_element_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_value_compound_entity "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_attr_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[attr_value_entity "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_attr_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[attr_value_compound_entity "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_comment(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[comment "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_comment_continuation(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[comment_continuation "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_element_key(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_key "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_element_name(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_name "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_element_ns(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_ns "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_colon(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[colon "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_apar_left(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[apar_left "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_apar_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[apar_right "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_brace_left(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[brace_left "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_brace_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[brace_right "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_equals(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[equals "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_equals_done(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        // Empty token, only used for popping in to DOM parser.
    }

    protected void do_attr_value_text(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[attr_value_text "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_element_value_text(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[element_value_text "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_ns_declaration(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[ns_declaration "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_attr_key(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[attr_key "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_ns_colon(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[ns_colon "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

    protected void do_attr_ns(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        System.out.print("[attr_ns "+
                         Util.xmq_quote_as_c(buffer_, start, stop, true)+" "+start_line+":"+start_col+"]");
    }

}
