// libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
//
// Clang + libFuzzer in-process harness over the xmq, xml, html, json and yaml
// parsers and the xmq/xml/json printers, built with AddressSanitizer and
// UndefinedBehaviorSanitizer from dist/xmq.c.
//
// The first byte of the input selects the content type, the rest is parsed.
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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1 || size > 65536) return 0;

    XMQContentType ct = types[data[0] % (sizeof(types)/sizeof(types[0]))];
    size_t len = size - 1;
    char *buf = (char*)malloc(len ? len : 1);
    if (len) memcpy(buf, data + 1, len);
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
