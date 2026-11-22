// libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
//
// Clang + libFuzzer in-process harness over the xmq, xml, html, json and yaml
// parsers and the xmq/xml/json printers, built with AddressSanitizer and
// UndefinedBehaviorSanitizer from an amalgamation of the parts (like dist/xmq.c).
//
// The first byte of the input selects the content type, the rest is parsed.
// \005 parses the rest as an ixml grammar, \006 parses the rest up to the first
// NUL as an ixml grammar and what follows the NUL as content with that grammar.
// Built with -DFUZZ_CONTENT_TYPE=n the type is fixed and the whole input is parsed.
// A successful parse is printed back as xmq, xml and json.
//
// The input is copied into a buffer of exactly its size, without a NUL terminator,
// so AddressSanitizer reports a parser that reads at or beyond stop.
//
//   make build_fuzz_clang
//   build/x86_64-pc-linux-gnu/fuzz_clang/fuzzxmq fuzz_testcases/clang
//   (add a scratch corpus dir first to keep new inputs out of the seeds)

#include"xmq.h"

#include<stddef.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>

static const XMQContentType types[] = {
    XMQ_CONTENT_XMQ,
    XMQ_CONTENT_XML,
    XMQ_CONTENT_HTML,
    XMQ_CONTENT_JSON,
    XMQ_CONTENT_YAML,
    XMQ_CONTENT_IXML,
    XMQ_CONTENT_IXML, // Grammar NUL content.
};

static void print_as(XMQDoc *doc, XMQContentType format)
{
    XMQOutputSettings *os = xmqNewOutputSettings();
    char *start = NULL;
    char *stop = NULL;
    xmqSetOutputFormat(os, format);
    xmqSetupPrintMemory(os, &start, &stop);
    xmqPrint(doc, os);
    free(start);
    xmqFreeOutputSettings(os);
}

// Parse the grammar before the first NUL and then the content after it with the grammar,
// both copied into buffers of exactly their size.
static int parse_with_ixml(const uint8_t *data, size_t size)
{
    const uint8_t *nul = (const uint8_t*)memchr(data, 0, size);
    if (!nul) return 0;
    size_t glen = nul - data;
    size_t clen = size - glen - 1;
    char *g = (char*)malloc(glen ? glen : 1);
    char *c = (char*)malloc(clen ? clen : 1);
    if (glen) memcpy(g, data, glen);
    if (clen) memcpy(c, nul + 1, clen);

    XMQReturnDoc rg = xmqNewDoc();
    XMQReturnDoc rd = xmqNewDoc();
    if (rg.status == XMQ_OK && rd.status == XMQ_OK &&
        xmqParseBufferWithType(rg.doc, g, g + glen, NULL, XMQ_CONTENT_IXML, 0))
    {
        if (xmqParseBufferWithIXML(rd.doc, c, c + clen, rg.doc, 0))
        {
            print_as(rd.doc, XMQ_CONTENT_XMQ);
            print_as(rd.doc, XMQ_CONTENT_XML);
        }
    }
    if (rd.status == XMQ_OK) xmqFreeDoc(rd.doc);
    if (rg.status == XMQ_OK) xmqFreeDoc(rg.doc);
    free(g);
    free(c);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 65536) return 0;
#ifndef FUZZ_CONTENT_TYPE
    if (size < 1) return 0;
#endif

#ifdef FUZZ_CONTENT_TYPE
    // Built for one content type (-DFUZZ_CONTENT_TYPE=0 for xmq, see make run_fuzz_clang_xmq),
    // the whole input is the content, so a finding is a plain file of that type.
    XMQContentType ct = types[FUZZ_CONTENT_TYPE];
    size_t skip = 0;
#else
    size_t type = data[0] % (sizeof(types)/sizeof(types[0]));
    XMQContentType ct = types[type];
    size_t skip = 1;
    if (type == 6) return parse_with_ixml(data + skip, size - skip);
#endif
    size_t len = size - skip;
    char *buf = (char*)malloc(len ? len : 1);
    if (len) memcpy(buf, data + skip, len);
    const char *start = buf;
    const char *stop = buf + len;

    XMQReturnDoc rd = xmqNewDoc();
    if (rd.status != XMQ_OK) { free(buf); return 0; }
    XMQDoc *doc = rd.doc;

    if (xmqParseBufferWithType(doc, start, stop, NULL, ct, 0))
    {
        print_as(doc, XMQ_CONTENT_XMQ);
        print_as(doc, XMQ_CONTENT_XML);
        print_as(doc, XMQ_CONTENT_JSON);
    }

    xmqFreeDoc(doc);
    free(buf);
    return 0;
}
