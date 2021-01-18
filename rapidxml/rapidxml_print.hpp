#ifndef RAPIDXML_PRINT_HPP_INCLUDED
#define RAPIDXML_PRINT_HPP_INCLUDED

// Copyright (C) 2006, 2009 Marcin Kalicinski
// Version 1.13
// Revision $DateTime: 2009/05/13 01:46:17 $
//! \file rapidxml_print.hpp This file contains rapidxml printer implementation

#include "rapidxml.hpp"

// Only include streams if not disabled
#ifndef RAPIDXML_NO_STREAMS
    #include <ostream>
    #include <iterator>
#endif

namespace rapidxml
{

    ///////////////////////////////////////////////////////////////////////
    // Printing flags

    const int print_no_indenting = 0x1;   //!< Printer flag instructing the printer to suppress indenting of XML. See print() function.
    const int no_final_newline = 0x2;     // Do not print the final newline
    const int print_html = 0x4;           // Translate attr alfa="alfa" into just alfa. Also void elements are printed without ending /.

    ///////////////////////////////////////////////////////////////////////
    // Internal

    //! \cond internal
    namespace internal
    {
        ///////////////////////////////////////////////////////////////////////////
        // Internal character operations

        // Copy characters from given range to given output iterator
        template<class OutIt, class Ch>
        inline OutIt copy_chars(const Ch *begin, const Ch *end, OutIt out)
        {
            while (begin != end)
                *out++ = *begin++;
            return out;
        }

        // Copy characters from given range to given output iterator and expand
        // characters into references (&lt; &gt; &apos; &quot; &amp;)
        template<class OutIt, class Ch>
        inline OutIt copy_and_expand_chars(const Ch *begin, const Ch *end, Ch noexpand, OutIt out)
        {
            while (begin != end)
            {
                if (*begin == noexpand)
                {
                    *out++ = *begin;    // No expansion, copy character
                }
                else if (noexpand == -1) // Only expand the dangerous: & < >
                {
                    switch (*begin)
                    {
                    case Ch('<'):
                        *out++ = Ch('&'); *out++ = Ch('l'); *out++ = Ch('t'); *out++ = Ch(';');
                        break;
                    case Ch('>'):
                        *out++ = Ch('&'); *out++ = Ch('g'); *out++ = Ch('t'); *out++ = Ch(';');
                        break;
                    case Ch('&'):
                        *out++ = Ch('&'); *out++ = Ch('a'); *out++ = Ch('m'); *out++ = Ch('p'); *out++ = Ch(';');
                        break;
                    default:
                        *out++ = *begin;    // No expansion, copy character
                    }
                }
                else
                {
                    switch (*begin)
                    {
                    case Ch('<'):
                        *out++ = Ch('&'); *out++ = Ch('l'); *out++ = Ch('t'); *out++ = Ch(';');
                        break;
                    case Ch('>'):
                        *out++ = Ch('&'); *out++ = Ch('g'); *out++ = Ch('t'); *out++ = Ch(';');
                        break;
                    case Ch('\''):
                        *out++ = Ch('&'); *out++ = Ch('a'); *out++ = Ch('p'); *out++ = Ch('o'); *out++ = Ch('s'); *out++ = Ch(';');
                        break;
                    case Ch('"'):
                        *out++ = Ch('&'); *out++ = Ch('q'); *out++ = Ch('u'); *out++ = Ch('o'); *out++ = Ch('t'); *out++ = Ch(';');
                        break;
                    case Ch('&'):
                        *out++ = Ch('&'); *out++ = Ch('a'); *out++ = Ch('m'); *out++ = Ch('p'); *out++ = Ch(';');
                        break;
                    case Ch('\n'):
                        *out++ = Ch('&'); *out++ = Ch('#'); *out++ = Ch('1'); *out++ = Ch('0'); *out++ = Ch(';');
                        break;
                    default:
                        *out++ = *begin;    // No expansion, copy character
                    }
                }
                ++begin;    // Step to next character
            }
            return out;
        }

        // Fill given output iterator with repetitions of the same character
        template<class OutIt, class Ch>
        inline OutIt fill_chars(OutIt out, int n, Ch ch)
        {
            if (ch == (Ch)'\t')
            {
                // Do not indent with tabs, instead use 2 spaces for each tab.
                // This is consistent with the pom.xml standard.
                int nn = n*2;
                for (int i = 0; i < nn; ++i)
                    *out++ = ' ';
                return out;
            }
            for (int i = 0; i < n; ++i)
                *out++ = ch;
            return out;
        }

