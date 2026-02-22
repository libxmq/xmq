/*
   YAEP (Yet Another Earley Parser)

   Copyright (c) 1997-2018 Vladimir Makarov <vmakarov@gcc.gnu.org>
   Copyright (c) 2025      Fredrik Öhrström <oehrstroem@gmail.com>

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

/* FILE NAME:   vlobject.c

   TITLE:       Package for work with variable length objects (VLO)

   DESCRIPTION:
       This file implements internal functions of the package.

   SPECIAL CONSIDERATION:
       The following functions are to be used only by the package
       macros.  Remember that they are internal functions - all work
       with VLO is executed through the macros.
         Defining macro `NDEBUG' (e.g. by option `-D' in C compiler
       command line) during the file compilation disables to fix
       some internal errors and errors of usage of the package.

*/

#ifndef BUILDING_DIST_XMQ

#include <assert.h>
#include <string.h>

#include "yaep_allocate.h"
#include "yaep_vlobject.h"

#endif

#ifdef YAEP_VLOBJECT_MODULE

/* The function implements macro `VLO_TAILOR'.  Length of memory
   allocated for VLO becames equal to VLO length (but memory for zero
   length object will contain one byte).  Remember that the VLO place
   may be changed after the call. */

void
_VLO_tailor_function (vlo_t * vlo)
{
  size_t vlo_length;
  char *new_vlo_start;

  assert (vlo->vlo_start != NULL);
  vlo_length = VLO_LENGTH (*vlo);
  if (vlo_length == 0)
    vlo_length = 1;
  new_vlo_start = (char*)yaep_realloc (vlo->vlo_alloc, vlo->vlo_start, vlo_length);
  if (new_vlo_start != vlo->vlo_start)
    {
      vlo->vlo_stop += new_vlo_start - vlo->vlo_start;
      vlo->vlo_start = new_vlo_start;
    }
  vlo->vlo_segment_stop = vlo->vlo_start + vlo_length;
}

/* The following function implements macro `VLO_ADD_STRING' (addition
   of string STR (with end marker is '\0') to the end of VLO).
   Remember that the VLO place may be changed after the call. */

void
_VLO_add_string_function (vlo_t * vlo, const char *str)
{
  size_t length;

  assert (vlo->vlo_start != NULL);
  if (str == NULL)
    return;
  if (vlo->vlo_stop != vlo->vlo_start)
    VLO_SHORTEN (*vlo, 1);
  length = strlen (str) + 1;
  if (vlo->vlo_stop + length > vlo->vlo_segment_stop)
    _VLO_expand_memory (vlo, length);
  memcpy( vlo->vlo_stop, str, length );
  vlo->vlo_stop = vlo->vlo_stop + length;
}

/* The following function changes size of memory allocated for VLO.
   The size becames equal to about one and a half of VLO length
   accounting for length of memory which will be added after the call.
   Remember that the VLO place may be changed after the call. */

void
_VLO_expand_memory (vlo_t * vlo, size_t additional_length)
{
  size_t vlo_length, old_vlo_length;
  char *new_vlo_start;

  assert (vlo->vlo_start != NULL);
  old_vlo_length = VLO_LENGTH (*vlo);
  vlo_length = old_vlo_length + additional_length;
  vlo_length += vlo_length / 2 + 1;
  new_vlo_start = (char*)yaep_realloc (vlo->vlo_alloc, vlo->vlo_start, vlo_length);
  if (new_vlo_start != vlo->vlo_start)
    {
     // Fix for fil-c, rewrite: vlo->vlo_stop += new_vlo_start - vlo->vlo_start;
     // because the new stop gets the old fil-c ptr capabilities and becomes out of bounds.
     // Better to use:
     vlo->vlo_stop  = new_vlo_start + old_vlo_length;
     // This creates a new capability that points into the new memory object created by realloc.
      vlo->vlo_start = new_vlo_start;
    }
  vlo->vlo_segment_stop = vlo->vlo_start + vlo_length;
}

size_t vlo_memusage(vlo_t *vlo)
{
    if (!vlo) return 0;
    return vlo->vlo_segment_stop - vlo->vlo_start;
}

#endif
