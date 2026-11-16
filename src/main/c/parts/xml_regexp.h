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

/* Portions of this file are ported from libxml2 (xmlregexp.c, xmlregexp.h and xmlautomata.h, version 2.16.0 as found
   in the libxmq fork in 3rdparty/libxml2).
   libxml2 is covered by the following notice (from libxml2/Copyright):

 Copyright (C) 1998-2012 Daniel Veillard.  All Rights Reserved.
 Copyright (C) The Libxml2 Contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef XML_REGEXP_H
#define XML_REGEXP_H

/* XML Schemas (Part 2, Appendix F) regular expressions, the automata used to build
   content models and their progressive execution, a port of libxml2 xmlregexp.c
   (xmlRegexp*, xmlAutomata* and xmlRegExec*). Strings are UTF-8. */

#include<stdio.h>

/** A compiled regular expression or automata (libxml2 xmlRegexp). */
typedef struct XmlRegexp XmlRegexp;
/** A progressive execution context (libxml2 xmlRegExecCtxt). */
typedef struct XmlRegExecCtxt XmlRegExecCtxt;
/** An automata under construction (libxml2 xmlAutomata). */
typedef struct XmlAutomata XmlAutomata;
/** A state of an automata (libxml2 xmlAutomataState). */
typedef struct XmlAutomataState XmlAutomataState;

/** Receives each compile error, message is the libxml2 error message
    ("failed to compile: ...\n"), regexp the expression (NULL for automata) and
    index the offset in it where the error was found (libxml2 error str2 and int1). */
typedef void (*XmlRegexpErrorFunc)(void *data, const char *message, const char *regexp, int index);

/** Called when a transition is taken by xml_reg_exec_push_string(2) (libxml2
    xmlRegExecCallbacks). Like libxml2, the first argument is the data given to
    xml_reg_exec_new (libxml2 declares it as the exec context but passes the data),
    token the transition token, transdata the data of the transition and
    inputdata the data pushed with the token. */
typedef void (*XmlRegExecCallbacks)(void *exec_data, const char *token, void *transdata, void *inputdata);

/* The exec status and error return values (libxml2 private/regexp.h). */
#define XML_REGEXP_OK               0
#define XML_REGEXP_NOT_FOUND        (-1)
#define XML_REGEXP_INTERNAL_ERROR   (-4)
#define XML_REGEXP_OUT_OF_MEMORY    (-5)
#define XML_REGEXP_INTERNAL_LIMIT   (-6)
#define XML_REGEXP_INVALID_UTF8     (-7)

/** The automata flag used by RELAX NG (libxml2 AM_AUTOMATA_RNG): string atoms are
    compared by pointer when computing determinism. */
#define XML_AUTOMATA_RNG 1

/** Compile a regular expression, NULL on error (libxml2 xmlRegexpCompile). */
XmlRegexp *xml_regexp_compile(const char *regexp);
/** Like xml_regexp_compile, error_func (may be NULL) receives each compile error. */
XmlRegexp *xml_regexp_compile_with_errors(const char *regexp, XmlRegexpErrorFunc error_func, void *error_data);
/** Returns 1 if content matches, 0 if not and a negative value on error (libxml2 xmlRegexpExec). */
int xml_regexp_exec(XmlRegexp *comp, const char *content);
/** Returns 1 if deterministic, 0 if not and -1 on error (libxml2 xmlRegexpIsDeterminist). */
int xml_regexp_is_determinist(XmlRegexp *comp);
/** A no-op, like libxml2 xmlRegexpPrint since 2.14. */
void xml_regexp_print(FILE *output, XmlRegexp *regexp);
/** libxml2 xmlRegFreeRegexp. */
void xml_regexp_free(XmlRegexp *regexp);

