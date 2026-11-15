/* libxmq - Copyright (C) 2026 Fredrik Öhrström (spdx: MIT)

   Random mutations of a stylesheet, applied identically to a libxml2 tree and a
   native tree, shared by the xslt differential tests. Included by the .c files.
*/

#ifndef XML_LIBXSLT1_MUTATE_H
#define XML_LIBXSLT1_MUTATE_H

// Mutations, applied identically to both trees so that the error paths are exercised.

static unsigned long rng_ = 1;
static int mutants_ = 0;
static long mutated_ = 0;

static unsigned rnd(unsigned n)
{
    rng_ = rng_ * 6364136223846793005UL + 1442695040888963407UL;
    return n ? (unsigned)((rng_ >> 33) % n) : 0;
}

static const char *values_[] = { "", "1.0", "1.1", "2.0", "a:b", "*", "{", "}", "{@x}", "x{{y}}z", "{1+}", "foo(", "$x", "$undef",
    "yes", "no", "maybe", "#default", "xsl", "text", "html", "xml", "a b", "//x[1]", "1 2", "@*|node()", "key('k',.)",
    "id('a')", "-1", "NaN", "ascending", "descending", "upper-first", "lower-first", "number", "single", "any", "multiple",
    "unknown:prefix", "nope.xsl", "x|", "|x", "x[", "x[1]]", "a/b[@c='1']|d", "child::x", "ancestor::x", "x:*", "1a", ":x",
    "x:", "x::y", "processing-instruction('p')", "text()|comment()", "/", "a//b", "@x", "..", ".", "count(.)", "format-number(1,'#')",
    "A", "a", "i", "I", "1", "01", "&#x10ffff;", "\xc3\xa5\xc3\xa4\xc3\xb6", "\xe2\x80\xb0", "##", "xmlns", "xml:space" };
static const char *attr_names_[] = { "select", "match", "name", "mode", "priority", "test", "use", "href", "version",
    "exclude-result-prefixes", "extension-element-prefixes", "use-attribute-sets", "namespace", "disable-output-escaping",
    "elements", "method", "indent", "cdata-section-elements", "stylesheet-prefix", "result-prefix", "level", "count",
    "from", "value", "format", "order", "data-type", "case-order", "lang", "terminate", "grouping-separator",
    "grouping-size", "decimal-separator", "zero-digit", "digit", "encoding", "standalone", "omit-xml-declaration",
    "doctype-system", "doctype-public", "media-type", "letter-value" };
static const char *elem_names_[] = { "template", "apply-templates", "value-of", "if", "choose", "when", "otherwise",
    "for-each", "sort", "variable", "param", "with-param", "call-template", "attribute", "attribute-set", "element",
    "copy", "copy-of", "number", "text", "comment", "processing-instruction", "key", "output", "decimal-format",
    "namespace-alias", "strip-space", "preserve-space", "include", "import", "message", "fallback", "document",
    "apply-imports", "stylesheet", "transform", "bogus" };
#define NUM(a) (sizeof(a) / sizeof(a[0]))

typedef struct
{
    int op;        // 0 set attribute value, 1 add attribute, 2 remove attribute, 3 rename, 4 delete, 5 move first
    unsigned elem; // index of the element in document order
    unsigned attr; // index of the attribute (op 0 and 2)
    const char *name;
    const char *value;
} Mutation;

static void lx_elements(xmlNodePtr n, xmlNodePtr **v, size_t *num, size_t *max)
{
    for (; n; n = n->next)
    {
        if (n->type != XML_ELEMENT_NODE) continue;
        if (*num >= *max)
        {
            *max = *max ? 2 * *max : 64;
            *v = realloc(*v, *max * sizeof(xmlNodePtr));
        }
        (*v)[(*num)++] = n;
        lx_elements(n->children, v, num, max);
    }
}

static void nx_elements(XMQNode *n, XMQNode ***v, size_t *num, size_t *max)
{
    for (; n; n = xml_node_next(n))
    {
        if (xml_node_kind(n) != XML_DOM_ELEMENT) continue;
        if (*num >= *max)
        {
            *max = *max ? 2 * *max : 64;
            *v = realloc(*v, *max * sizeof(XMQNode*));
        }
        (*v)[(*num)++] = n;
        nx_elements(xml_node_first_child(n), v, num, max);
    }
}