        // Find character
        template<class Ch, Ch ch>
        inline bool find_char(const Ch *begin, const Ch *end)
        {
            while (begin != end)
                if (*begin++ == ch)
                    return true;
            return false;
        }

        ///////////////////////////////////////////////////////////////////////////
        // HTML helpers

        template<class Ch>Ch locase(const Ch c)
        {
            if (c >= 65 && c <= 90) return c+32;
            return c;
        }

        template<class Ch>bool is_equal(const Ch *tag, size_t len, Ch *&text, size_t text_len)
        {
            if (len > text_len) return false;
            size_t i;
            for (i=0; i<len; ++i)
            {
                if (locase(text[i]) != tag[i]) {
                    return false;
                }
            }
            if (i == text_len) return true;
            if (i < text_len)
            {
                if (text[i] == 0 || text[i] == ' ' || text[i] == '>')
                {
                    return true;
                }
            }
            return false;
        }


#define EQ(s, t, tl) is_equal(s, strlen(s), t, tl)

        template<class Ch>bool is_void_element(Ch *text, size_t tl)
        {
            // area, base, br, col, command, embed, hr, img, input, keygen, link, meta, param, source, track, wbr
            if (EQ("area", text, tl)) return true;
            if (EQ("base", text, tl)) return true;
            if (EQ("br", text, tl)) return true;
            if (EQ("col", text, tl)) return true;
            if (EQ("command", text, tl)) return true;
            if (EQ("embed", text, tl)) return true;
            if (EQ("hr", text, tl)) return true;
            if (EQ("img", text, tl)) return true;
            if (EQ("input", text, tl)) return true;
            if (EQ("keygen", text, tl)) return true;
            if (EQ("link", text, tl)) return true;
            if (EQ("meta", text, tl)) return true;
            if (EQ("param", text, tl)) return true;
            if (EQ("source", text, tl)) return true;
            if (EQ("track", text, tl)) return true;
            if (EQ("wbr", text, tl)) return true;
            return false;
        }

        template<class Ch>bool is_inline_element(Ch *text, size_t tl)
        {
            // a abbr acronym b bdo big br button cite code dfn em i img input
            // kbd label map object output q samp script select small span strong sub sup textarea time tt var
            if (EQ("a", text, tl)) return true;
            if (EQ("abbr", text, tl)) return true;
            if (EQ("acronym", text, tl)) return true;
            if (EQ("b", text, tl)) return true;
            if (EQ("bdo", text, tl)) return true;
            if (EQ("big", text, tl)) return true;
            if (EQ("br", text, tl)) return true;
            if (EQ("button", text, tl)) return true;
            if (EQ("cite", text, tl)) return true;
            if (EQ("code", text, tl)) return true;
            if (EQ("dfn", text, tl)) return true;
            if (EQ("em", text, tl)) return true;
            if (EQ("i", text, tl)) return true;
            if (EQ("img", text, tl)) return true;
            if (EQ("input", text, tl)) return true;
            if (EQ("kbd", text, tl)) return true;
            if (EQ("label", text, tl)) return true;
            if (EQ("map", text, tl)) return true;
            if (EQ("object", text, tl)) return true;
            if (EQ("output", text, tl)) return true;
            if (EQ("q", text, tl)) return true;
            if (EQ("samp", text, tl)) return true;
            if (EQ("script", text, tl)) return true;
            if (EQ("select", text, tl)) return true;
            if (EQ("small", text, tl)) return true;
            if (EQ("span", text, tl)) return true;
            if (EQ("strong", text, tl)) return true;
            if (EQ("sub", text, tl)) return true;
            if (EQ("sup", text, tl)) return true;
            if (EQ("textarea", text, tl)) return true;
            if (EQ("time", text, tl)) return true;
            if (EQ("tt", text, tl)) return true;
            return false;
        }

        ///////////////////////////////////////////////////////////////////////////
        // Internal printing operations

        template<class OutIt, class Ch>
        inline OutIt print_children(OutIt out, const xml_node<Ch> *node, int flags, int indent,
                                    const xml_node<Ch> *prev, const xml_node<Ch> **last);

        template<class OutIt, class Ch>
        inline OutIt print_attributes(OutIt out, const xml_node<Ch> *node, int flags);

        template<class OutIt, class Ch>
        inline OutIt print_data_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev);

