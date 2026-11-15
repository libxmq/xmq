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

#ifndef XML_DOM_H
#define XML_DOM_H

/*
    The native xmq DOM. It replaces the libxml2 tree (xmlDoc/xmlNode/xmlAttr/xmlNs/xmlDtd).

    All data structures are opaque: they are defined in xml_dom.c only and are
    accessed through the functions below. The node type is the struct XMQNode
    that xmq.h declares as opaque and the attribute type is struct XMQAttr.
    An attribute is also a node: xml_attr_node/xml_node_as_attr convert between
    the two (xpath node sets contain attributes as nodes). The DTD, entity
    declarations and element/attribute/notation declarations are nodes too.

    All char* are UTF-8 encoded and zero terminated unless a length is given.
    The parsers validate and convert input, the dom assumes valid UTF-8.

    Ownership rules:
    - A tree is owned by an XmlDom. Every node belongs to a dom.
    - All strings are copied into the dom, returned const char* point into the
      dom and are valid until the node is changed or freed. Functions returning
      char* return malloced strings that the caller frees.
    - Namespaces (XmlNs) are reference counted. An element owns the namespaces
      declared on it and every node using a namespace holds a reference to it.
      Thus moving or copying subtrees never leaves dangling namespace pointers.
      After a move/copy, call xml_node_reconcile_ns to make sure all used
      namespaces are also declared in scope.
    - xml_node_add_child merges adjacent text nodes (like libxml2 xmlAddChild),
      in which case the added node is freed and the existing node is returned.
      Use xml_node_append_child to never merge.
    - An attribute value is stored as children of the attribute: text nodes and
      entity references (like libxml2), since xmq keeps &entity; references
      also inside attribute values.
    - An entity reference has no content of its own and, since entities are
      never expanded implicitly, contributes nothing to string values. A
      character reference kept as a reference node (name "#10" or "#x41", xmq
      can keep them) is a character: it contributes the character.
    - The DTD (doctype) is a child of the document node. Its children are the
      entity/element/attribute declarations (and comments/pis) in source order.
      Notations are kept in a separate list, as libxml2 does, since they are
      serialized first.

    There is a single dom for xml and html: html is only a choice of parser and serializer,
    never a different node or document type.

*/

#include"xmq.h"

#include<stdbool.h>
#include<stddef.h>
#include<stdint.h>

typedef enum
{
    XML_DOM_ELEMENT = 1,
    XML_DOM_ATTRIBUTE = 2,
    XML_DOM_TEXT = 3,
    XML_DOM_CDATA = 4,
    XML_DOM_ENTITY_REF = 5,
    XML_DOM_PI = 7,
    XML_DOM_COMMENT = 8,
    XML_DOM_DOCUMENT = 9,
    XML_DOM_DTD = 14,
    XML_DOM_ELEMENT_DECL = 15,
    XML_DOM_ATTRIBUTE_DECL = 16,
    XML_DOM_ENTITY_DECL = 17,
    /** An xpath namespace node (namespace:: axis). Not part of any tree: its parent is the
        element it belongs to, but it is not one of the element's children. */
    XML_DOM_NAMESPACE = 18,
    XML_DOM_NOTATION_DECL = 20,
    /** A parameter entity reference %name; between declarations in the dtd (never expanded). */
    XML_DOM_PE_REF = 21,
} XmlNodeKind;

typedef enum
{
    XML_ENTITY_INTERNAL_GENERAL = 1,
    XML_ENTITY_EXTERNAL_GENERAL_PARSED = 2,
    XML_ENTITY_EXTERNAL_GENERAL_UNPARSED = 3,
    XML_ENTITY_INTERNAL_PARAMETER = 4,
    XML_ENTITY_EXTERNAL_PARAMETER = 5,
} XmlEntityType;

/** A document tree. */
struct XmlDom;
typedef struct XmlDom XmlDom;

/** A namespace binding prefix -> href. */
struct XmlNs;
typedef struct XmlNs XmlNs;

#define XML_DOM_XML_NAMESPACE "http://www.w3.org/XML/1998/namespace"

// Dom ////////////////////////////////////////////////////////////

