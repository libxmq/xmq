/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT)

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

#ifndef LANG_H
#define LANG_H

#include<xmq.h>
#include<stdbool.h>
#include<stdlib.h>
#include<string.h>

// Best-effort conversion of an English plural word to the singular form:
//
//   steps -> step, jobs -> job, names -> name,
//   houses -> house, boxes -> box, buzzes -> buzz, classes -> class,
//   branches -> branch, dishes -> dish, processes -> process, potatoes -> potato,
//   cities -> city, berries -> berry,
//   children -> child, people -> person, mice -> mouse, ...
//
// Returns a malloc'ed string, or NULL when the word does not look plural.
char *plural_to_singular(const char *w);

#define LANG_MODULE

#endif // LANG_H