/** libxml2 xmlNewAutomata. */
XmlAutomata *xml_automata_new(void);
/** libxml2 xmlFreeAutomata. */
void xml_automata_free(XmlAutomata *am);
/** Errors while building the automata go to error_func (libxml2 uses the global error handler). */
void xml_automata_set_error_func(XmlAutomata *am, XmlRegexpErrorFunc error_func, void *error_data);
/** libxml2 xmlAutomataSetFlags, eg XML_AUTOMATA_RNG. */
void xml_automata_set_flags(XmlAutomata *am, int flags);
XmlAutomataState *xml_automata_get_init_state(XmlAutomata *am);
int xml_automata_set_final_state(XmlAutomata *am, XmlAutomataState *state);
XmlAutomataState *xml_automata_new_state(XmlAutomata *am);
XmlAutomataState *xml_automata_new_transition(XmlAutomata *am, XmlAutomataState *from, XmlAutomataState *to,
                                              const char *token, void *data);
XmlAutomataState *xml_automata_new_transition2(XmlAutomata *am, XmlAutomataState *from, XmlAutomataState *to,
                                               const char *token, const char *token2, void *data);
XmlAutomataState *xml_automata_new_neg_trans(XmlAutomata *am, XmlAutomataState *from, XmlAutomataState *to,
                                             const char *token, const char *token2, void *data);
XmlAutomataState *xml_automata_new_count_trans(XmlAutomata *am, XmlAutomataState *from, XmlAutomataState *to,
                                               const char *token, int min, int max, void *data);
XmlAutomataState *xml_automata_new_count_trans2(XmlAutomata *am, XmlAutomataState *from, XmlAutomataState *to,
                                                const char *token, const char *token2, int min, int max, void *data);
XmlAutomataState *xml_automata_new_once_trans(XmlAutomata *am, XmlAutomataState *from, XmlAutomataState *to,
                                              const char *token, int min, int max, void *data);
XmlAutomataState *xml_automata_new_once_trans2(XmlAutomata *am, XmlAutomataState *from, XmlAutomataState *to,
                                               const char *token, const char *token2, int min, int max, void *data);
XmlAutomataState *xml_automata_new_all_trans(XmlAutomata *am, XmlAutomataState *from, XmlAutomataState *to, int lax);
XmlAutomataState *xml_automata_new_epsilon(XmlAutomata *am, XmlAutomataState *from, XmlAutomataState *to);
XmlAutomataState *xml_automata_new_counted_trans(XmlAutomata *am, XmlAutomataState *from, XmlAutomataState *to, int counter);
XmlAutomataState *xml_automata_new_counter_trans(XmlAutomata *am, XmlAutomataState *from, XmlAutomataState *to, int counter);
int xml_automata_new_counter(XmlAutomata *am, int min, int max);
/** Compile the automata, it should be freed after this (libxml2 xmlAutomataCompile). */
XmlRegexp *xml_automata_compile(XmlAutomata *am);
int xml_automata_is_determinist(XmlAutomata *am);

/** libxml2 xmlRegNewExecCtxt. */
XmlRegExecCtxt *xml_reg_exec_new(XmlRegexp *comp, XmlRegExecCallbacks callback, void *data);
/** libxml2 xmlRegFreeExecCtxt. */
void xml_reg_exec_free(XmlRegExecCtxt *exec);
/** Push a token, NULL for the end. Returns 1 if a final state was reached, 0 if
    non-final and a negative value on error (libxml2 xmlRegExecPushString). */
int xml_reg_exec_push_string(XmlRegExecCtxt *exec, const char *value, void *data);
/** Push the token value|value2 (libxml2 xmlRegExecPushString2). */
int xml_reg_exec_push_string2(XmlRegExecCtxt *exec, const char *value, const char *value2, void *data);
/** libxml2 xmlRegExecNextValues, the values are owned by exec. */
int xml_reg_exec_next_values(XmlRegExecCtxt *exec, int *nbval, int *nbneg, char **values, int *terminal);
/** libxml2 xmlRegExecErrInfo, the strings are owned by exec. */
int xml_reg_exec_err_info(XmlRegExecCtxt *exec, const char **string, int *nbval, int *nbneg, char **values, int *terminal);
/** libxml2 xmlRegExecClearErrors. */
void xml_reg_exec_clear_errors(XmlRegExecCtxt *exec);

#ifndef XMQ_NO_XSD
#define XML_REGEXP_MODULE
#endif

#endif // XML_REGEXP_H