XmlDom *xml_dom_new();
void xml_dom_free(XmlDom *dom);
/** The document node, kind XML_DOM_DOCUMENT. Its children are the top level nodes. */
XMQNode *xml_dom_document(XmlDom *dom);
/** The first element child of the document node, or NULL. */
XMQNode *xml_dom_root_element(XmlDom *dom);
/** Replace the root element (the old one is freed). */
void xml_dom_set_root_element(XmlDom *dom, XMQNode *root);
/** Version from the xml declaration, eg "1.0". NULL if none. */
const char *xml_dom_version(const XmlDom *dom);
void xml_dom_set_version(XmlDom *dom, const char *version);
/** Encoding from the xml declaration. NULL if none. */
const char *xml_dom_encoding(const XmlDom *dom);
/** Where the document was loaded from (libxml2 doc->URL), or NULL. */
const char *xml_dom_url(const XmlDom *dom);
void xml_dom_set_url(XmlDom *dom, const char *url);
void xml_dom_set_encoding(XmlDom *dom, const char *encoding);
/** Standalone from the xml declaration: -1 not given, 0 no, 1 yes. */
int xml_dom_standalone(const XmlDom *dom);
void xml_dom_set_standalone(XmlDom *dom, int standalone);

// DTD ////////////////////////////////////////////////////////////

/** Get the DTD (doctype) node, kind XML_DOM_DTD, or NULL. */
XMQNode *xml_dom_dtd(XmlDom *dom);
/** Create (or replace) the DTD of the dom. It is inserted before the root element, or last. */
XMQNode *xml_dom_set_dtd(XmlDom *dom, const char *name, const char *public_id, const char *system_id);
const char *xml_dtd_public_id(const XMQNode *dtd);
const char *xml_dtd_system_id(const XMQNode *dtd);
/** Add an entity declaration. Returns NULL if an entity of the same kind and name already exists (first wins). */
XMQNode *xml_dtd_add_entity(XMQNode *dtd, const char *name, XmlEntityType etype,
                            const char *public_id, const char *system_id,
                            const char *notation, const char *content, const char *orig);
/** Add an element/attribute declaration as preformatted text without the final ">\n". */
XMQNode *xml_dtd_add_decl(XMQNode *dtd, XmlNodeKind kind, const char *name, const char *text, const char *default_value);
/** Add a notation declaration (text without the final ">\n"). Duplicates are ignored, returns NULL. */
XMQNode *xml_dtd_add_notation(XMQNode *dtd, const char *name, const char *text);
/** Add a parameter entity reference %name; between the declarations. */
XMQNode *xml_dtd_add_pe_ref(XMQNode *dtd, const char *name);
/** Iterate the notations with xml_node_next. */
XMQNode *xml_dtd_first_notation(const XMQNode *dtd);
/** True if the dtd has any declarations or parameter entity references (comments/pis do not count). */
bool xml_dtd_has_declarations(const XMQNode *dtd);
/** Find a general entity declaration in the dom's dtd. */
XMQNode *xml_dom_get_entity(XmlDom *dom, const char *name);
/** Find a parameter entity declaration in the dom's dtd. */
XMQNode *xml_dom_get_parameter_entity(XmlDom *dom, const char *name);
/** Entity declaration accessors. The value (char refs expanded) is xml_node_content. */
XmlEntityType xml_entity_decl_type(const XMQNode *decl);
/** The literal value as written, without quotes (NULL if unknown). */
const char *xml_entity_decl_orig(const XMQNode *decl);
const char *xml_entity_decl_public_id(const XMQNode *decl);
const char *xml_entity_decl_system_id(const XMQNode *decl);
/** NDATA notation name for unparsed entities. */
const char *xml_entity_decl_notation(const XMQNode *decl);
/** Element/attribute/notation declarations: the preformatted text is xml_node_content.
    An attribute declaration can have a default value, that is escaped when serialized. */
const char *xml_decl_default_value(const XMQNode *decl);

// Node creation //////////////////////////////////////////////////

XMQNode *xml_node_new_element(XmlDom *dom, const char *name, XmlNs *ns);
XMQNode *xml_node_new_text(XmlDom *dom, const char *text);
XMQNode *xml_node_new_text_len(XmlDom *dom, const char *text, size_t len);
XMQNode *xml_node_new_cdata(XmlDom *dom, const char *text, size_t len);
/** A comment. text NULL gives a comment without content (serialized as nothing). */
XMQNode *xml_node_new_comment(XmlDom *dom, const char *text);
/** A processing instruction. data may be NULL (serialized as <?target?>). */
XMQNode *xml_node_new_pi(XmlDom *dom, const char *target, const char *data);
/** An entity reference &name; */
XMQNode *xml_node_new_entity_ref(XmlDom *dom, const char *name);

