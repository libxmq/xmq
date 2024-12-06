/* libxmq - Copyright (C) 2024 Fredrik Öhrström (spdx: MIT)

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

#ifndef IXML_H
#define IXML_H

#include"xmq.h"

struct YaepGrammar;
typedef struct YaepGrammar YaepGrammar;
struct YaepParseRun;
typedef struct YaepParseRun YaepParseRun;
struct yaep_tree_node;

struct IXMLRule;
typedef struct IXMLRule IXMLRule;
struct IXMLTerminal;
typedef struct IXMLTerminal IXMLTerminal;
struct IXMLNonTerminal;
typedef struct IXMLNonTerminal IXMLNonTerminal;

bool ixml_build_yaep_grammar(YaepParseRun *pr,
                             YaepGrammar *g,
                             XMQParseState *state,
                             const char *grammar_start,
                             const char *grammar_stop,
                             const char *content_start, // Needed to minimize charset rule sizes.
                             const char *content_stop);

void free_ixml_rule(IXMLRule *r);
void free_ixml_terminal(IXMLTerminal *t);
void free_ixml_nonterminal(IXMLNonTerminal *nt);

#define IXML_MODULE

#endif // IXML_H