/** Apply a mutation to both trees, returns false if it does not apply. */
static bool mutate(xmlDocPtr doc, XmlDom *dom, Mutation *m)
{
    xmlNodePtr *lv = NULL;
    XMQNode **nv = NULL;
    size_t ln = 0, lmax = 0, nn = 0, nmax = 0;
    lx_elements(doc->children, &lv, &ln, &lmax);
    nx_elements(xml_node_first_child(xml_dom_document(dom)), &nv, &nn, &nmax);
    bool ok = false;
    if (ln != nn || ln == 0) goto done;
    unsigned k = m->elem % (unsigned)ln;
    xmlNodePtr le = lv[k];
    XMQNode *ne = nv[k];
    switch (m->op)
    {
    case 0:
    case 2:
    {
        xmlAttrPtr la = le->properties;
        XMQAttr *na = xml_node_first_attr(ne);
        unsigned na_num = 0;
        for (xmlAttrPtr x = la; x; x = x->next) na_num++;
        if (!na_num) goto done;
        unsigned ai = m->attr % na_num;
        for (unsigned i = 0; i < ai; ++i)
        {
            la = la->next;
            na = na ? xml_attr_next(na) : NULL;
        }
        if (!na || strcmp((const char*)la->name, xml_attr_name(na))) goto done;
        char *name = strdup((const char*)la->name);
        const char *ns_uri = la->ns ? (const char*)la->ns->href : NULL;
        if (m->op == 0)
        {
            xmlSetNsProp(le, la->ns, la->name, (const xmlChar*)m->value);
            xml_node_set_attr(ne, name, xml_node_ns(xml_attr_node(na)), m->value);
        }
        else
        {
            xml_node_remove_attr(ne, name, ns_uri);
            xmlRemoveProp(la);
        }
        free(name);
        ok = true;
        break;
    }
    case 1:
        xmlSetProp(le, (const xmlChar*)m->name, (const xmlChar*)m->value);
        xml_node_set_attr(ne, m->name, NULL, m->value);
        ok = true;
        break;
    case 3:
        xmlNodeSetName(le, (const xmlChar*)m->name);
        xml_node_set_name(ne, m->name);
        ok = true;
        break;
    case 4:
        if (k == 0) goto done;
        xmlUnlinkNode(le);
        xmlFreeNode(le);
        xml_node_unlink(ne);
        xml_node_free(ne);
        ok = true;
        break;
    case 5:
    {
        if (k == 0) goto done;
        xmlNodePtr lfirst = lv[0]->children;
        XMQNode *nfirst = xml_node_first_child(nv[0]);
        if (!lfirst || !nfirst || lfirst == le || nfirst == ne) goto done;
        if (lfirst->type != XML_ELEMENT_NODE || xml_node_kind(nfirst) != XML_DOM_ELEMENT) goto done;
        xmlUnlinkNode(le);
        xmlAddPrevSibling(lfirst, le);
        xml_node_unlink(ne);
        xml_node_add_prev_sibling(nfirst, ne);
        ok = true;
        break;
    }
    }
done:
    free(lv);
    free(nv);
    return ok;
}

static void random_mutation(Mutation *m)
{
    m->op = (int)rnd(6);
    m->elem = rnd(1u << 30);
    m->attr = rnd(16);
    m->name = m->op == 3 ? elem_names_[rnd(NUM(elem_names_))] : attr_names_[rnd(NUM(attr_names_))];
    m->value = values_[rnd(NUM(values_))];
}


/** Apply 1-3 random mutations to both trees, returns the number applied. */
static int mutate_both(xmlDocPtr doc, XmlDom *dom)
{
    int nops = 1 + (int)rnd(3), applied = 0;
    for (int i = 0; i < nops; ++i)
    {
        Mutation m;
        random_mutation(&m);
        if (mutate(doc, dom, &m)) applied++;
    }
    return applied;
}

#endif // XML_LIBXSLT1_MUTATE_H