/** A namespace node for xpath, name = prefix (NULL for the default namespace), content = href.
    index orders the namespace nodes of the same element (stored as the node line). Free with xml_node_free. */
XMQNode *xml_node_new_namespace_node(XMQNode *element, const char *prefix, const char *href, int index);

/** Unlink and free the node and its subtree. */
void xml_node_free(XMQNode *node);

// Node accessors /////////////////////////////////////////////////

XmlNodeKind xml_node_kind(const XMQNode *node);
XmlDom *xml_node_dom(const XMQNode *node);
/** The parent node. For an attribute this is the owner element. */
XMQNode *xml_node_parent(const XMQNode *node);
XMQNode *xml_node_first_child(const XMQNode *node);
XMQNode *xml_node_last_child(const XMQNode *node);
/** The next sibling. For attributes the next attribute, for notations the next notation. */
XMQNode *xml_node_next(const XMQNode *node);
XMQNode *xml_node_prev(const XMQNode *node);
/** Element/attribute local name, pi target, entity name, dtd name, declaration name. */
const char *xml_node_name(const XMQNode *node);
void xml_node_set_name(XMQNode *node, const char *name);
/** The raw content of a text/cdata/comment/pi node or a declaration. NULL for elements, attributes, entity refs. */
const char *xml_node_content(const XMQNode *node);
/** Set the raw content of a leaf node. For elements/attributes/document: replace all children with one text node. */
void xml_node_set_content(XMQNode *node, const char *content);
/** Append to the content of a leaf node (text/cdata/comment/pi). */
void xml_node_append_content(XMQNode *node, const char *content, size_t len);
/** The XPath string-value of the node: the text of the descendant text/cdata nodes, entity references
    contribute nothing. Always malloced, never NULL. */
char *xml_node_string_value(const XMQNode *node);
bool xml_node_is_element(const XMQNode *node);
bool xml_node_is_text(const XMQNode *node);
/** The namespace of an element or attribute, NULL if none. */
XmlNs *xml_node_ns(const XMQNode *node);
/** Namespace uri of the element/attribute, NULL if none (an xmlns="" undeclaration is also NULL). */
const char *xml_node_ns_uri(const XMQNode *node);
const char *xml_node_ns_prefix(const XMQNode *node);
/** Source line, 0 if unknown. */
int xml_node_line(const XMQNode *node);
void xml_node_set_line(XMQNode *node, int line);
/** Free for use by callers, never touched by the dom (replaces libxml2 _private). Not copied. */
void *xml_node_user_data(const XMQNode *node);
void xml_node_set_user_data(XMQNode *node, void *user_data);

// Tree editing ///////////////////////////////////////////////////

/** Append child last, no text merging. Child is unlinked first. Returns child. */
XMQNode *xml_node_append_child(XMQNode *parent, XMQNode *child);
/** Append child last, merging text with a preceding text node (then child is freed). Returns the resulting node. */
XMQNode *xml_node_add_child(XMQNode *parent, XMQNode *child);
/** Insert n before node. Returns n. */
XMQNode *xml_node_add_prev_sibling(XMQNode *node, XMQNode *n);
/** Insert n after node. Returns n. */
XMQNode *xml_node_add_next_sibling(XMQNode *node, XMQNode *n);
/** Detach node from its parent and siblings. Attributes are detached from their owner. */
void xml_node_unlink(XMQNode *node);
/** Put n where node is and unlink node (node is not freed). Returns node. */
XMQNode *xml_node_replace(XMQNode *node, XMQNode *n);
/** Deep (or shallow) copy node into dom. The copy is unlinked. Namespaces are shared refs, use reconcile.
    Attributes are always copied with their values. A DTD is always copied deep. */
XMQNode *xml_node_copy(XmlDom *dom, const XMQNode *node, bool deep);
size_t xml_node_num_children(const XMQNode *node);
/** Compare document order: <0 a before b, 0 same, >0 a after b. Nodes in different trees compare by address. */
int xml_node_compare_order(const XMQNode *a, const XMQNode *b);

// Attributes /////////////////////////////////////////////////////

