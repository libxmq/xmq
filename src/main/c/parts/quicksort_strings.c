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
