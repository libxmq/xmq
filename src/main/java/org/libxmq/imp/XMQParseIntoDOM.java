/* libxmq - Copyright (C) 2025-2026 Fredrik Öhrström (spdx: MIT)

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

import org.libxmq.ParseException;

import java.util.List;
import java.util.Stack;

import org.jdom2.Attribute;
import org.jdom2.Comment;
import org.jdom2.Content;
import org.jdom2.Document;
import org.jdom2.Element;
import org.jdom2.EntityRef;
import org.jdom2.Parent;
import org.jdom2.ProcessingInstruction;

/**
 * Parses xmq into a Document object model (jdom2).
 */
public class XMQParseIntoDOM extends XMQParser
{
    /**
     * Creates the parser with no source buffer.
     */
    public XMQParseIntoDOM()
    {
        super();
    }

    Document doc_ = new Document();
    Stack<Parent> element_stack_; // Top is last created node
    private ProcessingInstruction doctype_holder_; // The !DOCTYPE value holder
    Attribute attr_last_; // Last created attribute
    String namespace_declaration_; // xmlns or xmlns:alfa found
    String namespace_name_; // The alfa in xmlns:alfa.

    boolean parsing_doctype_; // True when parsing a doctype.
    boolean doctype_found_; // True after a doctype has been parsed.
    boolean parsing_pi_; // True when parsing a processing instruction, pi.
    boolean no_trim_quotes_; // No trimming if quotes, used when reading json strings.
    String pi_name_; // Name of the pi node just started.

    String element_namespace_; // The element namespace is found before the element name. Remember the namespace name here.
    String attribute_namespace_; // The attribute namespace is found before the attribute key. Remember the namespace name here.

    /**
     * Returns the parsed document.
     * @return The parsed document.
     */
    public Document doc()
    {
        return doc_;
    }

    void setup()
    {
        element_stack_ = new Stack<>();
        // JDOM2 requires a Document to have exactly one root element, so we
        // always create one here and drop it again after parsing, if possible.
        // An implicit root name passed along the API is used if available.
        String root_name = implicit_root_ == null ? "__XMQ_TMP_ROOT__" : implicit_root_;
        Element root = new Element(root_name);
        doc_.setRootElement(root);
    }

    /**
     * Parses an xmq buffer, then drops the created root element again if it
     * contains exactly one element, unless the element name equals the root
     * name, in which case it would only double nest the element.
     * @param buf The xmq source buffer to parse.
     * @param source A description of where the buffer comes from.
     * @return True if the input ends with a single root element.
     * @throws ParseException If the input cannot be parsed.
     */
    @Override
    public boolean parse(String buf, String source) throws ParseException
    {
        boolean ok = super.parse(buf, source);
        if (ok)
        {
            Element root = doc_.getRootElement();
            if (root != null)
            {
                List<Content> children = root.getContent();
                if (children.size() == 1 && children.get(0) instanceof Element only)
                {
                    if (root.getName().equals("__XMQ_TMP_ROOT__") || only.getName().equals(root.getName()))
                    {
                        only.detach();
                        doc_.setRootElement(only);
                    }
                }
            }
        }
        return ok;
    }

    XMQParseIntoDOM set_implicit_root_element(String name)
    {
        implicit_root_ = name;
        return this;
    }

    // Returns the current parent; the document root element
    // when the stack is empty (mirrors the W3C document base).
    Parent cur_parent()
    {
        if (element_stack_.isEmpty())
        {
            return doc_.getRootElement();
        }
        return element_stack_.peek();
    }

    // Returns the last child of the top of the stack (if any).
    Content last_child()
    {
        Parent p = cur_parent();
        List<Content> content = p.getContent();
        if (content.isEmpty())
        {
            return null;
        }
        return content.get(content.size() - 1);
    }

    void create_node(int start, int stop)
    {
        String name = buffer_.substring(start, stop);

        if (name.equals("!DOCTYPE"))
        {
            // The jdom2 DOM has no doctype value node, so we use a pi node named
            // DOCTYPE to hold the value. The printer prints such a pi node as a
            // !DOCTYPE line. The pi node is created now, so that do_equals has
            // a real last child to push onto the element stack.
            parsing_doctype_ = true;
            doctype_holder_ = new ProcessingInstruction("DOCTYPE", "");
            cur_parent().addContent(doctype_holder_);
        }
        else if (name.charAt(0) == '?')
        {
            parsing_pi_ = true;
            pi_name_ = name.substring(1); // Drop the ?
        }
        else
        {
            Element new_node;
            if (element_namespace_ != null)
            {
                new_node = new Element(name, element_namespace_);
                element_namespace_ = null;
            }
            else
            {
                new_node = new Element(name);
            }
            Parent parent = cur_parent();
            parent.addContent(new_node);
        }
    }

