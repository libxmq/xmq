// libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
//
// Built with dist style xmq.c and some of XMQ_NO_XMQ_PRINTING, XMQ_NO_JSON, XMQ_NO_YAML,
// XMQ_NO_LIBXSLT, XMQ_NO_XSD, XMQ_NO_IXML and XMQ_NO_HTML (see tests/test_excluded.sh). Every api of an excluded
// part must return XMQ_ERROR_EXCLUDED_FROM_BUILD (or fail), the others must work.

#include"xmq.h"

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

static int failed = 0;

static void check(const char *what, int ok)
{
    if (!ok)
    {
        printf("ERR: %s\n", what);
        failed = 1;
    }
}

static XMQDoc *parse(const char *content, XMQContentType ct, bool *ok)
{
    XMQDoc *doc = xmqNewDoc().doc;
    *ok = xmqParseBufferWithType(doc, content, content+strlen(content), NULL, ct, 0);
    return doc;
}

static char *print(XMQDoc *doc, XMQContentType ct)
{
    XMQOutputSettings *os = xmqNewOutputSettings();
    char *start = NULL;
    char *stop = NULL;
    xmqSetOutputFormat(os, ct);
    xmqSetupPrintMemory(os, &start, &stop);
    xmqPrint(doc, os);
    xmqFreeOutputSettings(os);
    return start;
}

// A part that is excluded fails with XMQ_ERROR_EXCLUDED_FROM_BUILD and a message.
static void check_excluded(const char *what, XMQDoc *doc)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s: expected XMQ_ERROR_EXCLUDED_FROM_BUILD got %d", what, (int)xmqDocErrno(doc));
    check(buf, xmqDocErrno(doc) == XMQ_ERROR_EXCLUDED_FROM_BUILD);
    const char *e = xmqDocError(doc);
    snprintf(buf, sizeof(buf), "%s: expected an excluded error message got %s", what, e ? e : "NULL");
    check(buf, e && strstr(e, "was excluded when xmq was built"));
}

