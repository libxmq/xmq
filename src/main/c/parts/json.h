/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

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

#ifndef JSON_H
#define JSON_H

#include"xmq.h"
#include<libxml/tree.h>

struct XMQPrintState;
typedef struct XMQPrintState XMQPrintState;

bool xmq_parse_buffer_json(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root);
void xmq_fixup_json_before_writeout(XMQDoc *doq);
void json_print_object_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to);
void collect_leading_ending_comments_doctype(XMQPrintState *ps, xmlNodePtr *first, xmlNodePtr *last);
void json_print_array_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to);
bool xmq_tokenize_buffer_json(XMQParseState *state, const char *start, const char *stop);

#define JSON_MODULE

#endif // JSON_H
