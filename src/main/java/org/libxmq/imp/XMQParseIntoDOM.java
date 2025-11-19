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

import java.util.Stack;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;

public class XMQParseIntoDOM extends XMQParser
{
    DocumentBuilderFactory factory_;
    DocumentBuilder builder_;
    Document doc_;

    Stack<Node> element_stack_; // Top is last created node
    Node element_last_; // Last added sibling to stack top node.

    Node add_pre_node_before_; // Used when retrofitting pre-root comments and doctype found in json.
    Node add_post_node_after_; // Used when retrofitting post-root comments found in json.

    boolean parsing_doctype_; // True when parsing a doctype.
    /*
    void *add_doctype_before; // Used when retrofitting a doctype found in json.
    bool doctype_found; // True after a doctype has been parsed.
    */
    boolean parsing_pi_; // True when parsing a processing instruction, pi.
    /*
    bool merge_text; // Merge text nodes and character entities.
    */
    boolean no_trim_quotes_; // No trimming if quotes, used when reading json strings.
    String pi_name_; // Name of the pi node just started.
    /*
    XMQOutputSettings *output_settings; // Used when coloring existing text using the tokenizer.
    int magic_cookie; // Used to check that the state has been properly initialized.
    */

    String element_namespace_; // The element namespace is found before the element name. Remember the namespace name here.
    String attribute_namespace_; // The attribute namespace is found before the attribute key. Remember the namespace name here.
    boolean declaring_xmlns_; // Set to true when the xmlns declaration is found, the next attr value will be a href
    Element declaring_xmlns_namespace_; // The namespace to be updated with attribute value, eg. xmlns=uri or xmlns:prefix=uri

    //void *default_namespace; // If xmlns=http... has been set, then a pointer to the namespace object is stored here.

    void setup()
    {
        element_stack_ = new Stack<>();
        element_last_ = null;

        try
        {
            factory_ = DocumentBuilderFactory.newInstance();
            builder_ = factory_.newDocumentBuilder();
            doc_ = builder_.newDocument();
            doc_.setXmlStandalone(true);
        }
        catch (ParserConfigurationException e)
        {
            e.printStackTrace();
            System.exit(1);
        }
    }

    public Document doc()
    {
        return doc_;
    }

    String findNameSpaceDeclarationURI(String ns, Node el)
    {
        String ns_declaration = "xmlns:"+ns;

        Node parent = el.getParentNode();
        while (parent != null && parent.getNodeType() == Node.ELEMENT_NODE)
        {
            String declared_ns = ((Element)parent).getAttribute(ns_declaration);
            if (declared_ns != null && !declared_ns.isEmpty())
            {
                System.out.println("Found ns declaration: "+ns_declaration+"="+declared_ns);
                return declared_ns;
            }
            parent = parent.getParentNode();
        }
        return null;
    }

    void create_node(int start, int stop)
    {
        String name = buffer_.substring(start, stop);
        if (element_namespace_ != null)
        {
            // Simply retrofit the namespace. The jdk API is simpler than the libxml2 api.
            name = element_namespace_ + ":" + name;
            element_namespace_ = null;
        }

        if (name.equals("!DOCTYPE"))
        {
            parsing_doctype_ = true;
        }
        else if (name.charAt(0) == '?')
        {
            parsing_pi_ = true;
            pi_name_ = name.substring(1); // Drop the ?
        }
        else
        {
            Node new_node = doc_.createElement(name);
            Node parent = element_stack_.empty()?null:element_stack_.peek();
            element_last_ = new_node;

            if (parent != null)
            {
                parent.appendChild(new_node);
            }
            else
            {
                if (implicit_root_ == null || name.equals(implicit_root_))
                {
                    // There is no implicit root, or name is the same as the implicit root.
                    // Then create the root node with name.
                    doc_.appendChild(new_node);
                }
                else
                {
                    // We have an implicit root and it is different from name.
                    Node implicit_root = doc_.createElement(implicit_root_);
                    element_stack_.push(implicit_root);
                    implicit_root.appendChild(new_node);
                }
            }
        }
    }

    protected void do_whitespace(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        var p = QuoteUtil.findQuoteStartStop(buffer_, start, stop);
        String content = QuoteUtil.trimQuote(buffer_, p.left(), p.right());
        org.w3c.dom.Text text = doc_.createTextNode(content);
        element_last_.appendChild(text);
    }

    protected void do_element_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_element_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_attr_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_attr_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_element_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_element_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_attr_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_attr_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    String xmq_un_comment(int start, int stop)
    {
        return buffer_.substring(start, stop);
    }

    protected void do_comment(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        Node parent = element_stack_.empty()?null:element_stack_.peek();
        String trimmed = no_trim_quotes_?buffer_.substring(start, stop):xmq_un_comment(start, stop);
        Node n = doc_.createComment(trimmed);

        if (add_pre_node_before_ != null)
        {
            // Insert comment before this node.
            parent.insertBefore(n, add_pre_node_before_);
        }
        else if (add_post_node_after_ != null)
        {
            // Insert comment after this node.
            //parent.insertAfter(n, add_post_node_after_);
            parent.appendChild(n);
        }
        else
        {
            parent.appendChild(n);
        }
        element_last_ = n;
    }

    protected void do_comment_continuation(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
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

    protected void do_brace_left(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        element_stack_.push(element_last_);
    }

    protected void do_brace_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        element_stack_.pop();
    }

    protected void do_equals(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_attr_value_text(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_element_value_text(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        if (parsing_pi_)
        {
            /*
            String content = ""; // potentially_add_leading_ending_space(start, stop);
            ProcessingInstruction pi = doc_.createProcessingInstruction(name, content);
            Node n = doc_.(xmlNodePtr)xmlNewPI((xmlChar*)state->pi_name, (xmlChar*)content);
            xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
            xmlAddChild(parent, n);
            free(content);

            state->parsing_pi = false;
            free((char*)state->pi_name);
            state->pi_name = NULL;*/
        }
        else if (parsing_doctype_)
        {
            /*
        size_t l = stop-start;
        char *tmp = (char*)malloc(l+1);
        memcpy(tmp, start, l);
        tmp[l] = 0;
        state->doq->docptr_.xml->intSubset = xmlNewDtd(state->doq->docptr_.xml, (xmlChar*)tmp, NULL, NULL);
        xmlNodePtr n = (xmlNodePtr)state->doq->docptr_.xml->intSubset;
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, n);
        free(tmp);

        state->parsing_doctype = false;
        state->doctype_found = true;*/
        }
        else
        {
            org.w3c.dom.Text text = doc_.createTextNode(buffer_.substring(start,stop));
            element_last_.appendChild(text);
        }
    }

    protected void do_ns_declaration(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_attr_key(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_ns_colon(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_attr_ns(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

}