    protected void do_whitespace(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    void add_quote(int start, int stop)
    {
        var pair = UtilParseQuote.findQuoteStartStop(buffer_, start, stop);

        if (parsing_doctype_)
        {
            // The doctype value is stored in a pi node named DOCTYPE, which is
            // printed as a !DOCTYPE line. As for other values, incidental
            // indentation is trimmed on parsing; the printer re-adds it on
            // output, mirroring C.
            String content = UtilParseQuote.trimQuote(buffer_, pair.left(), pair.right());
            doctype_holder_.setData(content);
            parsing_doctype_ = false;
            doctype_found_ = true;
            return;
        }

        String content = UtilParseQuote.trimQuote(buffer_, pair.left(), pair.right());
        org.jdom2.Text text = new org.jdom2.Text(content);
        cur_parent().addContent(text);
    }

    protected void do_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        add_quote(start, stop);
    }

    protected void do_element_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        add_quote(start, stop);
    }

    protected void do_element_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        add_quote(start, stop);
    }

    protected void do_attr_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_attr_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        add_entity(start, stop);
    }

    protected void do_element_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        add_entity(start, stop);
    }

    protected void do_element_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        add_entity(start, stop);
    }

    protected void do_attr_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_attr_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    void add_entity(int start, int stop)
    {
        // start..stop covers '&name;'
        String name = buffer_.substring(start + 1, stop - 1);
        EntityRef entity = new EntityRef(name);
        cur_parent().addContent(entity);
    }

    protected void do_comment(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        String trimmed = no_trim_quotes_?buffer_.substring(start, stop):xmq_un_comment(start, stop);
        Comment c = new Comment(trimmed);

        cur_parent().addContent(c);
    }

    protected void do_comment_continuation(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        String trimmed = no_trim_quotes_?buffer_.substring(start, stop):xmq_un_comment(start, stop);

        Content last = last_child();
        if (last instanceof Comment c)
        {
            String t = c.getText();
            c.setText(t+"\n"+trimmed);
        }
        else
        {
            Comment c = new Comment("\n"+trimmed);
            cur_parent().addContent(c);
        }
    }

    protected void do_element_key(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        create_node(start, stop);
    }

    protected void do_element_name(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        create_node(start, stop);
    }

    protected void do_element_ns(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        element_namespace_ = buffer_.substring(start, stop);
    }

    protected void do_colon(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_apar_left(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_apar_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_cpar_left(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_cpar_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_brace_left(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        Content last = last_child();
        if (last instanceof Element e)
        {
            element_stack_.push(e);
        }
    }

    protected void do_brace_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        if (!element_stack_.isEmpty())
        {
            element_stack_.pop();
        }
    }

    protected void do_equals(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        Content last = last_child();
        if (last instanceof Element e)
        {
            element_stack_.push(e);
        }
    }

    protected void do_equals_done(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        if (!element_stack_.isEmpty())
        {
            element_stack_.pop();
        }
    }

    protected void do_attr_value_text(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        String text = buffer_.substring(start, stop);

        if (namespace_declaration_ != null)
        {
            // xmlns=uri or xmlns:prefix=uri was parsed.
            // At this point do_equals pushed the target element onto the stack,
            // so cur_parent() is the element to set the namespace on.
            Element parent = (Element)cur_parent();
            parent.setAttribute(namespace_declaration_, text);
            namespace_declaration_ = null;
            return;
        }

        String prev = (attr_last_ == null) ? "" : attr_last_.getValue();
        attr_last_.setValue(prev+text);
    }

    protected void do_element_value_text(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        if (parsing_pi_)
        {
            // Not yet implemented.
        }
        else if (parsing_doctype_)
        {
            // The jdom2 DOM has no doctype node holding a xmq doctype value, so
            // we use a pi node named DOCTYPE to store it. The printer prints
            // such a pi node as a !DOCTYPE line.
            doctype_holder_.setData(buffer_.substring(start, stop));
            parsing_doctype_ = false;
            doctype_found_ = true;
        }
        else
        {
            org.jdom2.Text text = new org.jdom2.Text(buffer_.substring(start,stop));
            cur_parent().addContent(text);
        }
    }

    protected void do_ns_declaration(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        String name = buffer_.substring(start, stop);
        namespace_declaration_ = name;
    }

    protected void do_attr_key(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        String name = buffer_.substring(start, stop);

        Attribute new_attr;
        // Namespaces are ignored: the jdom2 attribute value is set separately
        // by do_attr_value_text, so create the attribute with an empty value.
        new_attr = new Attribute(name, "");
        attr_last_ = new_attr;

        Content last = last_child();
        if (last instanceof Element el)
        {
            el.setAttribute(new_attr);
        }
    }

    protected void do_ns_colon(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_attr_ns(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        String name = buffer_.substring(start, stop);
        namespace_name_ = name;
        attribute_namespace_ = name;
    }

}