        template<class OutIt, class Ch>
        inline OutIt print_cdata_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev);

        template<class OutIt, class Ch>
        inline OutIt print_element_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev);

        template<class OutIt, class Ch>
        inline OutIt print_declaration_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev);

        template<class OutIt, class Ch>
        inline OutIt print_comment_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev);

        template<class OutIt, class Ch>
        inline OutIt print_doctype_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev);

        template<class OutIt, class Ch>
        inline OutIt print_pi_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev);

        // Print node
        template<class OutIt, class Ch>
        inline OutIt print_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev)
        {
            bool print_newline = true;
            if (flags & no_final_newline)
            {
                print_newline = false;
                flags = flags & ~(no_final_newline);
            }

            xml_node<Ch> *next = NULL;
            if (node->parent())
            {
                next = node->next_sibling();
            }

            if (next && next->type() == node_data)
            {
                print_newline = false;
            }

            // Print proper node type
            switch (node->type())
            {

            // Document
            case node_document:
                out = print_children(out, node, flags, indent, prev, (const xml_node<Ch> **)0);
                break;

            // Element
            case node_element:
                out = print_element_node(out, node, flags, indent, prev);
                break;

            // Data
            case node_data:
                print_newline = false;
                out = print_data_node(out, node, flags, indent, prev);
                break;

            // CDATA
            case node_cdata:
                out = print_cdata_node(out, node, flags, indent, prev);
                break;

            // Declaration
            case node_declaration:
                out = print_declaration_node(out, node, flags, indent, prev);
                break;

            // Comment
            case node_comment:
                out = print_comment_node(out, node, flags, indent, prev);
                break;

            // Doctype
            case node_doctype:
                out = print_doctype_node(out, node, flags, indent, prev);
                break;

            // Pi
            case node_pi:
                out = print_pi_node(out, node, flags, indent, prev);
                break;

                // Unknown
            default:
                assert(0);
                break;
            }

            if (print_newline)
            {
                if ((flags & print_html) && is_inline_element(node->name(), node->name_size()))
                {
                    // To prevent introduction of spurios whitespace in the rendered
                    // html output, it is important not to generate: ...</span><span>...
                    // Instead it must be ...</span><span>...
                    print_newline = false;
                }
            }

            // If indenting not disabled, add line break after node
            if (!(flags & print_no_indenting) && print_newline)
            {
                *out = Ch('\n');
                ++out;
            }

            // Return modified iterator
            return out;
        }

        // Print children of the node
        template<class OutIt, class Ch>
        inline OutIt print_children(OutIt out, const xml_node<Ch> *node, int flags, int indent,
                                    const xml_node<Ch> *prevv, const xml_node<Ch> **last)
        {
            const xml_node<Ch> *prev = NULL;
            for (xml_node<Ch> *child = node->first_node(); child; child = child->next_sibling())
            {
                if (prev && prev->type() == node_data &&
                     child && child->type() == node_data)
                {
                    // The previous and this node are data nodes!
                    // Force a newline in between!
                    *out = Ch('\n');
                    ++out;
                }
                out = print_node(out, child, flags, indent, prev);
                prev = child;
            }
            if (last != NULL)
            {
                *last = prev;
            }
            return out;
        }

        // Print attributes of the node
        template<class OutIt, class Ch>
        inline OutIt print_attributes(OutIt out, const xml_node<Ch> *node, int flags)
        {
            for (xml_attribute<Ch> *attribute = node->first_attribute(); attribute; attribute = attribute->next_attribute())
            {
                if (attribute->name() && attribute->value())
                {
                    // Print attribute name
                    *out = Ch(' '), ++out;
                    out = copy_chars(attribute->name(), attribute->name() + attribute->name_size(), out);
                    bool print_equals = true;
                    if (flags & print_html)
                    {
                        if (attribute->value_size() == attribute->name_size() &&
                            !strncmp(attribute->name(), attribute->value(), attribute->value_size()))
                        {
                            // We have found alfa="alfa" and we are generating html.
                            // Skip printing ="alfa" only alfa remains.
                            print_equals = false;
                        }
                    }

                    if (print_equals)
                    {
                        *out = Ch('='), ++out;
                        // Print attribute value using appropriate quote type
                        if (find_char<Ch, Ch('"')>(attribute->value(), attribute->value() + attribute->value_size()))
                        {
                            *out = Ch('\''), ++out;
                            out = copy_and_expand_chars(attribute->value(), attribute->value() + attribute->value_size(), Ch('"'), out);
                            *out = Ch('\''), ++out;
                        }
                        else
                        {
                            *out = Ch('"'), ++out;
                            out = copy_and_expand_chars(attribute->value(), attribute->value() + attribute->value_size(), Ch('\''), out);
                            *out = Ch('"'), ++out;
                        }
                    }
                }
            }
            return out;
        }

        // Print data node
        template<class OutIt, class Ch>
        inline OutIt print_data_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev)
        {
            assert(node->type() == node_data);
            /* Never indent data nodes.
            if (!(flags & print_no_indenting))
            out = fill_chars(out, indent, Ch('\t'));*/
            out = copy_and_expand_chars(node->value(), node->value() + node->value_size(), Ch(-1), out);
            return out;
        }

        // Print data node
        template<class OutIt, class Ch>
        inline OutIt print_cdata_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev)
        {
            assert(node->type() == node_cdata);
            if (!(flags & print_no_indenting))
            {
                // Indent
                out = fill_chars(out, indent, Ch('\t'));
            }
            *out = Ch('<'); ++out;
            *out = Ch('!'); ++out;
            *out = Ch('['); ++out;
            *out = Ch('C'); ++out;
            *out = Ch('D'); ++out;
            *out = Ch('A'); ++out;
            *out = Ch('T'); ++out;
            *out = Ch('A'); ++out;
            *out = Ch('['); ++out;
            out = copy_chars(node->value(), node->value() + node->value_size(), out);
            *out = Ch(']'); ++out;
            *out = Ch(']'); ++out;
            *out = Ch('>'); ++out;
            return out;
        }

        // Print element node
        template<class OutIt, class Ch>
        inline OutIt print_element_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev)
        {
            assert(node->type() == node_element);

            bool print_indent = true;

            if ((flags & print_html) && is_inline_element(node->name(), node->name_size()))
            {
                print_indent = false;
            }

            if (prev && (flags & print_html) && is_inline_element(prev->name(), prev->name_size()))
            {
                print_indent = false;
            }

            // Print element name and attributes, if any
            if (!(flags & print_no_indenting) && print_indent)
            {
                // indent
                if (prev == NULL || (prev && prev->type() != node_data))
                {
                    // Only indent if previous node is not data.
                    out = fill_chars(out, indent, Ch('\t'));
                }
            }
            *out = Ch('<'), ++out;
            out = copy_chars(node->name(), node->name() + node->name_size(), out);
            out = print_attributes(out, node, flags);

            // If node is childless
            if (node->value_size() == 0 && !node->first_node())
            {
                if (flags & print_html)
                {
                    if (is_void_element(node->name(), node->name_size()))
                    {
                        // Print childless node tag ending
                        *out = Ch('/'), ++out;
                        *out = Ch('>'), ++out;
                    }
                    else
                    {
                        *out = Ch('>'), ++out;
                        *out = Ch('<'), ++out;
                        *out = Ch('/'), ++out;
                        out = copy_chars(node->name(), node->name() + node->name_size(), out);
                        *out = Ch('>'), ++out;
                    }
                }
                else
                {
                    // Print childless node tag ending
                    *out = Ch('/'), ++out;
                    *out = Ch('>'), ++out;
                }
            }
            else
            {
                // Print normal node tag ending
                *out = Ch('>'), ++out;

                // Test if node contains a single data node only (and no other nodes)
                xml_node<Ch> *child = node->first_node();
                if (!child)
                {
                    // If node has no children, only print its value without indenting
                    out = copy_and_expand_chars(node->value(), node->value() + node->value_size(), Ch(-1), out);
                }
                else if (child->next_sibling() == 0 && child->type() == node_data)
                {
                    // If node has a sole data child, only print its value without indenting
                    out = copy_and_expand_chars(child->value(), child->value() + child->value_size(), Ch(-1), out);
                }
                else
                {
                    bool print_newline_indent = true;

                    if ((flags & print_html) && is_inline_element(node->name(), node->name_size()))
                    {
                        // To prevent introduction of spurios whitespace in the rendered
                        // html output, it is important not to generate: ...</span><span>...
                        // Instead it must be ...</span><span>...
                        print_newline_indent = false;
                    }

                    // Print all children with full indenting
                    if (!(flags & print_no_indenting) && print_newline_indent)
                    {
                        if (node->first_node() && node->first_node()->type() != node_data)
                        {
                            *out = Ch('\n');
                            ++out;
                        }
                    }

                    const xml_node<Ch> *last;
                    out = print_children(out, node, flags, indent + 1, node, &last);

                    if (!(flags & print_no_indenting) && print_newline_indent)
                    {
                        // Indent
                        if (last && last->type() != node_data)
                        {
                            out = fill_chars(out, indent, Ch('\t'));
                        }
                    }
                }

                // Print node end
                *out = Ch('<'), ++out;
                *out = Ch('/'), ++out;
                out = copy_chars(node->name(), node->name() + node->name_size(), out);
                *out = Ch('>'), ++out;
            }
            return out;
        }

        // Print declaration node
        template<class OutIt, class Ch>
        inline OutIt print_declaration_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev)
        {
            // Print declaration start
            if (!(flags & print_no_indenting))
            {
                // Indent
                out = fill_chars(out, indent, Ch('\t'));
            }
            *out = Ch('<'), ++out;
            *out = Ch('?'), ++out;
            *out = Ch('x'), ++out;
            *out = Ch('m'), ++out;
            *out = Ch('l'), ++out;

            // Print attributes
            out = print_attributes(out, node, flags);

            // Print declaration end
            *out = Ch('?'), ++out;
            *out = Ch('>'), ++out;

            return out;
        }

        // Print comment node
        template<class OutIt, class Ch>
        inline OutIt print_comment_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev)
        {
            assert(node->type() == node_comment);
            if (!(flags & print_no_indenting))
            {
                // Indent
                out = fill_chars(out, indent, Ch('\t'));
            }
            *out = Ch('<'), ++out;
            *out = Ch('!'), ++out;
            *out = Ch('-'), ++out;
            *out = Ch('-'), ++out;
            out = copy_chars(node->value(), node->value() + node->value_size(), out);
            *out = Ch('-'), ++out;
            *out = Ch('-'), ++out;
            *out = Ch('>'), ++out;
            return out;
        }

        // Print doctype node
        template<class OutIt, class Ch>
        inline OutIt print_doctype_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev)
        {
            assert(node->type() == node_doctype);
            if (!(flags & print_no_indenting))
            {
                // Indent
                out = fill_chars(out, indent, Ch('\t'));
            }
            *out = Ch('<'), ++out;
            *out = Ch('!'), ++out;
            *out = Ch('D'), ++out;
            *out = Ch('O'), ++out;
            *out = Ch('C'), ++out;
            *out = Ch('T'), ++out;
            *out = Ch('Y'), ++out;
            *out = Ch('P'), ++out;
            *out = Ch('E'), ++out;
            *out = Ch(' '), ++out;
            out = copy_chars(node->value(), node->value() + node->value_size(), out);
            *out = Ch('>'), ++out;
            return out;
        }

        // Print pi node
        template<class OutIt, class Ch>
        inline OutIt print_pi_node(OutIt out, const xml_node<Ch> *node, int flags, int indent, const xml_node<Ch> *prev)
        {
            assert(node->type() == node_pi);
            if (!(flags & print_no_indenting))
            {
                // Indent
                out = fill_chars(out, indent, Ch('\t'));
            }
            *out = Ch('<'), ++out;
            *out = Ch('?'), ++out;
            out = copy_chars(node->name(), node->name() + node->name_size(), out);
            *out = Ch(' '), ++out;
            out = copy_chars(node->value(), node->value() + node->value_size(), out);
            *out = Ch('?'), ++out;
            *out = Ch('>'), ++out;
            return out;
        }

    }
    //! \endcond

    ///////////////////////////////////////////////////////////////////////////
    // Printing

    //! Prints XML to given output iterator.
    //! \param out Output iterator to print to.
    //! \param node Node to be printed. Pass xml_document to print entire document.
    //! \param flags Flags controlling how XML is printed.
    //! \return Output iterator pointing to position immediately after last character of printed text.
    template<class OutIt, class Ch>
    inline OutIt print(OutIt out, const xml_node<Ch> &node, int flags = 0, const xml_node<Ch> *prev = NULL)
    {
        return internal::print_node(out, &node, flags | no_final_newline, 0, prev);
    }

#ifndef RAPIDXML_NO_STREAMS

    //! Prints XML to given output stream.
    //! \param out Output stream to print to.
    //! \param node Node to be printed. Pass xml_document to print entire document.
    //! \param flags Flags controlling how XML is printed.
    //! \return Output stream.
    template<class Ch>
    inline std::basic_ostream<Ch> &print(std::basic_ostream<Ch> &out, const xml_node<Ch> &node, int flags = 0)
    {
        print(std::ostream_iterator<Ch>(out), node, flags);
        return out;
    }

    //! Prints formatted XML to given output stream. Uses default printing flags. Use print() function to customize printing process.
    //! \param out Output stream to print to.
    //! \param node Node to be printed.
    //! \return Output stream.
    template<class Ch>
    inline std::basic_ostream<Ch> &operator <<(std::basic_ostream<Ch> &out, const xml_node<Ch> &node)
    {
        return print(out, node);
    }

#endif

}

#endif