/** Set or replace attribute with local name and namespace (ns may be NULL). The value becomes one text child. */
XMQAttr *xml_node_set_attr(XMQNode *element, const char *name, XmlNs *ns, const char *value);
/** Find attribute by local name and namespace uri (NULL uri means no namespace). */
/** Move an attribute (from any element) to the end of the element's attributes, replacing an attribute
    with the same name and namespace. The namespace of the attribute must be in scope (see xml_node_reconcile_ns). */
XMQAttr *xml_node_move_attr(XMQNode *element, XMQAttr *attr);
/** Append an attribute without checking for an existing one with the same name (like libxml2 xmlNewProp),
    used by parsers that resolve namespaces later. value may be NULL for no value. */
XMQAttr *xml_node_append_attr(XMQNode *element, const char *name, XmlNs *ns, const char *value);
XMQAttr *xml_node_get_attr(const XMQNode *element, const char *name, const char *ns_uri);
/** Remove and free the attribute. Returns true if found. */
bool xml_node_remove_attr(XMQNode *element, const char *name, const char *ns_uri);
XMQAttr *xml_node_first_attr(const XMQNode *element);
XMQAttr *xml_attr_next(const XMQAttr *attr);
const char *xml_attr_name(const XMQAttr *attr);
/** The value if it is a single text node (or "" if empty), NULL if the value contains entity references.
    Use xml_node_string_value for the text without the entity references. */
const char *xml_attr_simple_value(const XMQAttr *attr);
XMQNode *xml_attr_owner(const XMQAttr *attr);
/** An attribute as a node (its children are the value). */
XMQNode *xml_attr_node(XMQAttr *attr);
/** A node as an attribute, NULL if it is not an attribute. */
XMQAttr *xml_node_as_attr(XMQNode *node);

// Namespaces /////////////////////////////////////////////////////

/** NULL for the default namespace. */
const char *xml_ns_prefix(const XmlNs *ns);
/** "" for an undeclaration xmlns="". */
/** The uri, "" for an undeclaration (xmlns=""), NULL for a prefix declared without uri (xmq allows that). */
const char *xml_ns_href(const XmlNs *ns);
/** The namespaces declared on an element, iterate with xml_ns_next_decl. */
XmlNs *xml_node_first_ns_decl(const XMQNode *element);
XmlNs *xml_ns_next_decl(const XmlNs *ns);
/** Declare prefix->href on element. Returns NULL if prefix is already declared there. */
XmlNs *xml_ns_declare(XMQNode *element, const char *prefix, const char *href);
/** Set the namespace of an element or attribute (the ref count is managed). */
void *xml_ns_user_data(const XmlNs *ns);
void xml_ns_set_user_data(XmlNs *ns, void *user_data);
/** Move the declaration ns of element last among the element's declarations. */
void xml_ns_move_decl_last(XMQNode *element, XmlNs *ns);
/** Remove the declaration ns from element, the element and its descendants (and attributes) that use ns
    are changed to use replacement (which may be NULL, it must be in scope). */
void xml_ns_remove_decl(XMQNode *element, XmlNs *ns, XmlNs *replacement);
/** Change the href of a declaration, used while parsing when an xmlns attribute follows its first use. */
void xml_ns_set_href(XmlNs *ns, const char *href);
void xml_node_set_ns(XMQNode *node, XmlNs *ns);
/** Find the in scope declaration of prefix (NULL = default namespace), walking ancestors. "xml" is predefined. */
XmlNs *xml_ns_lookup_prefix(const XMQNode *node, const char *prefix);
/** Find an in scope declaration of href whose prefix is not shadowed by a closer declaration. */
XmlNs *xml_ns_lookup_href(const XMQNode *node, const char *href);
/** Declare (on the node itself) every namespace used in the subtree that is not in scope. */
void xml_node_reconcile_ns(XMQNode *node);

/** Split "{prefix}uri" into prefix (malloced, or NULL) and uri. Returns NULL on a missing '}'. */
const char *xml_ns_split_puri(const char *puri, char **out_prefix);
/** Apply an xmq.h XMQNS action to a new element (already linked into its parent). */
XMQStatus xml_node_apply_xmqns(XMQNode *element, XMQNS ns);
/** Set an attribute using an xmq.h XMQNS action (NS_HERE is not allowed for attributes). */
XMQReturnAttr xml_node_set_attr_xmqns(XMQNode *element, const char *name, const char *value, XMQNS ns);

#define XML_DOM_MODULE

#endif // XML_DOM_H
