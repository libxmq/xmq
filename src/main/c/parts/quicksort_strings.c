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
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include"quicksort_strings.h"
#include<assert.h>
#include<string.h>

#ifdef QUICKSORT_STRINGS_MODULE

// FUNCTION DECLARATIONS //////////////////////////////////////////

void swap(const char **a, const char **b);

///////////////////////////////////////////////////////////////////

void swap(const char **a, const char **b)
{
	const char *temp = *a;
	*a = *b;
	*b = temp;
}

void quicksort_strings(char const *arr[], size_t length)
{
	size_t  i, piv = 0;
	if (length <= 1) return;

	for (i = 0; i < length; i++)
    {
		if (strcmp(arr[i], arr[length -1]) < 0)
        {
			swap(arr + i, arr + piv++);
        }
	}

	swap(arr + piv, arr + length - 1);

	quicksort_strings(arr, piv++);
	quicksort_strings(arr + piv, length - piv);
}

#endif // QUICKSORT_STRINGS_MODULE