int main()
{
    bool ok;

    // The xml parser and printer are never excluded.
    XMQDoc *doc = parse("<a><b>1</b></a>", XMQ_CONTENT_XML, &ok);
    check("parse xml", ok);
    char *xml = print(doc, XMQ_CONTENT_XML);
    check("print xml", xml && strstr(xml, "<b>1</b>"));
    free(xml);

    // xmq parsing is never excluded, only the xmq printing.
    XMQDoc *xdoc = parse("a{b=1}", XMQ_CONTENT_XMQ, &ok);
    check("parse xmq", ok);
    xmqFreeDoc(xdoc);

    char *xmq = print(doc, XMQ_CONTENT_XMQ);
#ifdef XMQ_NO_XMQ_PRINTING
    check("print xmq excluded", !xmq || !*xmq);
    check_excluded("print xmq", doc);
    check("xmqCompactQuote excluded", xmqCompactQuote("a b") == NULL);
#else
    check("print xmq", xmq && strstr(xmq, "b = 1"));
    char *q = xmqCompactQuote("a b");
    check("xmqCompactQuote", q && !strcmp(q, "'a b'"));
    free(q);
#endif
    free(xmq);
    xmqFreeDoc(doc);

    doc = parse("{\"a\":1}", XMQ_CONTENT_JSON, &ok);
#ifdef XMQ_NO_JSON
    check("parse json excluded", !ok);
    check_excluded("parse json", doc);
#else
    check("parse json", ok);
#endif
    xmqFreeDoc(doc);

    doc = parse("<a><b>1</b></a>", XMQ_CONTENT_XML, &ok);
    char *json = print(doc, XMQ_CONTENT_JSON);
#ifdef XMQ_NO_JSON
    check("print json excluded", !json || !*json);
    check_excluded("print json", doc);
#else
    check("print json", json && strstr(json, "\"b\": 1"));
#endif
    free(json);
    xmqFreeDoc(doc);

    doc = parse("a: 1\n", XMQ_CONTENT_YAML, &ok);
#ifdef XMQ_NO_YAML
    check("parse yaml excluded", !ok);
    check_excluded("parse yaml", doc);
#else
    check("parse yaml", ok);
#endif
    xmqFreeDoc(doc);

    doc = parse("<a><b>1</b></a>", XMQ_CONTENT_XML, &ok);
    char *yaml = print(doc, XMQ_CONTENT_YAML);
#ifdef XMQ_NO_YAML
    check("print yaml excluded", !yaml || !*yaml);
    check_excluded("print yaml", doc);
#else
    check("print yaml", yaml && strstr(yaml, "b: 1"));
#endif
    free(yaml);

    const char *xsl = "<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>"
        "<xsl:output method='text'/><xsl:template match='/'><xsl:value-of select='/a/b'/></xsl:template>"
        "</xsl:stylesheet>";
    XMQDoc *style = parse(xsl, XMQ_CONTENT_XML, &ok);
    XMQReturnXslt rx = xmqCompileXslt(style, NULL);
#ifdef XMQ_NO_LIBXSLT
    check("xmqCompileXslt excluded", rx.status == XMQ_ERROR_EXCLUDED_FROM_BUILD && rx.xslt == NULL);
    check_excluded("xmqCompileXslt", style);
    XMQReturnDoc rt = xmqTransform(NULL, doc);
    check("xmqTransform excluded", rt.status == XMQ_ERROR_EXCLUDED_FROM_BUILD && rt.doc == NULL);
#else
    check("xmqCompileXslt", rx.status == XMQ_OK && rx.xslt != NULL);
    XMQReturnDoc rt = xmqTransform(rx.xslt, doc);
    check("xmqTransform", rt.status == XMQ_OK);
    size_t len = 0;
    char *out = xmqSaveXsltResult(rx.xslt, rt.doc, &len);
    check("xmqSaveXsltResult", out && !strcmp(out, "1"));
    free(out);
    xmqFreeDoc(rt.doc);
#endif
    xmqFreeXslt(rx.xslt);
    xmqFreeDoc(style);

#if !defined(XMQ_NO_LIBXSLT)
    // The xslt html output method needs the html serializer.
    const char *xslh = "<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>"
        "<xsl:output method='html'/><xsl:template match='/'><p><xsl:value-of select='/a/b'/></p></xsl:template>"
        "</xsl:stylesheet>";
    XMQDoc *styleh = parse(xslh, XMQ_CONTENT_XML, &ok);
    XMQReturnXslt rh = xmqCompileXslt(styleh, NULL);
    check("xmqCompileXslt html method", rh.status == XMQ_OK);
    XMQReturnDoc th = xmqTransform(rh.xslt, doc);
    check("xmqTransform html method", th.status == XMQ_OK);
    size_t hlen = 0;
    char *hout = xmqSaveXsltResult(rh.xslt, th.doc, &hlen);
#ifdef XMQ_NO_HTML
    // Like an unknown output method: the message is reported and the result is empty.
    check("xslt html output excluded", !hout || !*hout);
#else
    check("xslt html output", hout && strstr(hout, "<p>1</p>"));
#endif
    free(hout);
    xmqFreeDoc(th.doc);
    xmqFreeXslt(rh.xslt);
    xmqFreeDoc(styleh);
#endif

    const char *xsd = "<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema'>"
        "<xs:element name='a'><xs:complexType><xs:sequence><xs:element name='b' type='xs:int'/>"
        "</xs:sequence></xs:complexType></xs:element></xs:schema>";
    XMQDoc *schema = parse(xsd, XMQ_CONTENT_XML, &ok);
    XMQReturnXsd rs = xmqCompileXsd(schema, NULL);
#ifdef XMQ_NO_XSD
    check("xmqCompileXsd excluded", rs.status == XMQ_ERROR_EXCLUDED_FROM_BUILD && rs.xsd == NULL);
    check_excluded("xmqCompileXsd", schema);
    check("xmqValidate excluded", xmqValidate(NULL, doc) == XMQ_ERROR_EXCLUDED_FROM_BUILD);
#else
    check("xmqCompileXsd", rs.status == XMQ_OK && rs.xsd != NULL);
    check("xmqValidate", xmqValidate(rs.xsd, doc) == XMQ_OK);
#endif
    xmqFreeXsd(rs.xsd);
    xmqFreeDoc(schema);
    xmqFreeDoc(doc);

    const char *ixml = "s = 'a', b. b = 'b'.";
    XMQDoc *grammar = parse(ixml, XMQ_CONTENT_IXML, &ok);
#ifdef XMQ_NO_IXML
    check("parse ixml grammar excluded", !ok);
    check_excluded("parse ixml grammar", grammar);
    doc = xmqNewDoc().doc;
    check("xmqParseBufferWithIXML excluded", !xmqParseBufferWithIXML(doc, "ab", NULL, grammar, 0));
    check_excluded("xmqParseBufferWithIXML", doc);
#else
    check("parse ixml grammar", ok);
    doc = xmqNewDoc().doc;
    const char *in = "ab";
    check("xmqParseBufferWithIXML", xmqParseBufferWithIXML(doc, in, in+2, grammar, 0));
    char *ix = print(doc, XMQ_CONTENT_XML);
    check("print ixml result", ix && strstr(ix, "<s>a<b>b</b></s>"));
    free(ix);
#endif
    xmqFreeDoc(doc);
    xmqFreeDoc(grammar);

    doc = parse("<html><body><p>x</p></body></html>", XMQ_CONTENT_HTML, &ok);
#ifdef XMQ_NO_HTML
    check("parse html excluded", !ok);
    check_excluded("parse html", doc);
#else
    check("parse html", ok);
#endif
    xmqFreeDoc(doc);
    // (Content starting with <html is detected as html and is not accepted as xml.)
    doc = parse("<a><p>x</p></a>", XMQ_CONTENT_XML, &ok);
    char *html = print(doc, XMQ_CONTENT_HTML);
#ifdef XMQ_NO_HTML
    check("print html excluded", !html || !*html);
    check_excluded("print html", doc);
#else
    check("print html", html && strstr(html, "<p>x</p>"));
#endif
    free(html);
    xmqFreeDoc(doc);

    // The features compiled in.
    const char *f = xmqFeatures();
#ifdef XMQ_NO_JSON
    check("features without json", !strstr(f, " json"));
#else
    check("features with json", strstr(f, " json") != NULL);
#endif
#ifdef XMQ_NO_IXML
    check("features without ixml", !strstr(f, " ixml"));
#else
    check("features with ixml", strstr(f, " ixml") != NULL);
#endif
#ifdef XMQ_NO_HTML
    check("features without html", !strstr(f, " html"));
#else
    check("features with html", strstr(f, " html") != NULL);
#endif

    if (!failed) printf("OK\n");
    return failed;
}
