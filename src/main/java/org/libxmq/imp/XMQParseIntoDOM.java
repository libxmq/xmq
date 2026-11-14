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

import java.util.ArrayList;
import java.util.List;

import org.libxmq.ParseErrorCode;
import org.libxmq.ParseException;
import org.libxmq.imp.xml.XmlAttr;
import org.libxmq.imp.xml.XmlComment;
import org.libxmq.imp.xml.XmlDocument;
import org.libxmq.imp.xml.XmlDtd;
import org.libxmq.imp.xml.XmlElement;
import org.libxmq.imp.xml.XmlEntityRef;
import org.libxmq.imp.xml.XmlNode;
import org.libxmq.imp.xml.XmlNodeType;
import org.libxmq.imp.xml.XmlNs;
import org.libxmq.imp.xml.XmlPI;
import org.libxmq.imp.xml.XmlParseException;
import org.libxmq.imp.xml.XmlParser;
import org.libxmq.imp.xml.XmlText;

/**
 * Parses xmq into the xmq DOM. This is a port of the parse callbacks in the
 * C version (xmq.c: create_node, do_quote, do_entity, do_attr_key ...), so
 * the resulting DOM is identical to the libxml2 DOM built by the C version.
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

    XmlDocument doc_ = new XmlDocument();
    List<XmlNode> element_stack_ = new ArrayList<>(); // Top is the current parent.
    XmlNode element_last_; // The last created node.
    boolean parsing_doctype_; // True when parsing a doctype.
    boolean doctype_found_; // True after a doctype has been parsed.
    boolean parsing_pi_; // True when parsing a processing instruction, pi.
    String pi_name_; // Name of the pi node just started.
    boolean no_trim_quotes_; // No trimming of quotes, used when reading json strings.
    boolean merge_text_ = true; // Merge adjacent text nodes, unless --nomerge.
    String element_namespace_; // The namespace prefix found before the element name.
    String attribute_namespace_; // The namespace prefix found before the attribute key.
    boolean declaring_xmlns_; // True when the next attribute value is a namespace uri.
    XmlNs declaring_xmlns_namespace_; // The namespace whose href is set by the next value.
    XmlNs default_namespace_; // The current default namespace declared with xmlns=uri.
    XmlNode add_pre_node_before_; // Insert the next comment/doctype before this node (json).
    XmlNode add_post_node_after_; // Insert the next comment after this node (json).

    /**
     * Returns the parsed document.
     * @return The parsed document.
     */
    public XmlDocument doc()
    {
        return doc_;
    }

    /**
     * Use an existing document to parse into.
     * @param doc The document.
     * @return This parser.
     */
    public XMQParseIntoDOM setDoc(XmlDocument doc)
    {
        doc_ = doc;
        return this;
    }

    /**
     * Set the implicit root element name. If the first element in the xmq
     * source has a different name, then the implicit root is created
     * to contain all the content.
     * @param name The root element name, or null.
     * @return This parser.
     */
    public XMQParseIntoDOM setImplicitRoot(String name)
    {
        implicit_root_ = (name == null || name.isEmpty()) ? null : name;
        return this;
    }

    /**
     * Control whether adjacent text nodes should be merged (the default)
     * or kept separate, like the --nomerge option.
     * @param merge True to merge.
     * @return This parser.
     */
    public XMQParseIntoDOM setMergeText(boolean merge)
    {
        merge_text_ = merge;
        return this;
    }

    /**
     * Do not trim the incidental indentation of quotes, used when the
     * quotes were generated from json strings.
     * @param no_trim True to keep quotes as is.
     * @return This parser.
     */
    public XMQParseIntoDOM setNoTrimQuotes(boolean no_trim)
    {
        no_trim_quotes_ = no_trim;
        return this;
    }

    @Override
    void setup()
    {
        element_stack_.clear();
        element_stack_.add(doc_);
        element_last_ = null;
    }

    XmlNode top()
    {
        return element_stack_.get(element_stack_.size() - 1);
    }

    void push(XmlNode n)
    {
        element_stack_.add(n);
    }

    XmlNode pop()
    {
        return element_stack_.remove(element_stack_.size() - 1);
    }

    /** The equivalent of xmq_un_quote(start, stop, true, true) on a quote token. */
    String un_quote(int start, int stop)
    {
        if (no_trim_quotes_)
        {
            return buffer_.substring(start, stop);
        }
        return XmqTrim.unQuote(buffer_, start, stop, true, true);
    }

    /** Add a text node, merging with a previous text node unless --nomerge. */
    XmlNode add_text(XmlNode parent, String content)
    {
        XmlText t = new XmlText(content);
        if (merge_text_)
        {
            return parent.addChild(t);
        }
        return parent.appendRaw(t);
    }

    static String potentially_add_leading_ending_space(String s)
    {
        if (s.isEmpty()) return s;
        boolean prefix = s.charAt(0) == '\'';
        boolean postfix = s.charAt(s.length() - 1) == '\'';
        if (!prefix && !postfix) return s;
        return (prefix ? " " : "") + s + (postfix ? " " : "");
    }

    void create_node(int start, int stop)
    {
        String name = buffer_.substring(start, stop);

        if (name.equals("!DOCTYPE"))
        {
            parsing_doctype_ = true;
            return;
        }
        if (!name.isEmpty() && name.charAt(0) == '?')
        {
            parsing_pi_ = true;
            pi_name_ = name.substring(1);
            return;
        }

        XmlElement new_node = new XmlElement(name);
        if (element_last_ == null)
        {
            if (implicit_root_ == null || name.equals(implicit_root_))
            {
                // There is no implicit root, or name is the same as the implicit root.
                element_last_ = new_node;
                doc_.setRootElement(new_node);
            }
            else
            {
                // We have an implicit root and it is different from name.
                XmlElement root = new XmlElement(implicit_root_);
                element_last_ = root;
                doc_.setRootElement(root);
                push(root);
            }
        }
        XmlNode parent = top();
        if (new_node.parent() == null) parent.addChild(new_node);

        if (element_namespace_ != null)
        {
            // Have a namespace before the element name, eg abc:work
            XmlNs ns = new_node.searchNs(element_namespace_);
            if (ns == null)
            {
                // The namespace does not yet exist. Lets hope it will be declared
                // inside the attributes of this node. Use a temporary href for now.
                ns = new_node.declareNs(element_namespace_, null);
            }
            new_node.setNs(ns);
            element_namespace_ = null;
        }
        else if (default_namespace_ != null)
        {
            new_node.setNs(default_namespace_);
        }
        element_last_ = new_node;
    }

    // Find a namespace declared on node or its ancestors, also finding the
    // not yet completed declarations without uri.
    static XmlNs find_ns(XmlNode node, String prefix)
    {
        for (XmlNode cur = node; cur != null; cur = cur.parent())
        {
            if (cur instanceof XmlElement e)
            {
                for (XmlNs ns : e.nsDefs())
                {
                    if (java.util.Objects.equals(prefix, ns.prefix())) return ns;
                }
            }
        }
        return null;
    }

    protected void do_whitespace(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    // Parse the embedded xml fragment and add its element to the current element.
    @Override
    void do_embedded_xml(int start, int stop)
    {
        XmlDocument fdoc;
        try
        {
            fdoc = XmlParser.parse(buffer_.substring(start, stop), source_, XmlParser.NOCDATA | XmlParser.NOBLANKS);
        }
        catch (XmlParseException e)
        {
            fdoc = null;
        }
        XmlElement node = fdoc == null ? null : fdoc.rootElement();
        if (node == null)
        {
            throw new XMQRuntimeException(new ParseException(ParseErrorCode.XMQ_ERROR_PARSING_XML,
                                                             buffer_.substring(line_start_, findNL(i_, 20)),
                                                             line_, col_, source_));
        }
        node.unlink();
        if (element_last_ == null)
        {
            if (implicit_root_ != null && !node.name().equals(implicit_root_))
            {
                XmlElement root = new XmlElement(implicit_root_);
                element_last_ = root;
                doc_.setRootElement(root);
                push(root);
            }
            else
            {
                element_last_ = node;
                doc_.setRootElement(node);
            }
        }
        if (node.parent() == null) top().addChild(node);
        element_last_ = node;
    }

    protected void do_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        element_last_ = add_text(top(), un_quote(start, stop));
    }

    protected void do_element_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        String trimmed = un_quote(start, stop);
        if (parsing_pi_)
        {
            top().addChild(new XmlPI(pi_name_, potentially_add_leading_ending_space(trimmed)));
            parsing_pi_ = false;
            pi_name_ = null;
        }
        else if (parsing_doctype_)
        {
            String buf = "<!DOCTYPE " + trimmed + "><foo></foo>";
            XmlDtd dtd = null;
            try
            {
                XmlDocument d = XmlParser.parse(buf, "noname.xml", 0);
                dtd = d.intSubset();
            }
            catch (XmlParseException e)
            {
                System.err.print(e.getMessage());
            }
            if (dtd == null)
            {
                throw new XMQRuntimeException(new ParseException(ParseErrorCode.XMQ_ERROR_BAD_DOCTYPE,
                                                                 buffer_.substring(line_start_, findNL(i_, 20)),
                                                                 line_, col_, source_));
            }
            dtd.unlink();
            if (add_pre_node_before_ != null) add_pre_node_before_.addPrevSibling(dtd);
            else top().appendRaw(dtd);
            doc_.setIntSubsetRef(dtd);
            parsing_doctype_ = false;
            doctype_found_ = true;
        }
        else
        {
            add_text(element_last_, trimmed);
        }
    }

    protected void do_element_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        do_quote(start_line, start_col, start, stop, stop_suffix);
    }

    protected void do_attr_value_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        String trimmed = un_quote(start, stop);
        if (declaring_xmlns_)
        {
            if (declaring_xmlns_namespace_ != null)
            {
                update_namespace_href(declaring_xmlns_namespace_, trimmed);
            }
            declaring_xmlns_ = false;
            declaring_xmlns_namespace_ = null;
            return;
        }
        add_text(element_last_, trimmed);
    }

    protected void do_attr_value_compound_quote(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        do_quote(start_line, start_col, start, stop, stop_suffix);
    }

    XmlNode create_entity(int start, int stop, XmlNode parent)
    {
        String tmp = buffer_.substring(start, stop);
        XmlNode n;
        if (tmp.length() > 1 && tmp.charAt(1) == '#')
        {
            // Character entity.
            long uc;
            try
            {
                if (tmp.charAt(2) == 'x') uc = Long.parseLong(tmp.substring(3, tmp.length() - 1), 16);
                else uc = Long.parseLong(tmp.substring(2, tmp.length() - 1), 10);
            }
            catch (NumberFormatException e)
            {
                uc = -1;
            }
            if (uc < 0 || uc > 0x10ffff)
            {
                throw new XMQRuntimeException(new ParseException(ParseErrorCode.XMQ_ERROR_INVALID_CHAR,
                                                                 buffer_.substring(line_start_, findNL(i_, 20)),
                                                                 line_, col_, source_));
            }
            String ch = new String(Character.toChars((int)uc));
            if (!merge_text_)
            {
                // Do not merge with surrounding text, keep the character reference.
                n = new XmlEntityRef(tmp.substring(1, tmp.length() - 1));
                return parent.appendRaw(n);
            }
            return parent.addChild(new XmlText(ch));
        }
        // Named references are kept as is. Like xmlNewReference, drop the & and a
        // trailing ; if present (&ff needs no ; and a lone & gives an empty name).
        String name = tmp.substring(1);
        if (name.endsWith(";")) name = name.substring(0, name.length() - 1);
        n = new XmlEntityRef(name);
        return parent.appendRaw(n);
    }

    protected void do_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        element_last_ = create_entity(start, stop, top());
    }

    protected void do_element_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        create_entity(start, stop, element_last_);
    }

    protected void do_element_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        do_entity(start_line, start_col, start, stop, stop_suffix);
    }

    protected void do_attr_value_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        create_entity(start, stop, element_last_);
    }

    protected void do_attr_value_compound_entity(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        do_entity(start_line, start_col, start, stop, stop_suffix);
    }

    protected void do_comment(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        String trimmed = no_trim_quotes_ ? buffer_.substring(start, stop) : xmq_un_comment(start, stop);
        XmlComment c = new XmlComment(trimmed);
        if (add_pre_node_before_ != null) add_pre_node_before_.addPrevSibling(c);
        else if (add_post_node_after_ != null) add_post_node_after_.addNextSibling(c);
        else top().appendRaw(c);
        element_last_ = c;
    }

    protected void do_comment_continuation(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        // We have ///* alfa *///* beta *///* gamma *///
        // and this function is invoked with "* beta *///"
        int k = stop - 1;
        int n = 0;
        while (k > start && buffer_.charAt(k) == '/')
        {
            n++;
            k--;
        }
        String trimmed = xmq_un_comment(start - n, stop);
        XmlNode last = element_last_;
        if (last != null)
        {
            String old = last.content() == null ? "" : last.content();
            last.setContent(old + "\n" + trimmed);
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
        push(element_last_);
    }

    protected void do_apar_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        element_last_ = pop();
    }

    protected void do_cpar_left(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        push(element_last_);
    }

    protected void do_cpar_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        element_last_ = pop();
    }

    protected void do_brace_left(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        push(element_last_);
    }

    protected void do_brace_right(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        element_last_ = pop();
    }

    protected void do_equals(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_equals_done(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    void update_namespace_href(XmlNs ns, String href)
    {
        ns.setHref(href);
        if (href.isEmpty() && ns == default_namespace_)
        {
            XmlNode element = top();
            if (element instanceof XmlElement e)
            {
                e.setNs(null);
            }
            default_namespace_ = null;
        }
    }

    protected void do_attr_value_text(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        if (declaring_xmlns_)
        {
            if (declaring_xmlns_namespace_ != null)
            {
                update_namespace_href(declaring_xmlns_namespace_, buffer_.substring(start, stop));
            }
            declaring_xmlns_ = false;
            declaring_xmlns_namespace_ = null;
            return;
        }
        add_text(element_last_, buffer_.substring(start, stop));
    }

    protected void do_element_value_text(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        String text = buffer_.substring(start, stop);
        if (parsing_pi_)
        {
            top().addChild(new XmlPI(pi_name_, potentially_add_leading_ending_space(text)));
            parsing_pi_ = false;
            pi_name_ = null;
        }
        else if (parsing_doctype_)
        {
            XmlDtd dtd = new XmlDtd(text, null, null);
            top().appendRaw(dtd);
            doc_.setIntSubsetRef(dtd);
            parsing_doctype_ = false;
            doctype_found_ = true;
        }
        else
        {
            add_text(element_last_, text);
        }
    }

    protected void do_ns_declaration(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        // We found a namespace declaration attribute. It is either a default
        // declaration xmlns=... or xmlns:prefix=... We can see the difference
        // since stop_suffix is either stop (xmlns=) or after stop (xmlns:foo=).
        XmlNode n = top();
        if (!(n instanceof XmlElement element))
        {
            // There is no element node, eg a processing instruction element.
            declaring_xmlns_ = true;
            declaring_xmlns_namespace_ = null;
            return;
        }
        XmlNs ns;
        if (stop == stop_suffix)
        {
            // A default namespace, eg: xmlns=uri
            ns = null;
            for (XmlNs d : element.nsDefs())
            {
                if (d.prefix() == null)
                {
                    ns = d;
                    break;
                }
            }
            if (ns == null) ns = element.declareNs(null, null);
            if (element.ns() == null)
            {
                element.setNs(ns);
            }
            default_namespace_ = ns;
        }
        else
        {
            // A namespace with a prefix: xmlns:prefix=uri
            String name = buffer_.substring(stop + 1, stop_suffix);
            ns = find_ns(element, name);
            if (ns == null && name.equals("xml")) ns = XmlElement.XML_NS;
            if (ns == null)
            {
                ns = element.declareNs(name, null);
            }
        }
        declaring_xmlns_ = true;
        declaring_xmlns_namespace_ = ns;
    }

    protected void do_attr_key(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        String key = buffer_.substring(start, stop);
        XmlNode parent = top();
        if (!(parent instanceof XmlElement element))
        {
            // There is no element node, eg a processing instruction with
            // attributes. The attribute is created but not attached.
            element_last_ = new XmlAttr(key, null);
            attribute_namespace_ = null;
            return;
        }
        XmlAttr attr = new XmlAttr(key, null);
        if (attribute_namespace_ != null)
        {
            XmlNs ns = find_ns(element, attribute_namespace_);
            if (ns == null && attribute_namespace_.equals("xml")) ns = XmlElement.XML_NS;
            if (ns == null)
            {
                // The namespace does not yet exist. Lets hope it will be declared
                // inside the attributes of this node. Use a temporary href for now.
                ns = element.declareNs(attribute_namespace_, null);
            }
            attr.setNs(ns);
            attribute_namespace_ = null;
        }
        element.addAttribute(attr);
        element_last_ = attr;
    }

    protected void do_ns_colon(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
    }

    protected void do_attr_ns(int start_line, int start_col, int start, int stop, int stop_suffix)
    {
        if (!declaring_xmlns_)
        {
            // Normal attribute namespace found before the attribute key, eg x:alfa=123
            attribute_namespace_ = buffer_.substring(start, stop);
        }
    }
}
