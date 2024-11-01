/****************** YAEP parser single source file headers **********************/
#ifndef BUILDING_XMQ

#include  "yaep.h"
#include<assert.h>
#include<setjmp.h>

#endif

#ifdef YAEP_MODULE
/*
   YAEP (Yet Another Earley Parser)

   Copyright (c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>

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

/**
 * @file allocate.h
 *
 * Memory allocation with error handling.
 */

#ifndef YAEP_ALLOCATE_H_
#define YAEP_ALLOCATE_H_

#ifdef NOT_DEFINED
"C"
{
#endif

#include<stddef.h>

/**
 * Type for functions behaving like standard malloc().
 *
 * @sa #Yaep_calloc()
 * @sa #Yaep_realloc()
 * @sa #Yaep_free()
 */
typedef void *(*Yaep_malloc) (size_t);

/**
 * Type for functions behaving like standard calloc().
 *
 * @sa #Yaep_malloc()
 * @sa #Yaep_realloc()
 * @sa #Yaep_free()
 */
typedef void *(*Yaep_calloc) (size_t, size_t);

/**
 * Type for functions behaving like standard realloc().
 *
 * @sa #Yaep_malloc()
 * @sa #Yaep_calloc()
 * @sa #Yaep_free()
 */
typedef void *(*Yaep_realloc) (void *, size_t);

/**
 * Type for functions behaving like standard free().
 *
 * @sa #Yaep_malloc()
 * @sa #Yaep_calloc()
 * @sa #Yaep_realloc()
 */
typedef void (*Yaep_free) (void *ptr);

/**
 * Callback type for allocation errors.
 *
 * It is not necessary for callbacks of this type
 * to return to the caller.
 *
 * @param userptr Pointer provided earlier by the caller.
 *
 * @sa #yaep_alloc_geterrfunc()
 * @sa #yaep_alloc_seterr()
 */
typedef void (*Yaep_alloc_error) (void *userptr);

/**
 * YAEP allocator type.
 *
 * @sa #yaep_alloc_new()
 */
typedef struct YaepAllocator YaepAllocator;

/**
 * Default error handling function.
 *
 * This function writes an error message to @ stderr
 * and exits the program.
 *
 * @param ignored Ignored parameter.
 *
 * @sa #yaep_alloc_seterr()
 */
void yaep_alloc_defaulterrfunc (void *ignored);

/**
 * Creates a new allocator.
 *
 * The new allocator uses #yaep_alloc_defaulterrfunc()
 * and a null user pointer.
 *
 * @param mallocf Pointer to function which behaves like @c malloc().
 * 	If this is a null pointer, @c malloc() is used instead.
 * @param callocf Pointer to function which behaves like @c calloc().
 * 	If this is a null pointer, a function which behaves like @c calloc()
 * 	and is compatible with the provided @c mallocf is used instead.
 * @param reallocf Pointer to function which behaves analogously to
 * 	@c realloc() with respect to @c mallocf and @c callocf.
 * 	If this is a null pointer, and @c malloc() and @c calloc() are used
 * 	for @c mallocf and @c callocf, respectively,
 * 	then @c realloc() is used instead.
 * 	Everything else is an error.
 * @param freef Pointer to function which can free memory returned by
 * 	@c mallocf, @c callocf, and @c reallocf.
 * 	If this is a null pointer, and @c malloc(), @c calloc(), and
 * 	@c realloc() are used for @c mallocf, @c callocf, and @c reallocf,
 * 	respectively, then @c free() is used instead.
 * 	Everything else is an error.
 *
 * @return On success, a pointer to the new allocator is returned.\n
 * 	On error, a null pointer is returned.
 *
 * @sa #yaep_alloc_del()
 * @sa #yaep_alloc_seterr()
 * @sa #yaep_alloc_defaulterrfunc()
 */
YaepAllocator *yaep_alloc_new (Yaep_malloc mallocf, Yaep_calloc callocf,
			       Yaep_realloc reallocf, Yaep_free freef);

/**
 * Destroys an allocator.
 *
 * @param allocator Pointer to allocator.
 *
 * @sa #yaep_alloc_new()
 */
void yaep_alloc_del (YaepAllocator * allocator);

/**
 * Allocates memory.
 *
 * @param allocator Pointer to allocator.
 * @param size Number of bytes to allocate.
 *
 * @return On success, a pointer to the allocated memory is returned.
 * 	If @c size was zero, this may be a null pointer.\n
 * 	On error, the allocator's error function is called.
 * 	If that function returns, a null pointer is returned.
 *
 * @sa #yaep_free()
 * @sa #yaep_realloc()
 * @sa #yaep_calloc()
 * @sa #yaep_alloc_seterr()
 */
void *yaep_malloc (YaepAllocator * allocator, size_t size);

/**
 * Allocates zero-initialised memory.
 *
 * @param allocator Pointer to allocator.
 * @param nmemb Number of elements to allocate.
 * @param size Element size in bytes.
 *
 * @return On success, a pointer to <code>nmemb * size</code> bytes
 * 	of newly allocated, zero-initialised  memory is returned.
 * 	If <code>nmemb * size</code> was zero, this may be a null pointer.\n
 * 	On error, the allocator's error function is called.
 * 	If that function returns, a null pointer is returned.
 *
 * @sa #yaep_free()
 * @sa #yaep_realloc()
 * @sa #yaep_malloc()
 * @sa #yaep_alloc_seterr()
 */
void *yaep_calloc (YaepAllocator * allocator, size_t nmemb, size_t size);

/**
 * Resizes memory.
 *
 * @param allocator Pointer to allocator previously used to
 * 	allocate @c ptr.
 * @param ptr Pointer to memory previously returned by
 * 	#yaep_malloc(), #yaep_calloc(), or #yaep_realloc().
 * 	If this is a null pointer, this function behaves like #yaep_malloc().
 * @param size New memory size in bytes.
 *
 * @return On success, a pointer to @c size bytes of allocated memory
 * 	is returned, the contents of which is equal to the contents of
 * 	@c ptr immediately before the call, up to the smaller size of
 * 	both blocks.\n
 * 	On error, the allocator's error function is called.
 * 	If that function returns, a null pointer is returned.
 *
 * @sa #yaep_free()
 * @sa #yaep_malloc()
 * @sa #yaep_calloc()
 * @sa #yaep_alloc_seterr()
 */
void *yaep_realloc (YaepAllocator * allocator, void *ptr, size_t size);

/**
 * Frees previously allocated memory.
 *
 * @param allocator Pointer to allocator previously used to
 * 	allocate @c ptr.
 * @param ptr Pointer to memory to be freed.
 * 	If this is a null pointer, no operation is performed.
 *
 * @sa #yaep_malloc()
 * @sa #yaep_calloc()
 * @sa #yaep_realloc()
 */
void yaep_free (YaepAllocator * allocator, void *ptr);

/**
 * Obtains the current error function of an allocator.
 *
 * @param allocator Pointer to allocator.
 *
 * @return On success, a pointer to the error function of the
 * 	specified allocator is returned.
 * 	If no error function has ever been set for this allocator,
 * 	this is #yaep_alloc_defaulterrfunc().\n
 * 	On error, a null pointer is returned.
 *
 * @sa #yaep_alloc_getuserptr()
 * @sa #yaep_alloc_seterr()
 * @sa #yaep_alloc_defaulterrfunc()
 */
Yaep_alloc_error yaep_alloc_geterrfunc (YaepAllocator * allocator);

/**
 * Obtains the current user-provided pointer.
 *
 * @param allocator Pointer to allocator.
 *
 * @return On success, the user-provided pointer of the
 * 	specified allocator is returned.
 * 	If no pointer has ever been set for this allocator,
 * 	a pointer to the allocator itself is returned.\n
 * 	On error, a null pointer is returned.
 *
 * @sa #yaep_alloc_seterr()
 */
void *yaep_alloc_getuserptr (YaepAllocator * allocator);

/**
 * Sets the error function and user-provided pointer of an allocator.
 *
 * The error function is called by the allocator with the user-provided
 * pointer as argument whenever an allocation error occurs.
 *
 * @param allocator Pointer to allocator.
 * @param errfunc Pointer to error function.
 * 	If this is a null pointer, #yaep_alloc_defaulterrfunc() will be used.
 * @param userptr User-provided pointer.
 * 	The allocator will never attempt to dereference this pointer.
 *
 * @sa #yaep_alloc_geterrfunc()
 * @sa #yaep_alloc_getuserptr()
 */
void yaep_alloc_seterr (YaepAllocator * allocator, Yaep_alloc_error errfunc,
			void *userptr);

#ifdef NOT_DEFINED
}
#endif

#endif
/*
   YAEP (Yet Another Earley Parser)

   Copyright (c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>

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

/* FILE NAME:   hashtab.h

   TITLE:       Include file of package for work with variable length
                hash tables

   DESCRIPTION: This header file contains type definitions and ANSI C
       prototype definitions of the package functions and definition of
       external variable of the package and C++ class `hash table'.

*/

#ifndef __HASH_TABLE__
#define __HASH_TABLE__

#include <stdlib.h>

//include"allocate.h"

/* The hash table element is represented by the following type. */

typedef const void *hash_table_entry_t;


#ifndef NOT_DEFINED


/* Hash tables are of the following type.  The structure
   (implementation) of this type is not needed for using the hash
   tables.  All work with hash table should be executed only through
   functions mentioned below. */

typedef struct
{
  /* Current size (in entries) of the hash table */
  size_t size;
  /* Current number of elements including also deleted elements */
  size_t number_of_elements;
  /* Current number of deleted elements in the table */
  size_t number_of_deleted_elements;
  /* The following member is used for debugging. Its value is number
     of all calls of `find_hash_table_entry' for the hash table. */
  int searches;
  /* The following member is used for debugging.  Its value is number
     of collisions fixed for time of work with the hash table. */
  int collisions;
  /* Pointer to function for evaluation of hash value (any unsigned value).
     This function has one parameter of type hash_table_entry_t. */
  unsigned (*hash_function) (hash_table_entry_t el_ptr);
  /* Pointer to function for test on equality of hash table elements (two
     parameter of type hash_table_entry_t. */
  int (*eq_function) (hash_table_entry_t el1_ptr,
                      hash_table_entry_t el2_ptr);
  /* Table itself */
  hash_table_entry_t *entries;
  /* Allocator */
  YaepAllocator * alloc;
} *hash_table_t;


/* The following variable is used for debugging. Its value is number
   of all calls of `find_hash_table_entry' for all hash tables. */

//extern int all_searches;

/* The following variable is used for debugging. Its value is number
   of collisions fixed for time of work with all hash tables. */

//extern int all_collisions;

/* The prototypes of the package functions. */

hash_table_t create_hash_table(
  YaepAllocator * allocator, size_t size, unsigned int ( *hash_function )( hash_table_entry_t el_ptr ), int ( *eq_function )( hash_table_entry_t el1_ptr, hash_table_entry_t el2_ptr )
);

void empty_hash_table (hash_table_t htab);

void delete_hash_table (hash_table_t htab);

hash_table_entry_t *find_hash_table_entry
  (hash_table_t htab, hash_table_entry_t element, int reserve);

void remove_element_from_hash_table_entry (hash_table_t htab,
                                                  hash_table_entry_t element);

size_t hash_table_size (hash_table_t htab);

size_t hash_table_elements_number (hash_table_t htab);

/* The following function returns number of searches during all work
   with given hash table. */
static inline int
get_searches (hash_table_t htab)
{
  return htab->searches;
}

/* The following function returns number of occurred collisions during
   all work with given hash table. */
static inline int
get_collisions (hash_table_t htab)
{
  return htab->collisions;
}

/* The following function returns number of searches during all work
   with all hash tables. */
static inline int
get_all_searches (void)
{
    return 0; // all_searches; TODO FREDRIK
}

/* The following function returns number of occurred collisions
   during all work with all hash tables. */
static inline int
get_all_collisions (void)
{
    return 0; // all_collisions; // TODO FREDRIK
}

int hash_table_collision_percentage (hash_table_t htab);

int all_hash_table_collision_percentage (void);

#else /* #ifndef NOT_DEFINED */



/* Hash tables are of the following class. */

class hash_table
{
  /* Current size (in entries) of the hash table */
  size_t _size;
  /* Current number of elements including also deleted elements */
  size_t number_of_elements;
  /* Current number of deleted elements in the table */
  size_t number_of_deleted_elements;
  /* The following member is used for debugging. Its value is number
     of all calls of `find_hash_table_entry' for the hash table. */
  int searches;
  /* The following member is used for debugging.  Its value is number
     of collisions fixed for time of work with the hash table. */
  int collisions;
  /* Pointer to function for evaluation of hash value (any unsigned value).
     This function has one parameter of type hash_table_entry_t. */
  unsigned (*hash_function) (hash_table_entry_t el_ptr);
  /* Pointer to function for test on equality of hash table elements (two
     parameter of type hash_table_entry_t. */
  int (*eq_function) (hash_table_entry_t el1_ptr,
                      hash_table_entry_t el2_ptr);
  /* Table itself */
  hash_table_entry_t *entries;
  /* Allocator */
  YaepAllocator * alloc;

  /* The following variable is used for debugging. Its value is number
     of all calls of `find_hash_table_entry' for all hash tables. */

  static int all_searches;

  /* The following variable is used for debugging. Its value is number
     of collisions fixed for time of work with all hash tables. */

  static int all_collisions;

public:

  /* Constructor. */
  hash_table( YaepAllocator * allocator, size_t size, unsigned int ( *hash_function )( hash_table_entry_t el_ptr ), int ( *eq_function )( hash_table_entry_t el1_ptr, hash_table_entry_t el2_ptr ) );
  /* Destructor. */
  ~hash_table (void);

  void empty (void);

  hash_table_entry_t *find_entry (hash_table_entry_t element, int reserve);

  void remove_element_from_entry (hash_table_entry_t element);

  /* The following function returns current size of given hash
     table. */

  inline size_t size (void)
    {
      return _size;
    }

  /* The following function returns current number of elements in
     given hash table. */

  inline size_t elements_number (void)
    {
      return number_of_elements - number_of_deleted_elements;
    }

  /* The following function returns number of searches during all work
     with given hash table. */

  inline int get_searches (void)
    {
      return this->searches;
    }

  /* The following function returns number of occurred collisions
     during all work with given hash table. */

  inline int get_collisions (void)
    {
      return this->collisions;
    }

  /* The following function returns number of searches
     during all work with all hash tables. */

  static inline int get_all_searches (void)
    {
      return all_searches;
    }

  /* The following function returns number of occurred collisions
     during all work with all hash tables. */

  static inline int get_all_collisions (void)
    {
      return all_collisions;
    }
private:

  void expand_hash_table (void);

};

typedef class hash_table *hash_table_t;


#endif /* #ifndef NOT_DEFINED */

#endif /* #ifndef __HASH_TABLE__ */
/*
   YAEP (Yet Another Earley Parser)

   Copyright (c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>

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

/* FILE NAME:   objstack.h

   TITLE:       Include file of package for work with stacks of
                objects (OS)

   DESCRIPTION: The package `objstack' implements efficient work
       with stacks of objects (OS).  Work with the object on the stack
       top is analogous to one with a variable length object.  One
       motivation for the package is the problem of growing char
       strings in symbol tables.  Memory for OS is allocated by
       segments.  A segment may contain more one objects.  The most
       recently allocated segment contains object on the top of OS.
       If there is not sufficient free memory for the top object than
       new segment is created and the top object is transferred into
       the new segment, i.e. there is not any memory reallocation.
       Therefore the top object may change its address.  But other
       objects never change address.

   SPECIAL CONSIDERATION:
         Defining macro `NDEBUG' (e.g. by option `-D' in C/C++
       compiler command line) before the package macros usage disables
       to fix some internal errors and errors of usage of the package.
         A file using the package can be compiled with option
       `-DOS_DEFAULT_SEGMENT_LENGTH=...'.
         C: Because arguments of all macros which return a result
       (`OS_TOP_LENGTH', `OS_TOP_BEGIN', and `OS_TOP_END') may be
       evaluated many times no side-effects should be in the
       arguments.

*/


#ifndef __OS__
#define __OS__

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

//include "allocate.h"

#include <assert.h>

/* This auxiliary structure is used to evaluation of maximum
   alignment for objects. */

struct _os_auxiliary_struct
{
  char _os_character;
  double _os_double;
};

/* This macro is auxiliary.  Its value is maximum alignment for objects. */

#ifdef VMS
/* It is necessarry for VMS C compiler. */
#define _OS_ALIGNMENT  4
#else
#define _OS_ALIGNMENT offsetof (struct _os_auxiliary_struct, _os_double)
#if 0
#define _OS_ALIGNMENT\
  ((char *) &((struct _os_auxiliary_struct *) 64)->_os_double - (char *) 64)
#else
#endif
#endif

/* This macro is auxiliary.  Its value is aligned address nearest to `a'. */

#define _OS_ALIGNED_ADDRESS(a)\
  ((void *) ((size_t) ((char *) (a) + (_OS_ALIGNMENT - 1))\
             & (~(size_t) (_OS_ALIGNMENT - 1))))

/* This macro value is default size of memory segments which will be
   allocated for OS when the stack is created (with zero initial
   segment size).  This is also minimal size of all segments.
   Original value of the macros is equal to 512.  This macro can be
   redefined in C compiler command line or with the aid of directive
   `#undef' before any using the package macros.  */

#ifndef OS_DEFAULT_SEGMENT_LENGTH
#define OS_DEFAULT_SEGMENT_LENGTH 512
#endif



#ifndef NOT_DEFINED

/* This internal structure describes segment of an object stack. */

struct _os_segment
{
  struct _os_segment *os_previous_segment;
  char os_segment_contest[_OS_ALIGNMENT];
};

/* This type describes a descriptor of stack of objects.  All work
   with stack of objects is executed by following macros through the
   descriptors.  Structure (implementation) of this type is not needed
   for using stack of objects.  But it should remember that work with
   the stack through several descriptors is not safe. */

typedef struct
{
  /* The real length of the first memory segment. */
  size_t initial_segment_length;
  struct _os_segment *os_current_segment;
  /* Pointer to memory currently used for storing the top object. */
  char *os_top_object_start;
  /* Pointer to first byte after the last byte of the top object. */
  char *os_top_object_free;
  /* Pointer to first byte after the memory allocated for storing
     the OS segment and the top object. */
  char *os_boundary;
  /* Pointer to allocator. */
  YaepAllocator *os_alloc;
} os_t;


/* This macro creates OS which contains the single zero length object.
   If initial length of OS segment is equal to zero the allocated
   memory segments length is equal to `OS_DEFAULT_SEGMENT_LENGTH'.
   But in any case the segment length is always not less than maximum
   alignment.  OS must be created before any using other macros of the
   package for work with given OS.  The macro has not side effects. */

#define OS_CREATE( os, allocator, initial_segment_length ) \
  do { \
    os_t * _temp_os = &( os ); \
    _temp_os->os_alloc = allocator; \
    _OS_create_function( _temp_os, initial_segment_length ); \
  } while( 0 )

/* This macro is used for freeing memory allocated for OS.  Any work
   (except for creation) with given OS is not possible after
   evaluation of this macros.  The macro has not side effects. */

#define OS_DELETE(os) _OS_delete_function (& (os))

/* This macro is used for freeing memory allocated for OS except for
   the first segment.  The macro has not side effects. */

#define OS_EMPTY(os) _OS_empty_function (& (os))

/* This macro makes that length of variable length object on the top
   of OS will be equal to zero.  The macro has not side effects. */

#define OS_TOP_NULLIFY(os)\
  do\
  {\
    os_t *_temp_os = &(os);\
    assert (_temp_os->os_top_object_start != NULL);\
    _temp_os->os_top_object_free = _temp_os->os_top_object_start;\
  }\
  while (0)

/* The macro creates new variable length object with initial zero
   length on the top of OS .  The work (analogous to one with variable
   length object) with object which was on the top of OS are finished,
   i.e. the object will never more change address.  The macro has not
   side effects. */

#define OS_TOP_FINISH(os)\
  do\
  {\
    os_t *_temp_os = &(os);\
    assert (_temp_os->os_top_object_start != NULL);\
    _temp_os->os_top_object_start\
        = (char *) _OS_ALIGNED_ADDRESS (_temp_os->os_top_object_free);    \
    _temp_os->os_top_object_free = _temp_os->os_top_object_start;\
  }\
  while (0)

/* This macro returns current length of variable length object on the
   top of OS.  The macro has side effects! */

#ifndef NDEBUG
#define OS_TOP_LENGTH(os)\
  ((os).os_top_object_start != NULL\
   ? (os).os_top_object_free - (os).os_top_object_start\
   : (abort (), 0))
#else
#define OS_TOP_LENGTH(os)  ((os).os_top_object_free - (os).os_top_object_start)
#endif

/* This macro returns pointer to the first byte of variable length
   object on the top of OS.  The macro has side effects!  Remember also
   that the top object may change own place after any addition. */

#ifndef NDEBUG
#define OS_TOP_BEGIN(os)\
  ((os).os_top_object_start != NULL ? (void *) (os).os_top_object_start\
                                    : (abort (), (void *) 0))
#else
#define OS_TOP_BEGIN(os) ((void *) (os).os_top_object_start)
#endif

/* This macro returns pointer (of type `void *') to the last byte of
   variable length object on the top OS.  The macro has side effects!
   Remember also that the top object may change own place after any
   addition. */

#ifndef NDEBUG
#define OS_TOP_END(os)\
  ((os).os_top_object_start != NULL ? (void *) ((os).os_top_object_free - 1)\
                                    : (abort (), (void *) 0))
#else
#define OS_TOP_END(os) ((void *) ((os).os_top_object_free - 1))
#endif

/* This macro returns pointer (of type `void *') to the next byte of
   the last byte of variable length object on the top OS.  The macro
   has side effects!  Remember also that the top object may change own
   place after any addition. */

#ifndef NDEBUG
#define OS_TOP_BOUND(os)\
  ((os).os_top_object_start != NULL ? (void *) (os).os_top_object_free\
                                    : (abort (), (void *) 0))
#else
#define OS_TOP_BOUND(os) ((void *) (os).os_top_object_free)
#endif

/* This macro removes N bytes from the end of variable length object
   on the top of OS.  The top variable length object is nullified if
   its length is less than N.  The macro has not side effects. */

#define OS_TOP_SHORTEN(os, n)\
  do\
  {\
    os_t *_temp_os = &(os);\
    size_t _temp_n = (n);\
    assert (_temp_os->os_top_object_start != NULL);\
    if ((size_t) OS_TOP_LENGTH (*_temp_os) < _temp_n)\
      _temp_os->os_top_object_free = _temp_os->os_top_object_start;\
    else\
      _temp_os->os_top_object_free -= _temp_n;\
  }\
  while (0)

/* This macro increases length of variable length object on the top of
   OS on given number of bytes.  The values of bytes added to the end
   of variable length object on the top of OS will be not defined.
   The macro has not side effects. */

#define OS_TOP_EXPAND(os, length)\
  do\
  {\
    os_t *_temp_os = &(os);\
    size_t _temp_length = (length);\
    assert (_temp_os->os_top_object_start != NULL);\
    if (_temp_os->os_top_object_free + _temp_length > _temp_os->os_boundary)\
      _OS_expand_memory (_temp_os, _temp_length);\
    _temp_os->os_top_object_free += _temp_length;\
  }\
  while (0)

/* This macro adds byte to the end of variable length object on the
   top of OS.  The macro has not side effects. */

#define OS_TOP_ADD_BYTE(os, b)\
  do\
  {\
    os_t *_temp_os = &(os);\
    assert (_temp_os->os_top_object_start != NULL);\
    if (_temp_os->os_top_object_free >= _temp_os->os_boundary)\
      _OS_expand_memory (_temp_os, 1);\
    *_temp_os->os_top_object_free++ = (b);\
  }\
  while (0)

/* This macro adds memory bytes to the end of variable length object
   on the top of OS.  The macro has not side effects. */

#define OS_TOP_ADD_MEMORY(os, str, length)\
  do\
  {\
    os_t *_temp_os = &(os);\
    size_t _temp_length = (length);\
    assert (_temp_os->os_top_object_start != NULL);\
    if (_temp_os->os_top_object_free + _temp_length > _temp_os->os_boundary)\
      _OS_expand_memory (_temp_os, _temp_length);\
    memcpy( _temp_os->os_top_object_free, ( str ), _temp_length ); \
    _temp_os->os_top_object_free += _temp_length;\
  }\
  while (0)

/* This macro adds C string (with end marker '\0') to the end of
   variable length object on the top of OS.  Before the addition the
   macro delete last character of the object.  The last character is
   suggested to be C string end marker '\0'.  The macro has not side
   effects. */

#define OS_TOP_ADD_STRING(os, str) _OS_add_string_function(&(os), (str))

/* The following functions are to be used only by the package macros.
   Remember that they are internal functions - all work with OS is
   executed through the macros. */

void _OS_create_function (os_t *os, size_t initial_segment_length);
void _OS_delete_function (os_t *os);
void _OS_empty_function (os_t *os);
void _OS_add_string_function (os_t *os, const char *str);
void _OS_expand_memory (os_t *os, size_t additional_length);

#else /* #ifndef NOT_DEFINED */

/* This class describes a descriptor of stack of objects.  All work
   with stack of objects is executed by member functions.  It should
   remember that work with the stack through several descriptors is
   not safe. */

class os
{
  /* The real length of the first memory segment. */
  size_t initial_segment_length;
  class _os_segment *os_current_segment;
  /* Pointer to memory currently used for storing the top object. */
  char *os_top_object_start;
  /* Pointer to the first byte after the last byte of the top
     object. */
  char *os_top_object_free;
  /* Pointer to first byte after the memory allocated for storing the
     OS segment and the top object. */
  char *os_boundary;
  /* Pointer to allocator. */
  YaepAllocator *os_alloc;

public:

  /* These constructors create OS which contains the single zero
     length object.  If initial length of OS segment is equal to zero
     or absent, the allocated memory segments length is equal to
     `OS_DEFAULT_SEGMENT_LENGTH'.  But in any case the segment length
     is always not less than maximum alignment. */

    explicit os (YaepAllocator * allocator, size_t initial_segment_length =
		 OS_DEFAULT_SEGMENT_LENGTH);

  /* This destructor is used for freeing memory allocated for OS. */

   ~os (void);

  /* This function is used for freeing memory allocated for OS except
     for the first segment. */

  void empty (void);

  /* This function makes that length of variable length object on the
     top of OS will be equal to zero. */

  inline void top_nullify (void)
  {
    assert (os_top_object_start != NULL);
    os_top_object_free = os_top_object_start;
  }

  /* The function creates new variable length object with initial zero
     length on the top of OS.  The work (analogous to one with
     variable length object) with object which was on the top of OS
     are finished, i.e. the object will never more change address. */

  inline void top_finish (void)
  {
    assert (os_top_object_start != NULL);
    os_top_object_start = (char *) _OS_ALIGNED_ADDRESS (os_top_object_free);
    os_top_object_free = os_top_object_start;
  }

  /* This function returns current length of variable length object on
     the top of OS. */

  inline size_t top_length (void)
  {
#ifndef NDEBUG
    return (os_top_object_start != NULL
	    ? os_top_object_free - os_top_object_start : (abort (), 0));
#else
    return (os_top_object_free - os_top_object_start);
#endif
  }

  /* This function returns pointer to the first byte of variable
     length object on the top of OS.  Remember also that the top
     object may change own place after any addition. */

  inline void *top_begin (void)
  {
#ifndef NDEBUG
    return (os_top_object_start != NULL
	    ? (void *) os_top_object_start : (abort (), (void *) 0));
#else
    return ((void *) os_top_object_start);
#endif
  }

  /* This function returns pointer (of type `void *') to the last byte
     of variable length object on the top OS.  Remember also that the
     top object may change own place after any addition. */

  inline void *top_end (void)
  {
#ifndef NDEBUG
    return (os_top_object_start != NULL
	    ? (void *) (os_top_object_free - 1) : (abort (), (void *) 0));
#else
    return ((void *) (os_top_object_free - 1));
#endif
  }

  /* This function returns pointer (of type `void *') to the next byte
     of the last byte of variable length object on the top OS.
     Remember also that the top object may change own place after any
     addition. */

  inline void *top_bound (void)
  {
#ifndef NDEBUG
    return (os_top_object_start != NULL
	    ? (void *) os_top_object_free : (abort (), (void *) 0));
#else
    return ((void *) os_top_object_free);
#endif
  }

  /* This function removes N bytes from the end of variable length
     object on the top of OS.  The top variable length object is
     nullified if its length is less than N. */

  inline void top_shorten (size_t n)
  {
    assert (os_top_object_start != NULL);
    if (top_length () < n)
      os_top_object_free = os_top_object_start;
    else
      os_top_object_free -= n;
  }

  /* This function increases length of variable length object on the
     top of OS on given number of bytes.  The values of bytes added to
     the end of variable length object on the top of OS will be not
     defined. */

  inline void top_expand (size_t length)
  {
    assert (os_top_object_start != NULL);
    if (os_top_object_free + length > os_boundary)
      _OS_expand_memory (length);
    os_top_object_free += length;
  }

  /* This function adds byte to the end of variable length object on
     the top of OS. */

  inline void top_add_byte (int b)
  {
    assert (os_top_object_start != NULL);
    if (os_top_object_free >= os_boundary)
      _OS_expand_memory (1);
    *os_top_object_free++ = b;
  }

  /* This function adds memory bytes to the end of variable length
     object on the top of OS. */

  inline void top_add_memory (const void *str, size_t length)
  {
    assert (os_top_object_start != NULL);
    if (os_top_object_free + length > os_boundary)
      _OS_expand_memory (length);
    memcpy (os_top_object_free, str, length);
    os_top_object_free += length;
  }

  /* This function adds C string (with end marker '\0') to the end of
     variable length object on the top of OS.  Before the addition the
     macro delete last character of the object.  The last character is
     suggested to be C string end marker '\0'. */

  void top_add_string (const char *str);

  /* The following function is used only by the package functions. */

  void _OS_expand_memory (size_t additional_length);
};

typedef os os_t;

/* This internal structure describes segment of an object stack. */

class _os_segment
{
  class _os_segment *os_previous_segment;
  char os_segment_contest[_OS_ALIGNMENT];
  friend os::os (YaepAllocator *, size_t);
  friend os::~os (void);
  friend void os::empty (void);
  friend void os::_OS_expand_memory (size_t additional_length);
};

#endif /* #ifndef NOT_DEFINED */

#endif /* #ifndef __OS__ */
/*
   YAEP (Yet Another Earley Parser)

   Copyright (c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>

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

/* FILE NAME:   vlobject.h

   TITLE:       Include file of package for work with variable length
                objects (VLO)

   DESCRIPTION: This header file contains macros and C++ class for
       work with variable length objects (VLO).  Any number of bytes
       my be added to and removed from the end of VLO.  If it is
       needed the memory allocated for storing variable length object
       may be expanded possibly with changing the object place.  But
       between any additions of the bytes (tailoring) the object place
       is not changed.  To decrease number of changes of the object
       place the memory being allocated for the object is longer than
       the current object length.

   SPECIAL CONSIDERATION:
         Defining macro `NDEBUG' (e.g. by option `-D' in C/C++
       compiler command line) before the package macros usage disables
       to fix some internal errors and errors of usage of the package.
         C: Because arguments of all macros which return a result
       (`VLO_LENGTH', `VLO_BEGIN', and `VLO_END') may be evaluated
       many times no side-effects should be in the arguments.  A file
       using the package can be compiled with option
       `-DVLO_DEFAULT_LENGTH=...'.  */


#ifndef __VLO__
#define __VLO__

#include <stdlib.h>
#include <string.h>

//include "allocate.h"

#include <assert.h>


/* Default initial size of memory is allocated for VLO when the object
   is created (with zero initial size).  This macro can be redefined
   in C compiler command line or with the aid of directive `#undef'
   before any using the package macros. */

#ifndef VLO_DEFAULT_LENGTH
#define VLO_DEFAULT_LENGTH 512
#endif


#ifndef NOT_DEFINED


/* This type describes a descriptor of variable length object.  All
   work with variable length object is executed by following macros
   through the descriptors.  Structure (implementation) of this type
   is not needed for using variable length object.  But it should
   remember that work with the object through several descriptors is
   not safe. */

typedef struct
{
  /* Pointer to memory currently used for storing the VLO. */
  char *vlo_start;
  /* Pointer to first byte after the last VLO byte. */
  char *vlo_free;
  /* Pointer to first byte after the memory currently allocated for storing
     the VLO. */
  char *vlo_boundary;
  /* Pointer to allocator. */
  YaepAllocator *vlo_alloc;
} vlo_t;


/* This macro is used for creation of VLO with initial zero length.
   If initial length of memory needed for the VLO is equal to 0 the
   initial allocated memory length is equal to VLO_DEFAULT_LENGTH.
   VLO must be created before any using other macros of the package
   for work with given VLO.  The macro has not side effects. */

#define VLO_CREATE(vlo, allocator, initial_length)\
  do\
  {\
    vlo_t *_temp_vlo = &(vlo);\
    size_t temp_initial_length = (initial_length);\
    YaepAllocator * _temp_alloc = ( allocator ); \
    temp_initial_length = (temp_initial_length != 0 ? temp_initial_length\
                                                    : VLO_DEFAULT_LENGTH);\
    _temp_vlo->vlo_start = (char*)yaep_malloc( _temp_alloc, temp_initial_length ); \
    _temp_vlo->vlo_boundary = _temp_vlo->vlo_start + temp_initial_length;\
    _temp_vlo->vlo_free = _temp_vlo->vlo_start;\
    _temp_vlo->vlo_alloc = _temp_alloc; \
  }\
  while (0)


/* This macro is used for freeing memory allocated for VLO.  Any work
   (except for creation) with given VLO is not possible after
   evaluation of this macro.  The macro has not side effects. */

#ifndef NDEBUG
#define VLO_DELETE(vlo)\
  do\
  {\
    vlo_t *_temp_vlo = &(vlo);\
    assert (_temp_vlo->vlo_start != NULL);\
    yaep_free( _temp_vlo->vlo_alloc,_temp_vlo->vlo_start );\
    _temp_vlo->vlo_start = NULL;\
  }\
  while (0)
#else
#define VLO_DELETE(vlo) \
  do { \
    vlo_t * _temp_vlo = &( vlo ); \
    yaep_free( _temp_vlo->vlo_alloc, _temp_vlo->vlo_start ); \
  } while( 0 )
#endif /* #ifndef NDEBUG */

/* This macro makes that length of VLO will be equal to zero (but
   memory for VLO is not freed and not reallocated).  The macro has
   not side effects. */

#define VLO_NULLIFY(vlo)\
  do\
  {\
    vlo_t *_temp_vlo = &(vlo);\
    assert (_temp_vlo->vlo_start != NULL);\
    _temp_vlo->vlo_free = _temp_vlo->vlo_start;\
  }\
  while (0)


/* The following macro makes that length of memory allocated for VLO
   becames equal to VLO length.  The macro has not side effects. */

#define VLO_TAILOR(vlo) _VLO_tailor_function(&(vlo))


/* This macro returns current length of VLO.  The macro has side
   effects! */

#ifndef NDEBUG
#define VLO_LENGTH(vlo) ((vlo).vlo_start != NULL\
                         ? (vlo).vlo_free - (vlo).vlo_start\
                         : (abort (), 0))
#else
#define VLO_LENGTH(vlo) ((vlo).vlo_free - (vlo).vlo_start)
#endif /* #ifndef NDEBUG */


/* This macro returns pointer (of type `void *') to the first byte of
   the VLO.  The macro has side effects!  Remember also that the VLO
   may change own place after any addition. */

#ifndef NDEBUG
#define VLO_BEGIN(vlo) ((vlo).vlo_start != NULL\
                        ? (void *) (vlo).vlo_start\
                        : (abort (), (void *) 0))
#else
#define VLO_BEGIN(vlo) ((void *) (vlo).vlo_start)
#endif /* #ifndef NDEBUG */

/* This macro returns pointer (of type `void *') to the last byte of
   VLO.  The macro has side effects!  Remember also that the VLO may
   change own place after any addition. */

#ifndef NDEBUG
#define VLO_END(vlo) ((vlo).vlo_start != NULL\
                      ? (void *) ((vlo).vlo_free - 1)\
                      : (abort (), (void *) 0))
#else
#define VLO_END(vlo) ((void *) ((vlo).vlo_free - 1))
#endif /* #ifndef NDEBUG */

/* This macro returns pointer (of type `void *') to the next byte of
   the last byte of VLO.  The macro has side effects!  Remember also
   that the VLO may change own place after any addition. */

#ifndef NDEBUG
#define VLO_BOUND(vlo) ((vlo).vlo_start != NULL\
                        ? (void *) (vlo).vlo_free\
                        : (abort (), (void *) 0))
#else
#define VLO_BOUND(vlo) ((void *) (vlo).vlo_free)
#endif /* #ifndef NDEBUG */

/* This macro removes N bytes from the end of VLO.  VLO is nullified
   if its length is less than N.  The macro has not side effects. */

#define VLO_SHORTEN(vlo, n)\
  do\
  {\
    vlo_t *_temp_vlo = &(vlo);\
    size_t _temp_n = (n);\
    assert (_temp_vlo->vlo_start != NULL);\
    if ((size_t) VLO_LENGTH (*_temp_vlo) < _temp_n)\
      _temp_vlo->vlo_free = _temp_vlo->vlo_start;\
    else\
      _temp_vlo->vlo_free -= _temp_n;\
  }\
  while (0)


/* This macro increases length of VLO.  The values of bytes added to
   the end of VLO will be not defined.  The macro has not side
   effects. */

#define VLO_EXPAND(vlo, length)\
  do\
  {\
    vlo_t *_temp_vlo = &(vlo);\
    size_t _temp_length = (length);\
    assert (_temp_vlo->vlo_start != NULL);\
    if (_temp_vlo->vlo_free + _temp_length > _temp_vlo->vlo_boundary)\
      _VLO_expand_memory (_temp_vlo, _temp_length);\
    _temp_vlo->vlo_free += _temp_length;\
  }\
  while (0)


/* This macro adds a byte to the end of VLO.  The macro has not side
   effects. */

#define VLO_ADD_BYTE(vlo, b)\
  do\
  {\
    vlo_t *_temp_vlo = &(vlo);\
    assert (_temp_vlo->vlo_start != NULL);\
    if (_temp_vlo->vlo_free >= _temp_vlo->vlo_boundary)\
      _VLO_expand_memory (_temp_vlo, 1);\
    *_temp_vlo->vlo_free++ = (b);\
  }\
  while (0)


/* This macro adds memory bytes to the end of VLO.  The macro has not
   side effects. */

#define VLO_ADD_MEMORY(vlo, str, length)\
  do\
  {\
    vlo_t *_temp_vlo = &(vlo);\
    size_t _temp_length = (length);\
    assert (_temp_vlo->vlo_start != NULL);\
    if (_temp_vlo->vlo_free + _temp_length > _temp_vlo->vlo_boundary)\
      _VLO_expand_memory (_temp_vlo, _temp_length);\
    memcpy( _temp_vlo->vlo_free, ( str ), _temp_length ); \
    _temp_vlo->vlo_free += _temp_length;\
  }\
  while (0)


/* This macro adds C string (with end marker '\0') to the end of VLO.
   Before the addition the macro delete last character of the VLO.
   The last character is suggested to be C string end marker '\0'.
   The macro has not side effects. */

#define VLO_ADD_STRING(vlo, str) _VLO_add_string_function(&(vlo), (str))


/* The following functions are to be used only by the package macros.
   Remember that they are internal functions - all work with VLO is
   executed through the macros. */

void _VLO_tailor_function (vlo_t *vlo);
void _VLO_add_string_function (vlo_t *vlo, const char *str);
void _VLO_expand_memory (vlo_t *vlo, size_t additional_length);

#else /* #ifndef NOT_DEFINED */


/* This type describes a descriptor of variable length object.  It
   should remember that work with the object through several
   descriptors is not safe. */

class vlo
{
  /* Pointer to memory currently used for storing the VLO. */
  char *vlo_start;
  /* Pointer to first byte after the last VLO byte. */
  char *vlo_free;
  /* Pointer to first byte after the memory currently allocated for storing
     the VLO. */
  char *vlo_boundary;
  /* Pointer to allocator. */
  YaepAllocator *vlo_alloc;
public:

  /* This function is used for creation of VLO with initial zero
     length.  If initial length of memory needed for the VLO is equal
     to 0 the initial allocated memory length is equal to
     VLO_DEFAULT_LENGTH. */

  explicit vlo (YaepAllocator * allocator, size_t initial_length = VLO_DEFAULT_LENGTH):vlo_alloc (allocator)
  {
    initial_length = (initial_length != 0
		      ? initial_length : VLO_DEFAULT_LENGTH);
    vlo_start = (char *) yaep_malloc (vlo_alloc, initial_length);
    vlo_boundary = vlo_start + initial_length;
    vlo_free = vlo_start;
  }


  /* This function is used for freeing memory allocated for VLO.  Any
     work (except for creation) with given VLO is not possible after
     evaluation of this function. */

  inline ~ vlo (void)
  {
#ifndef NDEBUG
    assert (vlo_start != NULL);
    yaep_free (vlo_alloc, vlo_start);
    vlo_start = NULL;
#else
    yaep_free (vlo_alloc, vlo_start);
#endif /* #ifndef NDEBUG */
  }

  /* This function makes that length of VLO will be equal to zero (but
     memory for VLO is not freed and not reallocated). */

  inline void nullify (void)
  {
    assert (vlo_start != NULL);
    vlo_free = vlo_start;
  }


  /* The following function makes that length of memory allocated for
     VLO becames equal to VLO length. */

  void tailor (void);


  /* This function returns current length of VLO. */

  inline size_t length (void)
  {
    assert (vlo_start != NULL);
    return vlo_free - vlo_start;
  }


  /* This function returns pointer (of type `void *') to the first byte
     of the VLO.  Remember also that the VLO may change own place
     after any addition. */

  inline void *begin (void)
  {
    assert (vlo_start != NULL);
    return (void *) vlo_start;
  }

  /* This function returns pointer (of type `void *') to the last byte
     of VLO.  Remember also that the VLO may change own place after
     any addition. */

  inline void *end (void)
  {
    assert (vlo_start != NULL);
    return (void *) (vlo_free - 1);
  }

  /* This function returns pointer (of type `void *') to the next byte
     of the last byte of VLO.  Remember also that the VLO may change
     own place after any addition. */

  inline void *bound (void)
  {
    assert (vlo_start != NULL);
    return (void *) vlo_free;
  }

  /* This function removes N bytes from the end of VLO.  VLO is nullified
     if its length is less than N. */

  inline void shorten (size_t n)
  {
    assert (vlo_start != NULL);
    if (length () < n)
      vlo_free = vlo_start;
    else
      vlo_free -= n;
  }


  /* This function increases length of VLO.  The values of bytes added
     to the end of VLO will be not defined. */

  void expand (size_t length)
  {
    assert (vlo_start != NULL);
    if (vlo_free + length > vlo_boundary)
      _VLO_expand_memory (length);
    vlo_free += length;
  }


  /* This function adds a byte to the end of VLO. */

  inline void add_byte (int b)
  {
    assert (vlo_start != NULL);
    if (vlo_free >= vlo_boundary)
      _VLO_expand_memory (1);
    *vlo_free++ = b;
  }


  /* This function adds memory bytes to the end of VLO. */

  inline void add_memory (const void *str, size_t length)
  {
    assert (vlo_start != NULL);
    if (vlo_free + length > vlo_boundary)
      _VLO_expand_memory (length);
    memcpy (vlo_free, str, length);
    vlo_free += length;
  }


  /* This function adds C string (with end marker '\0') to the end of
     VLO.  Before the addition the function deletes last character of
     the VLO.  The last character is suggested to be C string end
     marker '\0'. */

  void add_string (const char *str);

private:

  /* The following functions is used only by the class functions. */

  void _VLO_expand_memory (size_t additional_length);
};

typedef vlo vlo_t;

#endif /* #ifndef NOT_DEFINED */

#endif /* #ifndef __VLO__ */
/****************** YAEP parser single source file code **********************/
/*
   YAEP (Yet Another Earley Parser)

   Copyright (c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>

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

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

//include"allocate.h"

struct YaepAllocator
{
  Yaep_malloc malloc;
  Yaep_calloc calloc;
  Yaep_realloc realloc;
  Yaep_free free;
  Yaep_alloc_error alloc_error;
  void *userptr;
};

void
yaep_alloc_defaulterrfunc (void *ignored)
{
  (void) ignored;

  fputs ("*** out of memory ***\n", stderr);
  exit (EXIT_FAILURE);
}

struct YaepAllocator *
yaep_alloc_new (Yaep_malloc mallocf, Yaep_calloc callocf,
		Yaep_realloc reallocf, Yaep_free freef)
{
  struct YaepAllocator *result;

  /* Sanity checks */
  if (mallocf == NULL)
    mallocf = malloc;
  if ((callocf == NULL) && (mallocf == malloc))
    callocf = calloc;
  if (reallocf == NULL)
    {
      if ((mallocf == malloc) && (callocf == calloc))
	reallocf = realloc;
      else
	return NULL;
    }
  if (freef == NULL)
    {
      if ((mallocf == malloc) && (callocf == calloc) && (reallocf == realloc))
	freef = free;
      else
	return NULL;
    }

  /* Allocate allocator */
  result = (struct YaepAllocator*)mallocf (sizeof (*result));
  if (result == NULL)
    return NULL;
  result->malloc = mallocf;
  result->calloc = callocf;
  result->realloc = reallocf;
  result->free = freef;
  result->alloc_error = yaep_alloc_defaulterrfunc;
  result->userptr = result;

  return result;
}

void
yaep_alloc_del (struct YaepAllocator *allocator)
{
  if (allocator != NULL)
    {
      Yaep_free freef = allocator->free;

      freef (allocator);
    }
}

void *
yaep_malloc (struct YaepAllocator *allocator, size_t size)
{
  void *result;

  if (allocator == NULL)
    return NULL;

  result = allocator->malloc (size);
  if ((result == NULL) && (size != 0))
    allocator->alloc_error (allocator->userptr);

  return result;
}

void *
yaep_calloc (struct YaepAllocator *allocator, size_t nmemb, size_t size)
{
  void *result;

  if (allocator == NULL)
    return NULL;

  if (allocator->calloc != NULL)
    {
      result = allocator->calloc (nmemb, size);
    }
  else if ((nmemb == 0) || (size == 0))
    {
      result = NULL;
    }
  else
    {
      size_t total = nmemb * size;
      if (total / nmemb != size)
	result = NULL;
      else
	{
	  result = allocator->malloc (total);
	  if (result != NULL)
	    memset (result, '\0', total);
	}
    }

  if ((result == NULL) && (nmemb != 0) && (size != 0))
    allocator->alloc_error (allocator->userptr);

  return result;
}

void *
yaep_realloc (struct YaepAllocator *allocator, void *ptr, size_t size)
{
  void *result;

  if (allocator == NULL)
    return NULL;

  result = allocator->realloc (ptr, size);
  if ((result == NULL) && (size != 0))
    allocator->alloc_error (allocator->userptr);

  return result;
}

void
yaep_free (struct YaepAllocator *allocator, void *ptr)
{
  if (allocator != NULL)
    allocator->free (ptr);
}

Yaep_alloc_error
yaep_alloc_geterrfunc (YaepAllocator * allocator)
{
  if (allocator != NULL)
    return allocator->alloc_error;
  else
    return NULL;
}

void *
yaep_alloc_getuserptr (YaepAllocator * allocator)
{
  if (allocator != NULL)
    return allocator->userptr;
  else
    return NULL;
}

void
yaep_alloc_seterr (YaepAllocator * allocator, Yaep_alloc_error errfunc,
		   void *userptr)
{
  if (allocator != NULL)
    {
      if (errfunc != NULL)
	allocator->alloc_error = errfunc;
      else
	allocator->alloc_error = yaep_alloc_defaulterrfunc;
      allocator->userptr = userptr;
    }
}
/*
   YAEP (Yet Another Earley Parser)

   Copyright (c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>

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

/* FILE NAME:   hashtab.c

   TITLE:       Package for work with hash tables

   DESCRIPTION: This package implements features analogous to ones of
       public domain functions `hsearch', `hcreate' and `hdestroy'.
       The goal of the package creation is to implement additional
       needed features.  The abstract data permits to work
       simultaneously with several expandable hash tables.  Besides
       insertion and search of elements the elements from the hash
       tables can be also removed.  The table element can be only a
       pointer.  The size of hash tables is not fixed.  The hash table
       will be expanded when its occupancy will became big.  The
       abstract data implementation is based on generalized Algorithm
       D from Knuth's book "The art of computer programming".  Hash
       table is expanded by creation of new hash table and
       transferring elements from the old table to the new table.

   SPECIAL CONSIDERATION:
         Defining macro `NDEBUG' (e.g. by option `-D' in C compiler
       command line) during the file compilation disables to fix
       some internal errors and errors of usage of the package.

*/

//include "allocate.h"
//include "hashtab.h"

#include <assert.h>

/* This macro defines reserved value for empty table entry. */

#define EMPTY_ENTRY    NULL

/* This macro defines reserved value for table entry which contained
   a deleted element. */

#define DELETED_ENTRY  ((void *) 1)

/* The following function returns the nearest prime number which is
   greater than given source number. */

static unsigned long
higher_prime_number (unsigned long number)
{
  unsigned long i;

  for (number = (number / 2) * 2 + 3;; number += 2)
    {
      for (i = 3; i * i <= number; i += 2)
	if (number % i == 0)
	  break;
      if (i * i > number)
	return number;
    }
}

/* This function creates table with length slightly longer than given
   source length.  Created hash table is initiated as empty (all the
   hash table entries are EMPTY_ENTRY).  The function returns the
   created hash table. */

hash_table_t
create_hash_table (YaepAllocator * allocator, size_t size,
		   unsigned int (*hash_function) (hash_table_entry_t el_ptr),
		   int (*eq_function) (hash_table_entry_t el1_ptr,
				       hash_table_entry_t el2_ptr))
{
  hash_table_t result;
  hash_table_entry_t *entry_ptr;

  size = higher_prime_number (size);
  result = (hash_table_t)yaep_malloc (allocator, sizeof (*result));
  result->entries = (const void**)
    yaep_malloc (allocator, size * sizeof (hash_table_entry_t));
  result->size = size;
  result->hash_function = hash_function;
  result->eq_function = eq_function;
  result->number_of_elements = 0;
  result->number_of_deleted_elements = 0;
  result->searches = 0;
  result->collisions = 0;
  result->alloc = allocator;
  for (entry_ptr = result->entries;
       entry_ptr < result->entries + size; entry_ptr++)
    *entry_ptr = EMPTY_ENTRY;
  return result;
}

/* This function makes the table empty.  Naturally the hash table must
   already exist. */

void
empty_hash_table (hash_table_t htab)
{
  hash_table_entry_t *entry_ptr;

  assert (htab != NULL);
  htab->number_of_elements = 0;
  htab->number_of_deleted_elements = 0;
  for (entry_ptr = htab->entries;
       entry_ptr < htab->entries + htab->size; entry_ptr++)
    *entry_ptr = EMPTY_ENTRY;
}

/* This function frees all memory allocated for given hash table.
   Naturally the hash table must already exist. */

void
delete_hash_table (hash_table_t htab)
{
  assert (htab != NULL);
  yaep_free (htab->alloc, htab->entries);
  yaep_free (htab->alloc, htab);
}

/* The following function changes size of memory allocated for the
   entries and repeatedly inserts the table elements.  The occupancy
   of the table after the call will be about 50%.  Naturally the hash
   table must already exist.  Remember also that the place of the
   table entries is changed. */

static void
expand_hash_table (hash_table_t htab)
{
  hash_table_t new_htab;
  hash_table_entry_t *entry_ptr;
  hash_table_entry_t *new_entry_ptr;

  assert (htab != NULL);
  new_htab =
    create_hash_table (htab->alloc, htab->number_of_elements * 2,
		       htab->hash_function, htab->eq_function);
  for (entry_ptr = htab->entries; entry_ptr < htab->entries + htab->size;
       entry_ptr++)
    if (*entry_ptr != EMPTY_ENTRY && *entry_ptr != DELETED_ENTRY)
      {
	new_entry_ptr = find_hash_table_entry (new_htab, *entry_ptr,
					       1 /* TRUE */ );
	assert (*new_entry_ptr == EMPTY_ENTRY);
	*new_entry_ptr = (*entry_ptr);
      }
  yaep_free (htab->alloc, htab->entries);
  *htab = (*new_htab);
  yaep_free (new_htab->alloc, new_htab);
}

/* The following variable is used for debugging. Its value is number
   of all calls of `find_hash_table_entry' for all hash tables. */

int all_searches = 0;

/* The following variable is used for debugging. Its value is number
   of collisions fixed for time of work with all hash tables. */

int all_collisions = 0;

/* This function searches for hash table entry which contains element
   equal to given value or empty entry in which given value can be
   placed (if the element with given value does not exist in the
   table).  The function works in two regimes.  The first regime is
   used only for search.  The second is used for search and
   reservation empty entry for given value.  The table is expanded if
   occupancy (taking into accout also deleted elements) is more than
   75%.  Naturally the hash table must already exist.  If reservation
   flag is TRUE then the element with given value should be inserted
   into the table entry before another call of
   `find_hash_table_entry'. */

hash_table_entry_t *
find_hash_table_entry (hash_table_t htab,
		       hash_table_entry_t element, int reserve)
{
  hash_table_entry_t *entry_ptr;
  hash_table_entry_t *first_deleted_entry_ptr;
  unsigned hash_value, secondary_hash_value;

  assert (htab != NULL);
  if (htab->size / 4 <= htab->number_of_elements / 3)
    expand_hash_table (htab);
  hash_value = (*htab->hash_function) (element);
  secondary_hash_value = 1 + hash_value % (htab->size - 2);
  hash_value %= htab->size;
  htab->searches++;
  all_searches++;
  first_deleted_entry_ptr = NULL;
  for (;; htab->collisions++, all_collisions++)
    {
      entry_ptr = htab->entries + hash_value;
      if (*entry_ptr == EMPTY_ENTRY)
	{
	  if (reserve)
	    {
	      htab->number_of_elements++;
	      if (first_deleted_entry_ptr != NULL)
		{
		  entry_ptr = first_deleted_entry_ptr;
		  *entry_ptr = EMPTY_ENTRY;
		}
	    }
	  break;
	}
      else if (*entry_ptr != DELETED_ENTRY)
	{
	  if ((*htab->eq_function) (*entry_ptr, element))
	    break;
	}
      else if (first_deleted_entry_ptr == NULL)
	first_deleted_entry_ptr = entry_ptr;
      hash_value += secondary_hash_value;
      if (hash_value >= htab->size)
	hash_value -= htab->size;
    }
  return entry_ptr;
}

/* This function deletes element with given value from hash table.
   The hash table entry value will be `DELETED_ENTRY' after the
   function call.  Naturally the hash table must already exist.  Hash
   table entry for given value should be not empty (or deleted). */

void
remove_element_from_hash_table_entry (hash_table_t htab,
				      hash_table_entry_t element)
{
  hash_table_entry_t *entry_ptr;

  assert (htab != NULL);
  entry_ptr = find_hash_table_entry (htab, element, 0);
  assert (*entry_ptr != EMPTY_ENTRY && *entry_ptr != DELETED_ENTRY);
  *entry_ptr = DELETED_ENTRY;
  htab->number_of_deleted_elements++;
}

/* The following function returns current size of given hash table. */

size_t
hash_table_size (hash_table_t htab)
{
  assert (htab != NULL);
  return htab->size;
}

/* The following function returns current number of elements in given
   hash table. */

size_t
hash_table_elements_number (hash_table_t htab)
{
  assert (htab != NULL);
  return htab->number_of_elements - htab->number_of_deleted_elements;
}
/*
   YAEP (Yet Another Earley Parser)

   Copyright (c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>

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

/* FILE NAME:   objstack.c

   TITLE:       Package for work with stacks of objects (OS)

   DESCRIPTION:
       This file implements internal functions of the package.

   SPECIAL CONSIDERATION:
       The following functions are to be used only by the package
       macros.  Remember that they are internal functions - all work
       with OS is executed through the macros.
         Defining macro `NDEBUG' (e.g. by option `-D' in C compiler
       command line) during the file compilation disables to fix
       some internal errors and errors of usage of the package.

*/

#include <string.h>
//include "allocate.h"

//include "objstack.h"

#include <assert.h>

/* The function implements macro `OS_CREATE' (creation of stack of
   object).  OS must be created before any using other macros of the
   package for work with given OS. */

void
_OS_create_function (os_t * os, size_t initial_segment_length)
{
  if (initial_segment_length == 0)
    initial_segment_length = OS_DEFAULT_SEGMENT_LENGTH;
  os->os_current_segment = (struct _os_segment*)
    yaep_malloc (os->os_alloc,
		 initial_segment_length + sizeof (struct _os_segment));
  os->os_current_segment->os_previous_segment = NULL;
  os->os_top_object_start
    =
    (char *) _OS_ALIGNED_ADDRESS (os->os_current_segment->os_segment_contest);
  os->os_top_object_free = os->os_top_object_start;
  os->os_boundary = os->os_top_object_start + initial_segment_length;
  os->initial_segment_length = initial_segment_length;
}

/* The function implements macro `OS_DELETE' (freeing memory allocated
   for OS).  Any work (except for creation) with given OS is not
   possible after evaluation of this macros.  The macro has not side
   effects. */

void
_OS_delete_function (os_t * os)
{
  struct _os_segment *current_segment, *previous_segment;

  assert (os->os_top_object_start != NULL);
  os->os_top_object_start = NULL;
  for (current_segment = os->os_current_segment; current_segment != NULL;
       current_segment = previous_segment)
    {
      previous_segment = current_segment->os_previous_segment;
      yaep_free (os->os_alloc, current_segment);
    }
}

/* The following function implements macro `OS_EMPTY' (freeing memory
   allocated for OS except for the first segment). */

void
_OS_empty_function (os_t * os)
{
  struct _os_segment *current_segment, *previous_segment;

  assert (os->os_top_object_start != NULL && os->os_current_segment != NULL);
  current_segment = os->os_current_segment;
  for (;;)
    {
      previous_segment = current_segment->os_previous_segment;
      if (previous_segment == NULL)
	break;
      yaep_free (os->os_alloc, current_segment);
      current_segment = previous_segment;
    }
  os->os_current_segment = current_segment;
  os->os_top_object_start
    = (char *) _OS_ALIGNED_ADDRESS (current_segment->os_segment_contest);
  os->os_top_object_free = os->os_top_object_start;
  os->os_boundary = os->os_top_object_start + os->initial_segment_length;
}

/* The function implements macro `OS_ADD_STRING' (addition of string
   STR (with end marker is '\0') to the end of OS).  Remember that the
   OS place may be changed after the call. */

void
_OS_add_string_function (os_t * os, const char *str)
{
  size_t string_length;

  assert (os->os_top_object_start != NULL);
  if (str == NULL)
    return;
  if (os->os_top_object_free != os->os_top_object_start)
    OS_TOP_SHORTEN (*os, 1);
  string_length = strlen (str) + 1;
  if (os->os_top_object_free + string_length > os->os_boundary)
    _OS_expand_memory (os, string_length);
  memcpy( os->os_top_object_free, str, string_length );
  os->os_top_object_free = os->os_top_object_free + string_length;
}

/* The function creates new segment for OS.  The segment becames
   current and its size becames equal to about one and a half of the
   top object length accounting for length of memory which will be
   added after the call (but not less than the default segment
   length).  The function deletes the segment which was current if the
   segment contained only the top object.  Remember that the top
   object place may be changed after the call. */

void
_OS_expand_memory (os_t * os, size_t additional_length)
{
  size_t os_top_object_length, segment_length;
  struct _os_segment *new_segment, *previous_segment;
  char *new_os_top_object_start;

  assert (os->os_top_object_start != NULL);
  os_top_object_length = OS_TOP_LENGTH (*os);
  segment_length = os_top_object_length + additional_length;
  segment_length += segment_length / 2 + 1;
  if (segment_length < OS_DEFAULT_SEGMENT_LENGTH)
    segment_length = OS_DEFAULT_SEGMENT_LENGTH;
  new_segment = (struct _os_segment*)
    yaep_malloc (os->os_alloc, segment_length + sizeof (struct _os_segment));
  new_os_top_object_start =
    (char *) _OS_ALIGNED_ADDRESS (new_segment->os_segment_contest);
  memcpy (new_os_top_object_start, os->os_top_object_start,
	      os_top_object_length);
  if (os->os_top_object_start ==
      (char *) _OS_ALIGNED_ADDRESS (os->os_current_segment->
				    os_segment_contest))
    {
      previous_segment = os->os_current_segment->os_previous_segment;
      yaep_free (os->os_alloc, os->os_current_segment);
    }
  else
    previous_segment = os->os_current_segment;
  os->os_current_segment = new_segment;
  new_segment->os_previous_segment = previous_segment;
  os->os_top_object_start = new_os_top_object_start;
  os->os_top_object_free = os->os_top_object_start + os_top_object_length;
  os->os_boundary = os->os_top_object_start + segment_length;
}
/*
   YAEP (Yet Another Earley Parser)

   Copyright (c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>

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

#include <string.h>

//include "allocate.h"
//include "vlobject.h"

#include <assert.h>

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
      vlo->vlo_free += new_vlo_start - vlo->vlo_start;
      vlo->vlo_start = new_vlo_start;
    }
  vlo->vlo_boundary = vlo->vlo_start + vlo_length;
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
  if (vlo->vlo_free != vlo->vlo_start)
    VLO_SHORTEN (*vlo, 1);
  length = strlen (str) + 1;
  if (vlo->vlo_free + length > vlo->vlo_boundary)
    _VLO_expand_memory (vlo, length);
  memcpy( vlo->vlo_free, str, length );
  vlo->vlo_free = vlo->vlo_free + length;
}

/* The following function changes size of memory allocated for VLO.
   The size becames equal to about one and a half of VLO length
   accounting for length of memory which will be added after the call.
   Remember that the VLO place may be changed after the call. */

void
_VLO_expand_memory (vlo_t * vlo, size_t additional_length)
{
  size_t vlo_length;
  char *new_vlo_start;

  assert (vlo->vlo_start != NULL);
  vlo_length = VLO_LENGTH (*vlo) + additional_length;
  vlo_length += vlo_length / 2 + 1;
  new_vlo_start = (char*)yaep_realloc (vlo->vlo_alloc, vlo->vlo_start, vlo_length);
  if (new_vlo_start != vlo->vlo_start)
    {
      vlo->vlo_free += new_vlo_start - vlo->vlo_start;
      vlo->vlo_start = new_vlo_start;
    }
  vlo->vlo_boundary = vlo->vlo_start + vlo_length;
}
/*
  YAEP (Yet Another Earley Parser)

  Copyright (c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>
  Copyright (c) 2024 Fredrik Öhrström <oehrstroem@gmail.com>

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

/* ??? optimize grammar-> rules-> symbs->. */
/* This file implements parsing any CFG with minimal error recovery
   and syntax directed translation.  The algorithm is originated from
   Earley's algorithm.  The algorithm is sufficiently fast to be used
   in serious language processors. */

#include <assert.h>

#ifndef NDEBUG
#define NDEBUG 1
#endif

#ifdef YAEP_DEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

//include "allocate.h"
//include "hashtab.h"
//include "vlobject.h"
//include "objstack.h"
//include "yaep.h"



#ifndef NO_INLINE
#ifdef __GNUC__
#define MAKE_INLINE 1
#ifndef INLINE
#define INLINE
#endif
#else /* #ifdef __GNUC__ */
#if MAKE_INLINE
#ifndef INLINE
#define INLINE
#endif
#endif /* #if MAKE_INLINE */
#endif /* #ifdef __GNUC__ */
#endif /* #ifndef NO_INLINE */

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef UNDEF
#define UNDEF 1
#endif

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#ifndef YAEP_MAX_ERROR_MESSAGE_LENGTH
#define YAEP_MAX_ERROR_MESSAGE_LENGTH 200
#endif

/* Define if you don't want to use hash table: symb code -> code.  It
   is faster not to use hash tables but it requires more memory if
   symb codes are sparse.  */
#define SYMB_CODE_TRANS_VECT

/* Define this if you want to reuse already calculated Earley's sets
   and fast their reproduction.  It considerably speed up the
   parser.  */
#define USE_SET_HASH_TABLE

/* Define the following macro if you want to use absolute distances
   (in other words parsing list indexes) in Earley sets.  We don't
   recommend this as it considerably slows down the parser (there are
   much more unique triples [set, term, lookahead] in this case).  */
/* #define ABSOLUTE_DISTANCES */

/* Define the following macro if you want to use transitive
   transitions.  For some grammars it can improve the parser speed a
   bit but in most cases the speed will not be improved.

   For each core and symb, transitive transitions will contain all
   shift situations and situations added by the completer from the
   same set as the shifted original situation.  */
/* #define TRANSITIVE_TRANSITION */

/* Prime number (79087987342985798987987) mod 32 used for hash
   calculations.  */
static const unsigned jauquet_prime_mod32 = 2053222611;
/* Shift used for hash calculations.  */
static const unsigned hash_shift = 611;


/* The following is major structure which stores information about
   grammar. */
struct grammar
{
    /* The following member is TRUE if the grammar is undefined (you
       should set up the grammar by yaep_read_grammar or
       yaep_parse_grammar) or bad (error was occured in setting up the
       grammar). */
    int undefined_p;

    /* This member always contains the last occurred error code for
       given grammar. */
    int error_code;
    /* This member contains message are always contains error message
       corresponding to the last occurred error code. */
    char error_message[YAEP_MAX_ERROR_MESSAGE_LENGTH + 1];

    /* The following is grammar axiom.  There is only one rule with axiom
       in lhs. */
    struct symb *axiom;
    /* The following auxiliary symbol denotes EOF. */
    struct symb *end_marker;
    /* The following auxiliary symbol is used for describing error
       recovery. */
    struct symb *term_error;
    /* And its internal number. */
    int term_error_num;
    /* The level of usage of lookaheads:
       0    - no usage
       1    - static lookaheads
       >= 2 - dynamic lookaheads */
    int lookahead_level;
    /* The following value means how much subsequent tokens should be
       successfuly shifted to finish error recovery. */
    int recovery_token_matches;

    /* The following value is debug level:
       <0 - print translation for graphviz.
       0 - print nothing.
       1 - print statistics.
       2 - print parse tree.
       3 - print rules, parser list
       4 - print sets.
       5 - print also nonstart situations.
       6 - print additionally lookaheads. */
    int debug_level;

    /* The following value is TRUE if we need only one parse. */
    int one_parse_p;

    /* The following value is TRUE if we need parse(s) with minimal
       costs. */
    int cost_p;

    /* The following value is TRUE if we need to make error recovery. */
    int error_recovery_p;

    /* The following vocabulary used for this grammar. */
    struct symbs *symbs_ptr;
    /* The following rules used for this grammar. */
    struct rules *rules_ptr;
    /* The following terminal sets used for this grammar. */
    struct term_sets *term_sets_ptr;
    /* Allocator. */
    YaepAllocator *alloc;
};

/* The following variable value is the reference for the current
   grammar structure. */
static struct grammar *grammar;
/* The following variable values are values of the corresponding
   members for the current grammar. */
static struct symbs *symbs_ptr;
static struct term_sets *term_sets_ptr;
static struct rules *rules_ptr;

/* The following is set up the parser amnd used globally. */
static int (*read_token) (void **attr);
static void (*syntax_error) (int err_tok_num,
			     void *err_tok_attr,
			     int start_ignored_tok_num,
			     void *start_ignored_tok_attr,
			     int start_recovered_tok_num,
			     void *start_recovered_tok_attr);
static void *(*parse_alloc) (int nmemb);
static void (*parse_free) (void *mem);

/* Forward decrlarations: */
static void yaep_error (int code, const char *format, ...);

extern
void yaep_free_grammar (struct grammar *g);

/* The following is default number of tokens sucessfully matched to
   stop error recovery alternative (state). */
#define DEFAULT_RECOVERY_TOKEN_MATCHES 3

/* Expand VLO to contain N_ELS integers.  Initilize the new elements
   as zero. Return TRUE if we really made an expansion.  */
static int
expand_int_vlo (vlo_t * vlo, int n_els)
{
    int i, prev_n_els = VLO_LENGTH (*vlo) / sizeof (int);

    if (prev_n_els >= n_els)
        return FALSE;
    VLO_EXPAND (*vlo, (n_els - prev_n_els) * sizeof (int));
    for (i = prev_n_els; i < n_els; i++)
        ((int *) VLO_BEGIN (*vlo))[i] = 0;
    return TRUE;
}



/* This page is abstract data `grammar symbols'. */

/* Forward declaration. */
struct core_symb_vect;

/* The following is type of element of array representing set of
   terminals. */
typedef long int term_set_el_t;

/* The following describes symbol of grammar. */
struct symb
{
    /* The following is external representation of the symbol.  It
       should be allocated by parse_alloc because the string will be
       referred from parse tree. */
    const char *repr;
    union
    {
        struct
        {
            /* The following value is code of the terminal symbol. */
            int code;
            /* The following member is order number of the terminal. */
            int term_num;
        } term;
        struct
        {
            /* The following refers for all rules with the nonterminal
               symbol is in the left hand side of the rules. */
            struct rule *rules;
            /* The following member is order number of the nonterminal. */
            int nonterm_num;
            /* The following value is nonzero if nonterminal may derivate
               itself.  In other words there is a grammar loop for this
               nonterminal. */
            int loop_p;
            /* The following members are FIRST and FOLLOW sets of the
               nonterminal. */
            term_set_el_t *first, *follow;
        } nonterm;
    } u;
    /* The following member is TRUE if it is nonterminal. */
    char term_p;
    /* The following member value (if defined) is TRUE if the symbol is
       accessible (derivated) from the axiom. */
    char access_p;
    /* The following member is TRUE if it is a termainal or it is a
       nonterminal which derivates a terminal string. */
    char derivation_p;
    /* The following is TRUE if it is nonterminal which may derivate
       empty string. */
    char empty_p;
    /* The following member is order number of symbol. */
    int num;
#ifdef USE_CORE_SYMB_HASH_TABLE
    /* The following is used as cache for subsequent search for
       core_symb_vect with given symb. */
    struct core_symb_vect *cached_core_symb_vect;
#endif
};

/* The following structure contians all information about grammar
   vocabulary. */
struct symbs
{
    /* The following is number of all symbols and terminals.  The
       variables can be read externally. */
    int n_terms, n_nonterms;

    /* All symbols are placed in the following object. */
    os_t symbs_os;

    /* All references to the symbols, terminals, nonterminals are stored
       in the following vlos.  The indexes in the arrays are the same as
       corresponding symbol, terminal, and nonterminal numbers. */
    vlo_t symbs_vlo;
    vlo_t terms_vlo;
    vlo_t nonterms_vlo;

    /* The following are tables to find terminal by its code and symbol by
       its representation. */
    hash_table_t repr_to_symb_tab;	/* key is `repr' */
    hash_table_t code_to_symb_tab;	/* key is `code' */
#ifdef SYMB_CODE_TRANS_VECT
    /* If terminal symbol codes are not spared (in this case the member
       value is not NULL, we use translation vector instead of hash
       table.  */
    struct symb **symb_code_trans_vect;
    int symb_code_trans_vect_start;
    int symb_code_trans_vect_end;
#endif
};

/* Hash of symbol representation. */
static unsigned
symb_repr_hash (hash_table_entry_t s)
{
    unsigned result = jauquet_prime_mod32;
    const char *str = ((struct symb *) s)->repr;
    int i;

    for (i = 0; str[i] != '\0'; i++)
        result = result * hash_shift + (unsigned) str[i];
    return result;
}

/* Equality of symbol representations. */
static int
symb_repr_eq (hash_table_entry_t s1, hash_table_entry_t s2)
{
    return strcmp (((struct symb *) s1)->repr, ((struct symb *) s2)->repr) == 0;
}

/* Hash of terminal code. */
static unsigned
symb_code_hash (hash_table_entry_t s)
{
    struct symb *symb = ((struct symb *) s);

    assert (symb->term_p);
    return symb->u.term.code;
}

/* Equality of terminal codes. */
static int
symb_code_eq (hash_table_entry_t s1, hash_table_entry_t s2)
{
    struct symb *symb1 = ((struct symb *) s1);
    struct symb *symb2 = ((struct symb *) s2);

    assert (symb1->term_p && symb2->term_p);
    return symb1->u.term.code == symb2->u.term.code;
}

/* Initialize work with symbols and returns storage for the
   symbols. */
static struct symbs *
symb_init (void)
{
    void *mem;
    struct symbs *result;

    mem = yaep_malloc (grammar->alloc, sizeof (struct symbs));
    result = (struct symbs *) mem;
    OS_CREATE (result->symbs_os, grammar->alloc, 0);
    VLO_CREATE (result->symbs_vlo, grammar->alloc, 1024);
    VLO_CREATE (result->terms_vlo, grammar->alloc, 512);
    VLO_CREATE (result->nonterms_vlo, grammar->alloc, 512);
    result->repr_to_symb_tab =
        create_hash_table (grammar->alloc, 300, symb_repr_hash, symb_repr_eq);
    result->code_to_symb_tab =
        create_hash_table (grammar->alloc, 200, symb_code_hash, symb_code_eq);
#ifdef SYMB_CODE_TRANS_VECT
    result->symb_code_trans_vect = NULL;
#endif
    result->n_nonterms = result->n_terms = 0;
    return result;
}

/* Return symbol (or NULL if it does not exist) whose representation
   is REPR. */
static struct symb *
symb_find_by_repr (const char *repr)
{
    struct symb symb;

    symb.repr = repr;
    return (struct symb *) *find_hash_table_entry (symbs_ptr->repr_to_symb_tab,
                                                   &symb, FALSE);
}

/* Return symbol (or NULL if it does not exist) which is terminal with
   CODE. */
#if MAKE_INLINE
INLINE
#endif
static struct symb *
symb_find_by_code (int code)
{
    struct symb symb;

#ifdef SYMB_CODE_TRANS_VECT
    if (symbs_ptr->symb_code_trans_vect != NULL)
    {
        if ((code < symbs_ptr->symb_code_trans_vect_start)
            || (code >= symbs_ptr->symb_code_trans_vect_end))
        {
            return NULL;
        }
        else
        {
            return symbs_ptr->symb_code_trans_vect
                [code - symbs_ptr->symb_code_trans_vect_start];
        }
    }
#endif
    symb.term_p = TRUE;
    symb.u.term.code = code;
    return (struct symb *) *find_hash_table_entry (symbs_ptr->code_to_symb_tab,
                                                   &symb, FALSE);
}

/* The function creates new terminal symbol and returns reference for
   it.  The symbol should be not in the tables.  The function should
   create own copy of name for the new symbol. */
static struct symb *
symb_add_term (const char *name, int code)
{
    struct symb symb, *result;
    hash_table_entry_t *repr_entry, *code_entry;

    symb.repr = name;
    symb.term_p = TRUE;
    symb.num = symbs_ptr->n_nonterms + symbs_ptr->n_terms;
    symb.u.term.code = code;
    symb.u.term.term_num = symbs_ptr->n_terms++;
    symb.empty_p = FALSE;
    repr_entry =
        find_hash_table_entry (symbs_ptr->repr_to_symb_tab, &symb, TRUE);
    assert (*repr_entry == NULL);
    code_entry =
        find_hash_table_entry (symbs_ptr->code_to_symb_tab, &symb, TRUE);
    assert (*code_entry == NULL);
    OS_TOP_ADD_STRING (symbs_ptr->symbs_os, name);
    symb.repr = (char *) OS_TOP_BEGIN (symbs_ptr->symbs_os);
    OS_TOP_FINISH (symbs_ptr->symbs_os);
    OS_TOP_ADD_MEMORY (symbs_ptr->symbs_os, &symb, sizeof (struct symb));
    result = (struct symb *) OS_TOP_BEGIN (symbs_ptr->symbs_os);
    OS_TOP_FINISH (symbs_ptr->symbs_os);
    *repr_entry = (hash_table_entry_t) result;
    *code_entry = (hash_table_entry_t) result;
    VLO_ADD_MEMORY (symbs_ptr->symbs_vlo, &result, sizeof (struct symb *));
    VLO_ADD_MEMORY (symbs_ptr->terms_vlo, &result, sizeof (struct symb *));
    return result;
}

/* The function creates new nonterminal symbol and returns reference
   for it.  The symbol should be not in the table.  The function
   should create own copy of name for the new symbol. */
static struct symb *
symb_add_nonterm (const char *name)
{
    struct symb symb, *result;
    hash_table_entry_t *entry;

    symb.repr = name;
    symb.term_p = FALSE;
    symb.num = symbs_ptr->n_nonterms + symbs_ptr->n_terms;
    symb.u.nonterm.rules = NULL;
    symb.u.nonterm.loop_p = 0;
    symb.u.nonterm.nonterm_num = symbs_ptr->n_nonterms++;
    entry = find_hash_table_entry (symbs_ptr->repr_to_symb_tab, &symb, TRUE);
    assert (*entry == NULL);
    OS_TOP_ADD_STRING (symbs_ptr->symbs_os, name);
    symb.repr = (char *) OS_TOP_BEGIN (symbs_ptr->symbs_os);
    OS_TOP_FINISH (symbs_ptr->symbs_os);
    OS_TOP_ADD_MEMORY (symbs_ptr->symbs_os, &symb, sizeof (struct symb));
    result = (struct symb *) OS_TOP_BEGIN (symbs_ptr->symbs_os);
    OS_TOP_FINISH (symbs_ptr->symbs_os);
    *entry = (hash_table_entry_t) result;
    VLO_ADD_MEMORY (symbs_ptr->symbs_vlo, &result, sizeof (struct symb *));
    VLO_ADD_MEMORY (symbs_ptr->nonterms_vlo, &result, sizeof (struct symb *));
    return result;
}

/* The following function return N-th symbol (if any) or NULL
   otherwise. */
static struct symb *
symb_get (int n)
{
    struct symb *symb;

    if (n < 0 || (VLO_LENGTH (symbs_ptr->symbs_vlo) / sizeof (struct symb *)
                  <= (size_t) n))
        return NULL;
    symb = ((struct symb **) VLO_BEGIN (symbs_ptr->symbs_vlo))[n];
    assert (symb->num == n);
    return symb;
}

/* The following function return N-th symbol (if any) or NULL
   otherwise. */
static struct symb *
term_get (int n)
{
    struct symb *symb;

    if (n < 0 || (VLO_LENGTH (symbs_ptr->terms_vlo)
                  / sizeof (struct symb *) <= (size_t) n))
        return NULL;
    symb = ((struct symb **) VLO_BEGIN (symbs_ptr->terms_vlo))[n];
    assert (symb->term_p && symb->u.term.term_num == n);
    return symb;
}

/* The following function return N-th symbol (if any) or NULL
   otherwise. */
static struct symb *
nonterm_get (int n)
{
    struct symb *symb;

    if (n < 0 || (VLO_LENGTH (symbs_ptr->nonterms_vlo) / sizeof (struct symb *)
                  <= (size_t) n))
        return NULL;
    symb = ((struct symb **) VLO_BEGIN (symbs_ptr->nonterms_vlo))[n];
    assert (!symb->term_p && symb->u.nonterm.nonterm_num == n);
    return symb;
}

#ifndef NO_YAEP_DEBUG_PRINT

/* The following function prints symbol SYMB to file F.  Terminal is
   printed with its code if CODE_P. */
static void
symb_print (FILE * f, struct symb *symb, int code_p)
{
    fprintf (f, "%s", symb->repr);
    if (code_p && symb->term_p)
        fprintf (f, "(%d)", symb->u.term.code);
}

#endif /* #ifndef NO_YAEP_DEBUG_PRINT */

#ifdef SYMB_CODE_TRANS_VECT

#define SYMB_CODE_TRANS_VECT_SIZE 10000

static void
symb_finish_adding_terms (void)
{
    int i, max_code, min_code;
    struct symb *symb;
    void *mem;

    for (min_code = max_code = i = 0; (symb = term_get (i)) != NULL; i++)
    {
        if (i == 0 || min_code > symb->u.term.code)
            min_code = symb->u.term.code;
        if (i == 0 || max_code < symb->u.term.code)
            max_code = symb->u.term.code;
    }
    assert (i != 0);
    if (max_code - min_code < SYMB_CODE_TRANS_VECT_SIZE)
    {
        symbs_ptr->symb_code_trans_vect_start = min_code;
        symbs_ptr->symb_code_trans_vect_end = max_code + 1;
        mem = yaep_malloc (grammar->alloc,
                           sizeof (struct symb*) * (max_code - min_code + 1));
        symbs_ptr->symb_code_trans_vect = (struct symb **) mem;
        for (i = 0; (symb = term_get (i)) != NULL; i++)
            symbs_ptr->symb_code_trans_vect[symb->u.term.code - min_code] = symb;
    }
}
#endif

/* Free memory for symbols. */
static void
symb_empty (struct symbs *symbs)
{
    if (symbs == NULL)
        return;
#ifdef SYMB_CODE_TRANS_VECT
    if (symbs_ptr->symb_code_trans_vect != NULL)
    {
        yaep_free (grammar->alloc, symbs_ptr->symb_code_trans_vect);
        symbs_ptr->symb_code_trans_vect = NULL;
    }
#endif
    empty_hash_table (symbs->repr_to_symb_tab);
    empty_hash_table (symbs->code_to_symb_tab);
    VLO_NULLIFY (symbs->nonterms_vlo);
    VLO_NULLIFY (symbs->terms_vlo);
    VLO_NULLIFY (symbs->symbs_vlo);
    OS_EMPTY (symbs->symbs_os);
    symbs->n_nonterms = symbs->n_terms = 0;
}

/* Finalize work with symbols. */
static void
symb_fin (struct symbs *symbs)
{
    if (symbs == NULL)
        return;
#ifdef SYMB_CODE_TRANS_VECT
    if (symbs_ptr->symb_code_trans_vect != NULL)
        yaep_free (grammar->alloc, symbs_ptr->symb_code_trans_vect);
#endif
    delete_hash_table (symbs_ptr->repr_to_symb_tab);
    delete_hash_table (symbs_ptr->code_to_symb_tab);
    VLO_DELETE (symbs_ptr->nonterms_vlo);
    VLO_DELETE (symbs_ptr->terms_vlo);
    VLO_DELETE (symbs_ptr->symbs_vlo);
    OS_DELETE (symbs_ptr->symbs_os);
    yaep_free (grammar->alloc, symbs);
    symbs = NULL;
}




/* This page contains abstract data set of terminals. */

/* The following is element of term set hash table. */
struct tab_term_set
{
    /* Number of set in the table. */
    int num;
    /* The terminal set itself. */
    term_set_el_t *set;
};

/* The following container for the abstract data. */
struct term_sets
{
    /* All terminal sets are stored in the following os. */
    os_t term_set_os;

    /* The following variables can be read externally.  Their values are
       number of terminal sets and their overall size. */
    int n_term_sets, n_term_sets_size;

    /* The following is hash table of terminal sets (key is member
       `set'). */
    hash_table_t term_set_tab;

    /* References to all structure tab_term_set are stored in the
       following vlo. */
    vlo_t tab_term_set_vlo;
};

/* Hash of table terminal set. */
static unsigned
term_set_hash (hash_table_entry_t s)
{
    term_set_el_t *set = ((struct tab_term_set *) s)->set;
    term_set_el_t *bound;
    int size;
    unsigned result = jauquet_prime_mod32;

    size = ((symbs_ptr->n_terms + CHAR_BIT * sizeof (term_set_el_t) - 1)
            / (CHAR_BIT * sizeof (term_set_el_t)));
    bound = set + size;
    while (set < bound)
        result = result * hash_shift + *set++;
    return result;
}

/* Equality of terminal sets. */
static int
term_set_eq (hash_table_entry_t s1, hash_table_entry_t s2)
{
    term_set_el_t *set1 = ((struct tab_term_set *) s1)->set;
    term_set_el_t *set2 = ((struct tab_term_set *) s2)->set;
    term_set_el_t *bound;
    int size;

    size = ((symbs_ptr->n_terms + CHAR_BIT * sizeof (term_set_el_t) - 1)
            / (CHAR_BIT * sizeof (term_set_el_t)));
    bound = set1 + size;
    while (set1 < bound)
        if (*set1++ != *set2++)
            return FALSE;
    return TRUE;
}

/* Initialize work with terminal sets and returns storage for terminal
   sets. */
static struct term_sets *
term_set_init (void)
{
    void *mem;
    struct term_sets *result;

    mem = yaep_malloc (grammar->alloc, sizeof (struct term_sets));
    result = (struct term_sets *) mem;
    OS_CREATE (result->term_set_os, grammar->alloc, 0);
    result->term_set_tab =
        create_hash_table (grammar->alloc, 1000, term_set_hash, term_set_eq);
    VLO_CREATE (result->tab_term_set_vlo, grammar->alloc, 4096);
    result->n_term_sets = result->n_term_sets_size = 0;
    return result;
}

/* Return new terminal SET.  Its value is undefined. */
static term_set_el_t *
term_set_create (void)
{
    int size;
    term_set_el_t *result;

    assert (sizeof (term_set_el_t) <= 8);
    size = 8;
    /* Make it 64 bit multiple to have the same statistics for 64 bit
       machines. */
    size = ((symbs_ptr->n_terms + CHAR_BIT * 8 - 1) / (CHAR_BIT * 8)) * 8;
    OS_TOP_EXPAND (term_sets_ptr->term_set_os, size);
    result = (term_set_el_t *) OS_TOP_BEGIN (term_sets_ptr->term_set_os);
    OS_TOP_FINISH (term_sets_ptr->term_set_os);
    term_sets_ptr->n_term_sets++;
    term_sets_ptr->n_term_sets_size += size;
    return result;
}

/* Make terminal SET empty. */
#if MAKE_INLINE
INLINE
#endif
static void
term_set_clear (term_set_el_t * set)
{
    term_set_el_t *bound;
    int size;

    size = ((symbs_ptr->n_terms + CHAR_BIT * sizeof (term_set_el_t) - 1)
            / (CHAR_BIT * sizeof (term_set_el_t)));
    bound = set + size;
    while (set < bound)
        *set++ = 0;
}

/* Copy SRC into DEST. */
#if MAKE_INLINE
INLINE
#endif
static void
term_set_copy (term_set_el_t * dest, term_set_el_t * src)
{
    term_set_el_t *bound;
    int size;

    size = ((symbs_ptr->n_terms + CHAR_BIT * sizeof (term_set_el_t) - 1)
            / (CHAR_BIT * sizeof (term_set_el_t)));
    bound = dest + size;
    while (dest < bound)
        *dest++ = *src++;
}

/* Add all terminals from set OP with to SET.  Return TRUE if SET has
   been changed. */
#if MAKE_INLINE
INLINE
#endif
static int
term_set_or (term_set_el_t * set, term_set_el_t * op)
{
    term_set_el_t *bound;
    int size, changed_p;

    size = ((symbs_ptr->n_terms + CHAR_BIT * sizeof (term_set_el_t) - 1)
            / (CHAR_BIT * sizeof (term_set_el_t)));
    bound = set + size;
    changed_p = 0;
    while (set < bound)
    {
        if ((*set | *op) != *set)
            changed_p = 1;
        *set++ |= *op++;
    }
    return changed_p;
}

/* Add terminal with number NUM to SET.  Return TRUE if SET has been
   changed. */
#if MAKE_INLINE
INLINE
#endif
static int
term_set_up (term_set_el_t * set, int num)
{
    int ind, changed_p;
    term_set_el_t bit;

    assert (num < symbs_ptr->n_terms);
    ind = num / (CHAR_BIT * sizeof (term_set_el_t));
    bit = ((term_set_el_t) 1) << (num % (CHAR_BIT * sizeof (term_set_el_t)));
    changed_p = (set[ind] & bit ? 0 : 1);
    set[ind] |= bit;
    return changed_p;
}

/* Return TRUE if terminal with number NUM is in SET. */
#if MAKE_INLINE
INLINE
#endif
static int
term_set_test (term_set_el_t * set, int num)
{
    int ind;
    term_set_el_t bit;

    assert (num >= 0 && num < symbs_ptr->n_terms);
    ind = num / (CHAR_BIT * sizeof (term_set_el_t));
    bit = ((term_set_el_t) 1) << (num % (CHAR_BIT * sizeof (term_set_el_t)));
    return (set[ind] & bit) != 0;
}

/* The following function inserts terminal SET into the table and
   returns its number.  If the set is already in table it returns -its
   number - 1 (which is always negative).  Don't set after
   insertion!!! */
static int
term_set_insert (term_set_el_t * set)
{
    hash_table_entry_t *entry;
    struct tab_term_set tab_term_set, *tab_term_set_ptr;

    tab_term_set.set = set;
    entry =
        find_hash_table_entry (term_sets_ptr->term_set_tab, &tab_term_set, TRUE);
    if (*entry != NULL)
        return -((struct tab_term_set *) *entry)->num - 1;
    else
    {
        OS_TOP_EXPAND (term_sets_ptr->term_set_os,
                       sizeof (struct tab_term_set));
        tab_term_set_ptr =
            (struct tab_term_set *) OS_TOP_BEGIN (term_sets_ptr->term_set_os);
        OS_TOP_FINISH (term_sets_ptr->term_set_os);
        *entry = (hash_table_entry_t) tab_term_set_ptr;
        tab_term_set_ptr->set = set;
        tab_term_set_ptr->num = (VLO_LENGTH (term_sets_ptr->tab_term_set_vlo)
                                 / sizeof (struct tab_term_set *));
        VLO_ADD_MEMORY (term_sets_ptr->tab_term_set_vlo, &tab_term_set_ptr,
                        sizeof (struct tab_term_set *));
        return ((struct tab_term_set *) *entry)->num;
    }
}

/* The following function returns set which is in the table with
   number NUM. */
#if MAKE_INLINE
INLINE
#endif
static term_set_el_t *
term_set_from_table (int num)
{
    assert (num >= 0);
    assert ((long unsigned int)num < VLO_LENGTH (term_sets_ptr->tab_term_set_vlo)
            / sizeof (struct tab_term_set *));
    return ((struct tab_term_set **)
            VLO_BEGIN (term_sets_ptr->tab_term_set_vlo))[num]->set;
}

/* Print terminal SET into file F. */
static void
term_set_print (FILE * f, term_set_el_t * set)
{
    int i;

    for (i = 0; i < symbs_ptr->n_terms; i++)
        if (term_set_test (set, i))
        {
            fprintf (f, " ");
            symb_print (f, term_get (i), FALSE);
        }
}

/* Free memory for terminal sets. */
static void
term_set_empty (struct term_sets *term_sets)
{
    if (term_sets == NULL)
        return;
    VLO_NULLIFY (term_sets->tab_term_set_vlo);
    empty_hash_table (term_sets->term_set_tab);
    OS_EMPTY (term_sets->term_set_os);
    term_sets->n_term_sets = term_sets->n_term_sets_size = 0;
}

/* Finalize work with terminal sets. */
static void
term_set_fin (struct term_sets *term_sets)
{
    if (term_sets == NULL)
        return;
    VLO_DELETE (term_sets->tab_term_set_vlo);
    delete_hash_table (term_sets->term_set_tab);
    OS_DELETE (term_sets->term_set_os);
    yaep_free (grammar->alloc, term_sets);
    term_sets = NULL;
}



/* This page is abstract data `grammar rules'. */

/* The following describes rule of grammar. */
struct rule
{
    /* The following is order number of rule. */
    int num;
    /* The following is length of rhs. */
    int rhs_len;
    /* The following is the next grammar rule. */
    struct rule *next;
    /* The following is the next grammar rule with the same nonterminal
       in lhs of the rule. */
    struct rule *lhs_next;
    /* The following is nonterminal in the left hand side of the
       rule. */
    struct symb *lhs;
    /* The ixml default mark of the rule */
    char mark;
    /* The following is symbols in the right hand side of the rule. */
    struct symb **rhs;
    /* The ixml marks for all the terms in the right hand side of the rule. */
    char *marks;
    /* The following three members define rule translation. */
    const char *anode;		/* abstract node name if any. */
    int anode_cost;		/* the cost of the abstract node if any, otherwise 0. */
    int trans_len;		/* number of symbol translations in the rule translation. */
    /* The following array elements correspond to element of rhs with
       the same index.  The element value is order number of the
       corresponding symbol translation in the rule translation.  If the
       symbol translation is rejected, the corresponding element value is
       negative. */
    int *order;
    /* The following member value is equal to size of all previous rule
       lengths + number of the previous rules.  Imagine that all left
       hand symbol and right hand size symbols of the rules are stored
       in array.  Then the following member is index of the rule lhs in
       the array. */
    int rule_start_offset;
    /* The following is the same string as anode but memory allocated in
       parse_alloc. */
    char *caller_anode;
};

/* The following container for the abstract data. */
struct rules
{
    /* The following is number of all rules and their summary rhs
       length.  The variables can be read externally. */
    int n_rules, n_rhs_lens;
    /* The following is the first rule. */
    struct rule *first_rule;
    /* The following is rule being formed.  It can be read
       externally. */
    struct rule *curr_rule;
    /* All rules are placed in the following object. */
    os_t rules_os;
};

/* Initialize work with rules and returns pointer to rules storage. */
static struct rules *
rule_init (void)
{
    void *mem;
    struct rules *result;

    mem = yaep_malloc (grammar->alloc, sizeof (struct rules));
    result = (struct rules *) mem;
    OS_CREATE (result->rules_os, grammar->alloc, 0);
    result->first_rule = result->curr_rule = NULL;
    result->n_rules = result->n_rhs_lens = 0;
    return result;
}

/* Create new rule with LHS empty rhs. */
static struct rule *
rule_new_start (struct symb *lhs, const char *anode, int anode_cost)
{
    struct rule *rule;
    struct symb *empty;

    assert (!lhs->term_p);
    OS_TOP_EXPAND (rules_ptr->rules_os, sizeof (struct rule));
    rule = (struct rule *) OS_TOP_BEGIN (rules_ptr->rules_os);
    OS_TOP_FINISH (rules_ptr->rules_os);
    rule->lhs = lhs;
    if (anode == NULL)
    {
        rule->anode = NULL;
        rule->anode_cost = 0;
    }
    else
    {
        OS_TOP_ADD_STRING (rules_ptr->rules_os, anode);
        rule->anode = (char *) OS_TOP_BEGIN (rules_ptr->rules_os);
        OS_TOP_FINISH (rules_ptr->rules_os);
        rule->anode_cost = anode_cost;
    }
    rule->trans_len = 0;
    rule->marks = NULL;
    rule->order = NULL;
    rule->next = NULL;
    if (rules_ptr->curr_rule != NULL)
        rules_ptr->curr_rule->next = rule;
    rule->lhs_next = lhs->u.nonterm.rules;
    lhs->u.nonterm.rules = rule;
    rule->rhs_len = 0;
    empty = NULL;
    OS_TOP_ADD_MEMORY (rules_ptr->rules_os, &empty, sizeof (struct symb *));
    rule->rhs = (struct symb **) OS_TOP_BEGIN (rules_ptr->rules_os);
    rules_ptr->curr_rule = rule;
    if (rules_ptr->first_rule == NULL)
        rules_ptr->first_rule = rule;
    rule->rule_start_offset = rules_ptr->n_rhs_lens + rules_ptr->n_rules;
    rule->num = rules_ptr->n_rules++;
    return rule;
}

/* Add SYMB at the end of current rule rhs. */
static void
rule_new_symb_add (struct symb *symb)
{
    struct symb *empty;

    empty = NULL;
    OS_TOP_ADD_MEMORY (rules_ptr->rules_os, &empty, sizeof (struct symb *));
    rules_ptr->curr_rule->rhs
        = (struct symb **) OS_TOP_BEGIN (rules_ptr->rules_os);
    rules_ptr->curr_rule->rhs[rules_ptr->curr_rule->rhs_len] = symb;
    rules_ptr->curr_rule->rhs_len++;
    rules_ptr->n_rhs_lens++;
}

/* The function should be called at end of forming each rule.  It
   creates and initializes situation cache. */
static void
rule_new_stop (void)
{
    int i;

    OS_TOP_FINISH (rules_ptr->rules_os);
    OS_TOP_EXPAND (rules_ptr->rules_os,
                   rules_ptr->curr_rule->rhs_len * sizeof (int));
    rules_ptr->curr_rule->marks = (char*)calloc(rules_ptr->curr_rule->rhs_len, sizeof(1)); // IXML
    rules_ptr->curr_rule->order = (int *) OS_TOP_BEGIN (rules_ptr->rules_os);
    OS_TOP_FINISH (rules_ptr->rules_os);
    for (i = 0; i < rules_ptr->curr_rule->rhs_len; i++)
        rules_ptr->curr_rule->order[i] = -1;
}

#ifndef NO_YAEP_DEBUG_PRINT

/* The following function prints RULE with its translation (if
   TRANS_P) to file F. */
static void
rule_print (FILE * f, struct rule *rule, int trans_p)
{
    int i, j;

    assert(rule->mark >= 0 && rule->mark < 128);
    fprintf (f, "%c", rule->mark?rule->mark:' ');
    symb_print (f, rule->lhs, FALSE);
    fprintf (f, " :");
    for (i = 0; i < rule->rhs_len; i++)
    {
        assert(rule->marks[i] >= 0 && rule->marks[i] < 128);
        fprintf (f, " %c", rule->marks[i]?rule->marks[i]:' ');
        symb_print (f, rule->rhs[i], FALSE);
    }
    if (trans_p)
    {
        fprintf (f, " ---- ");
        if (rule->anode != NULL)
            fprintf (f, "%s (", rule->anode);
        for (i = 0; i < rule->trans_len; i++)
	{
            for (j = 0; j < rule->rhs_len; j++)
                if (rule->order[j] == i)
                {
                    fprintf (f, " %d:", j);
                    symb_print (f, rule->rhs[j], FALSE);
                    break;
                }
            if (j >= rule->rhs_len)
                fprintf (f, " nil");
	}
        if (rule->anode != NULL)
            fprintf (f, " )");
    }
    fprintf (f, "\n");
}

/* The following function prints RULE to file F with dot in position
   POS. */
static void
rule_dot_print (FILE * f, struct rule *rule, int pos)
{
    int i;

    assert (pos >= 0 && pos <= rule->rhs_len);
    symb_print (f, rule->lhs, FALSE);
    fprintf (f, " :");
    for (i = 0; i < rule->rhs_len; i++)
    {
        fprintf (f, i == pos ? " ." : " ");
        symb_print (f, rule->rhs[i], FALSE);
    }
    if (rule->rhs_len == pos)
        fprintf (f, ".");
}

#endif /* #ifndef NO_YAEP_DEBUG_PRINT */

/* The following function frees memory for rules. */
static void
rule_empty (struct rules *rules)
{
    if (rules == NULL)
        return;
    OS_EMPTY (rules->rules_os);
    rules->first_rule = rules->curr_rule = NULL;
    rules->n_rules = rules->n_rhs_lens = 0;
}

/* Finalize work with rules. */
static void
rule_fin (struct rules *rules)
{
    if (rules == NULL)
        return;
    OS_DELETE (rules->rules_os);
    yaep_free (grammar->alloc, rules);
    rules = NULL;
}



/* This page is abstract data `input tokens'. */

/* The initial length of array (in tokens) in which input tokens are
   placed. */
#ifndef YAEP_INIT_TOKENS_NUMBER
#define YAEP_INIT_TOKENS_NUMBER 10000
#endif

/* The following describes input token. */
struct tok
{
    /* The following is symb correseponding to the token. */
    struct symb *symb;
    /* The following is an attribute of the token. */
    void *attr;
};

/* The following two variables contains all input tokens and their
   number.  The variables can be read externally. */
static struct tok *toks;
static int toks_len;
static int tok_curr;

/* The following array contains all input tokens. */
static vlo_t toks_vlo;

/* Initialize work with tokens. */
static void
tok_init (void)
{
    VLO_CREATE (toks_vlo, grammar->alloc,
                YAEP_INIT_TOKENS_NUMBER * sizeof (struct tok));
    toks_len = 0;
}

/* Add input token with CODE and attribute at the end of input tokens
   array. */
static void
tok_add (int code, void *attr)
{
    struct tok tok;

    tok.attr = attr;
    tok.symb = symb_find_by_code (code);
    if (tok.symb == NULL)
    {
        yaep_error (YAEP_INVALID_TOKEN_CODE, "syntax error at offset %d '%c'", toks_len, code);
    }
    VLO_ADD_MEMORY (toks_vlo, &tok, sizeof (struct tok));
    toks = (struct tok *) VLO_BEGIN (toks_vlo);
    toks_len++;
}

/* Finalize work with tokens. */
static void
tok_fin (void)
{
    VLO_DELETE (toks_vlo);
}



/* This page is abstract data `situations'. */

/* The following describes situation without distance of its original
   set.  To save memory we extract such structure because there are
   many duplicated structures. */
struct sit
{
    /* The following is the situation rule. */
    struct rule *rule;
    /* The following is position of dot in rhs of the situation rule. */
    short pos;
    /* The following member is TRUE if the tail can derive empty
       string. */
    char empty_tail_p;
    /* unique situation number.  */
    int sit_number;
    /* The following is number of situation context which is number of
       the corresponding terminal set in the table.  It is really used
       only for dynamic lookahead. */
    int context;
#ifdef TRANSITIVE_TRANSITION
    /* The following member is used for using transitive transition
       vectors to exclude multiple situation processing.  */
    int sit_check;
#endif
    /* The following member is the situation lookahead it is equal to
       FIRST (the situation tail || FOLLOW (lhs)) for static lookaheads
       and FIRST (the situation tail || context) for dynamic ones. */
    term_set_el_t *lookahead;
};

/* The following contains current number of unique situations.  It can
   be read externally. */
static int n_all_sits;

/* The following two dimensional array (the first dimension is context
   number, the second one is situation number) contains references to
   all possible situations. */
static struct sit ***sit_table;

/* The following vlo is indexed by situation context number and gives
   array which is indexed by situation number
   (sit->rule->rule_start_offset + sit->pos). */
static vlo_t sit_table_vlo;

/* All situations are placed in the following object. */
static os_t sits_os;

/* Initialize work with situations. */
static void
sit_init (void)
{
    n_all_sits = 0;
    OS_CREATE (sits_os, grammar->alloc, 0);
    VLO_CREATE (sit_table_vlo, grammar->alloc, 4096);
    sit_table = (struct sit ***) VLO_BEGIN (sit_table_vlo);
}

/* The following function sets up lookahead of situation SIT.  The
   function returns TRUE if the situation tail may derive empty
   string. */
static int
sit_set_lookahead (struct sit *sit)
{
    struct symb *symb, **symb_ptr;

    if (grammar->lookahead_level == 0)
        sit->lookahead = NULL;
    else
    {
        sit->lookahead = term_set_create ();
        term_set_clear (sit->lookahead);
    }
    symb_ptr = &sit->rule->rhs[sit->pos];
    while ((symb = *symb_ptr) != NULL)
    {
        if (grammar->lookahead_level != 0)
	{
            if (symb->term_p)
                term_set_up (sit->lookahead, symb->u.term.term_num);
            else
                term_set_or (sit->lookahead, symb->u.nonterm.first);
	}
        if (!symb->empty_p)
            break;
        symb_ptr++;
    }
    if (symb == NULL)
    {
        if (grammar->lookahead_level == 1)
            term_set_or (sit->lookahead, sit->rule->lhs->u.nonterm.follow);
        else if (grammar->lookahead_level != 0)
            term_set_or (sit->lookahead, term_set_from_table (sit->context));
        return TRUE;
    }
    return FALSE;
}

/* The following function returns situations with given
   characteristics.  Remember that situations are stored in one
   exemplar. */
#if MAKE_INLINE
INLINE
#endif
static struct sit *
sit_create (struct rule *rule, int pos, int context)
{
    struct sit *sit;
    struct sit ***context_sit_table_ptr;

    assert (context >= 0);
    context_sit_table_ptr = sit_table + context;
    if ((char *) context_sit_table_ptr >= (char *) VLO_BOUND (sit_table_vlo))
    {
        struct sit ***bound, ***ptr;
        int i, diff;

        assert ((grammar->lookahead_level <= 1 && context == 0)
                || (grammar->lookahead_level > 1 && context >= 0));
        diff
            = (char *) context_sit_table_ptr - (char *) VLO_BOUND (sit_table_vlo);
        diff += sizeof (struct sit **);
        if (grammar->lookahead_level > 1 && diff == sizeof (struct sit **))
            diff *= 10;
        VLO_EXPAND (sit_table_vlo, diff);
        sit_table = (struct sit ***) VLO_BEGIN (sit_table_vlo);
        bound = (struct sit ***) VLO_BOUND (sit_table_vlo);
        context_sit_table_ptr = sit_table + context;
        ptr = bound - diff / sizeof (struct sit **);
        while (ptr < bound)
	{
            OS_TOP_EXPAND (sits_os, (rules_ptr->n_rhs_lens + rules_ptr->n_rules)
                           * sizeof (struct sit *));
            *ptr = (struct sit **) OS_TOP_BEGIN (sits_os);
            OS_TOP_FINISH (sits_os);
            for (i = 0; i < rules_ptr->n_rhs_lens + rules_ptr->n_rules; i++)
                (*ptr)[i] = NULL;
            ptr++;
	}
    }
    if ((sit = (*context_sit_table_ptr)[rule->rule_start_offset + pos]) != NULL)
        return sit;
    OS_TOP_EXPAND (sits_os, sizeof (struct sit));
    sit = (struct sit *) OS_TOP_BEGIN (sits_os);
    OS_TOP_FINISH (sits_os);
    n_all_sits++;
    sit->rule = rule;
    sit->pos = pos;
    sit->sit_number = n_all_sits;
    sit->context = context;
    sit->empty_tail_p = sit_set_lookahead (sit);
#ifdef TRANSITIVE_TRANSITION
    sit->sit_check = 0;
#endif
    (*context_sit_table_ptr)[rule->rule_start_offset + pos] = sit;
    return sit;
}

#ifndef NO_YAEP_DEBUG_PRINT

/* The following function prints situation SIT to file F.  The
   situation is printed with the lookahead set if LOOKAHEAD_P. */
static void
sit_print (FILE * f, struct sit *sit, int lookahead_p)
{
    fprintf (f, "%3d ", sit->sit_number);
    rule_dot_print (f, sit->rule, sit->pos);
    if (grammar->lookahead_level != 0 && lookahead_p)
    {
        fprintf (f, ",");
        term_set_print (f, sit->lookahead);
    }
}

#endif /* #ifndef NO_YAEP_DEBUG_PRINT */

/* Return hash of sequence of N_SITS situations in array SITS.  */
static unsigned
sits_hash (int n_sits, struct sit **sits)
{
    int n, i;
    unsigned result;

    result = jauquet_prime_mod32;
    for (i = 0; i < n_sits; i++)
    {
        n = sits[i]->sit_number;
        result = result * hash_shift + n;
    }
    return result;
}

/* Finalize work with situations. */
static void
sit_fin (void)
{
    VLO_DELETE (sit_table_vlo);
    OS_DELETE (sits_os);
}



/* This page is abstract data `sets'. */

/* The following is set in Earley's algorithm without distance
   information.  Because there are many duplications of such
   structures we extract the set cores and store them in one
   exemplar. */
struct set_core
{
    /* The following is unique number of the set core. It is defined
       only after forming all set. */
    int num;
    /* The set core hash.  We save it as it is used several times.  */
    unsigned int hash;
    /* The following is term shifting which resulted into this core.  It
       is defined only after forming all set. */
    struct symb *term;
    /* The following are numbers of all situations and start situations
       in the following array. */
    int n_sits;
    int n_start_sits;
    /* Array of situations.  Start situations are always placed the
       first in the order of their creation (with subsequent duplicates
       are removed), then nonstart noninitial (situation with at least
       one symbol before the dot) situations are placed and then initial
       situations are placed.  You should access to a set situation only
       through this member or variable `new_sits' (in other words don't
       save the member value in another variable). */
    struct sit **sits;
    /* The following member is number of start situations and nonstart
       (noninitial) situations whose distance is defined from a start
       situation distance.  All nonstart initial situations have zero
       distances.  This distances are not stored.  */
    int n_all_dists;
    /* The following is array containing number of start situation from
       which distance of (nonstart noninitial) situation with given
       index (between n_start_situations -> n_all_dists) is taken. */
    int *parent_indexes;
};

/* The following describes set in Earley's algorithm. */
struct set
{
    /* The following is set core of the set.  You should access to set
       core only through this member or variable `new_core' (in other
       words don't save the member value in another variable). */
    struct set_core *core;
    /* Hash of the set distances.  We save it as it is used several
       times.  */
    unsigned int dists_hash;
    /* The following is distances only for start situations.  Other
       situations have their distances equal to 0.  The start situation
       in set core and the corresponding distance has the same index.
       You should access to distances only through this member or
       variable `new_dists' (in other words don't save the member value
       in another variable). */
    int *dists;
};

/* Maximal goto sets saved for triple (set, terminal, lookahead).  */
#define MAX_CACHED_GOTO_RESULTS 3

/* The triple and possible goto sets for it.  */
struct set_term_lookahead
{
    struct set *set;
    struct symb *term;
    int lookahead;
    /* Saved goto sets form a queue.  The last goto is saved at the
       following array elements whose index is given by CURR.  */
    int curr;
    /* Saved goto sets to which we can go from SET by the terminal with
       subsequent terminal LOOKAHEAD given by its code.  */
    struct set *result[MAX_CACHED_GOTO_RESULTS];
    /* Corresponding places of the goto sets in the parsing list.  */
    int place[MAX_CACHED_GOTO_RESULTS];
};

/* The following variable is set being created.  It can be read
   externally.  It is defined only when new_set_ready_p is TRUE. */
static struct set *new_set;

/* The following variable is always set core of set being created.  It
   can be read externally.  Member core of new_set has always the
   following value.  It is defined only when new_set_ready_p is TRUE. */
static struct set_core *new_core;

/* The following says that new_set, new_core and their members are
   defined.  Before this the access to data of the set being formed
   are possible only through the following variables. */
static int new_set_ready_p;

/* To optimize code we use the following variables to access to data
   of new set.  They are always defined and correspondingly
   situations, distances, and the current number of start situations
   of the set being formed. */
static struct sit **new_sits;
static int *new_dists;
static int new_n_start_sits;

/* The following are number of unique set cores and their start
   situations, unique distance vectors and their summary length, and
   number of parent indexes.  The variables can be read externally. */
static int n_set_cores, n_set_core_start_sits;
static int n_set_dists, n_set_dists_len, n_parent_indexes;

/* Number unique sets and their start situations.  */
static int n_sets, n_sets_start_sits;

/* Number unique triples (set, term, lookahead).  */
static int n_set_term_lookaheads;

/* The set cores of formed sets are placed in the following os. */
static os_t set_cores_os;

/* The situations of formed sets are placed in the following os. */
static os_t set_sits_os;

/* The indexes of the parent start situations whose distances are used
   to get distances of some nonstart situations are placed in the
   following os. */
static os_t set_parent_indexes_os;

/* The distances of formed sets are placed in the following os. */
static os_t set_dists_os;

/* The sets themself are placed in the following os. */
static os_t sets_os;

/* Container for triples (set, term, lookahead.  */
static os_t set_term_lookahead_os;

/* The following 3 tables contain references for sets which refers
   for set cores or distances or both which are in the tables. */
static hash_table_t set_core_tab;	/* key is only start situations. */
static hash_table_t set_dists_tab;	/* key is distances. */
static hash_table_t set_tab;	/* key is (core, distances). */
/* Table for triples (set, term, lookahead).  */
static hash_table_t set_term_lookahead_tab;	/* key is (core, distances, lookeahed). */

/* Hash of set core. */
static unsigned
set_core_hash (hash_table_entry_t s)
{
    return ((struct set *) s)->core->hash;
}

/* Equality of set cores. */
static int
set_core_eq (hash_table_entry_t s1, hash_table_entry_t s2)
{
    struct set_core *set_core1 = ((struct set *) s1)->core;
    struct set_core *set_core2 = ((struct set *) s2)->core;
    struct sit **sit_ptr1, **sit_ptr2, **sit_bound1;

    if (set_core1->n_start_sits != set_core2->n_start_sits)
        return FALSE;
    sit_ptr1 = set_core1->sits;
    sit_bound1 = sit_ptr1 + set_core1->n_start_sits;
    sit_ptr2 = set_core2->sits;
    while (sit_ptr1 < sit_bound1)
        if (*sit_ptr1++ != *sit_ptr2++)
            return FALSE;
    return TRUE;
}

/* Hash of set distances. */
static unsigned
dists_hash (hash_table_entry_t s)
{
    return ((struct set *) s)->dists_hash;
}

/* Equality of distances. */
static int
dists_eq (hash_table_entry_t s1, hash_table_entry_t s2)
{
    int *dists1 = ((struct set *) s1)->dists;
    int *dists2 = ((struct set *) s2)->dists;
    int n_dists = ((struct set *) s1)->core->n_start_sits;
    int *bound;

    if (n_dists != ((struct set *) s2)->core->n_start_sits)
        return FALSE;
    bound = dists1 + n_dists;
    while (dists1 < bound)
        if (*dists1++ != *dists2++)
            return FALSE;
    return TRUE;
}

/* Hash of set core and distances. */
static unsigned
set_core_dists_hash (hash_table_entry_t s)
{
    return set_core_hash (s) * hash_shift + dists_hash (s);
}

/* Equality of set cores and distances. */
static int
set_core_dists_eq (hash_table_entry_t s1, hash_table_entry_t s2)
{
    struct set_core *set_core1 = ((struct set *) s1)->core;
    struct set_core *set_core2 = ((struct set *) s2)->core;
    int *dists1 = ((struct set *) s1)->dists;
    int *dists2 = ((struct set *) s2)->dists;

    return set_core1 == set_core2 && dists1 == dists2;
}

/* Hash of triple (set, term, lookahead). */
static unsigned
set_term_lookahead_hash (hash_table_entry_t s)
{
    struct set *set = ((struct set_term_lookahead *) s)->set;
    struct symb *term = ((struct set_term_lookahead *) s)->term;
    int lookahead = ((struct set_term_lookahead *) s)->lookahead;

    return ((set_core_dists_hash (set) * hash_shift
             + term->u.term.term_num) * hash_shift + lookahead);
}

/* Equality of tripes (set, term, lookahead). */
static int
set_term_lookahead_eq (hash_table_entry_t s1, hash_table_entry_t s2)
{
    struct set *set1 = ((struct set_term_lookahead *) s1)->set;
    struct set *set2 = ((struct set_term_lookahead *) s2)->set;
    struct symb *term1 = ((struct set_term_lookahead *) s1)->term;
    struct symb *term2 = ((struct set_term_lookahead *) s2)->term;
    int lookahead1 = ((struct set_term_lookahead *) s1)->lookahead;
    int lookahead2 = ((struct set_term_lookahead *) s2)->lookahead;

    return set1 == set2 && term1 == term2 && lookahead1 == lookahead2;
}



/* This page contains code for table of pairs (sit, dist).  */

/* Vector implementing map: sit number -> vlo of the distance check
   indexed by the distance.  */
static vlo_t sit_dist_vec_vlo;

/* The value used to check the validity of elements of check_dist
   structures.  */
static int curr_sit_dist_vec_check;

/* Initiate the set of pairs (sit, dist).  */
static void
sit_dist_set_init (void)
{
    VLO_CREATE (sit_dist_vec_vlo, grammar->alloc, 8192);
    curr_sit_dist_vec_check = 0;
}

/* Make the set empty.  */
static void
empty_sit_dist_set (void)
{
    curr_sit_dist_vec_check++;
}

/* Insert pair (SIT, DIST) into the set.  If such pair exists return
   FALSE, otherwise return TRUE.  */
static int
sit_dist_insert (struct sit *sit, int dist)
{
    int i, len, sit_number;
    vlo_t *check_dist_vlo;

    sit_number = sit->sit_number;
    /* Expand the set to accommodate possibly a new situation.  */
    len = VLO_LENGTH (sit_dist_vec_vlo) / sizeof (vlo_t);
    if (len <= sit_number)
    {
        VLO_EXPAND (sit_dist_vec_vlo, (sit_number + 1 - len) * sizeof (vlo_t));
        for (i = len; i <= sit_number; i++)
            VLO_CREATE (((vlo_t *) VLO_BEGIN (sit_dist_vec_vlo))[i],
                        grammar->alloc, 64);
    }
    check_dist_vlo = &((vlo_t *) VLO_BEGIN (sit_dist_vec_vlo))[sit_number];
    len = VLO_LENGTH (*check_dist_vlo) / sizeof (int);
    if (len <= dist)
    {
        VLO_EXPAND (*check_dist_vlo, (dist + 1 - len) * sizeof (int));
        for (i = len; i <= dist; i++)
            ((int *) VLO_BEGIN (*check_dist_vlo))[i] = 0;
    }
    if (((int *) VLO_BEGIN (*check_dist_vlo))[dist] == curr_sit_dist_vec_check)
        return FALSE;
    ((int *) VLO_BEGIN (*check_dist_vlo))[dist] = curr_sit_dist_vec_check;
    return TRUE;
}

/* Finish the set of pairs (sit, dist).  */
static void
sit_dist_set_fin (void)
{
    int i, len = VLO_LENGTH (sit_dist_vec_vlo) / sizeof (vlo_t);

    for (i = 0; i < len; i++)
        VLO_DELETE (((vlo_t *) VLO_BEGIN (sit_dist_vec_vlo))[i]);
    VLO_DELETE (sit_dist_vec_vlo);
}



#ifdef TRANSITIVE_TRANSITION
/* The following varaibles are used for *using* transitive transition
   vectors to exclude multiple situation processing.  */
static int curr_sit_check;

/* The following varaibles are used for *building* transitive transition
   vectors:  */
/*  The value is used to mark already processed symbols.  */
static int core_symbol_check;
/* The first is used to check already processed symbols.  The second
   contains symbols to be processed.  The third is a queue used during
   building transitive transitions.  */
static vlo_t core_symbol_check_vlo, core_symbols_vlo, core_symbol_queue_vlo;

#endif

/* Initialize work with sets for parsing input with N_TOKS tokens. */
static void
set_init (int n_toks)
{
    int n = n_toks >> 3;

    OS_CREATE (set_cores_os, grammar->alloc, 0);
    OS_CREATE (set_sits_os, grammar->alloc, 2048);
    OS_CREATE (set_parent_indexes_os, grammar->alloc, 2048);
    OS_CREATE (set_dists_os, grammar->alloc, 2048);
    OS_CREATE (sets_os, grammar->alloc, 0);
    OS_CREATE (set_term_lookahead_os, grammar->alloc, 0);
    set_core_tab =
        create_hash_table (grammar->alloc, 2000, set_core_hash, set_core_eq);
    set_dists_tab =
        create_hash_table (grammar->alloc, n < 20000 ? 20000 : n, dists_hash,
                           dists_eq);
    set_tab =
        create_hash_table (grammar->alloc, n < 20000 ? 20000 : n,
                           set_core_dists_hash, set_core_dists_eq);
    set_term_lookahead_tab =
        create_hash_table (grammar->alloc, n < 30000 ? 30000 : n,
                           set_term_lookahead_hash, set_term_lookahead_eq);
    n_set_cores = n_set_core_start_sits = 0;
    n_set_dists = n_set_dists_len = n_parent_indexes = 0;
    n_sets = n_sets_start_sits = 0;
    n_set_term_lookaheads = 0;
    sit_dist_set_init ();
#ifdef TRANSITIVE_TRANSITION
    curr_sit_check = 0;
    VLO_CREATE (core_symbol_check_vlo, grammar->alloc, 0);
    VLO_CREATE (core_symbols_vlo, grammar->alloc, 0);
    VLO_CREATE (core_symbol_queue_vlo, grammar->alloc, 0);
    core_symbol_check = 0;
#endif
}

/* The following function starts forming of new set. */
#if MAKE_INLINE
INLINE
#endif
static void
set_new_start (void)
{
    new_set = NULL;
    new_core = NULL;
    new_set_ready_p = FALSE;
    new_n_start_sits = 0;
    new_sits = NULL;
    new_dists = NULL;
}

/* Add start SIT with distance DIST at the end of the situation array
   of the set being formed. */
#if MAKE_INLINE
INLINE
#endif
static void
set_new_add_start_sit (struct sit *sit, int dist)
{
    assert (!new_set_ready_p);
    OS_TOP_EXPAND (set_dists_os, sizeof (int));
    new_dists = (int *) OS_TOP_BEGIN (set_dists_os);
    OS_TOP_EXPAND (set_sits_os, sizeof (struct sit *));
    new_sits = (struct sit **) OS_TOP_BEGIN (set_sits_os);
    new_sits[new_n_start_sits] = sit;
    new_dists[new_n_start_sits] = dist;
    new_n_start_sits++;
}

/* Add nonstart, noninitial SIT with distance DIST at the end of the
   situation array of the set being formed.  If this is situation and
   there is already the same pair (situation, the corresponding
   distance), we do not add it. */
#if MAKE_INLINE
INLINE
#endif
static void
set_add_new_nonstart_sit (struct sit *sit, int parent)
{
    int i;

    assert (new_set_ready_p);
    /* When we add non-start situations we need to have pairs
       (situation, the corresponding distance) without duplicates
       because we also forms core_symb_vect at that time. */
    for (i = new_n_start_sits; i < new_core->n_sits; i++)
        if (new_sits[i] == sit && new_core->parent_indexes[i] == parent)
            return;
    OS_TOP_EXPAND (set_sits_os, sizeof (struct sit *));
    new_sits = new_core->sits = (struct sit **) OS_TOP_BEGIN (set_sits_os);
    OS_TOP_EXPAND (set_parent_indexes_os, sizeof (int));
    new_core->parent_indexes
        = (int *) OS_TOP_BEGIN (set_parent_indexes_os) - new_n_start_sits;
    new_sits[new_core->n_sits++] = sit;
    new_core->parent_indexes[new_core->n_all_dists++] = parent;
    n_parent_indexes++;
}

/* Add non-start (initial) SIT with zero distance at the end of the
   situation array of the set being formed.  If this is non-start
   situation and there is already the same pair (situation, zero
   distance), we do not add it. */
#if MAKE_INLINE
INLINE
#endif
static void
set_new_add_initial_sit (struct sit *sit)
{
    int i;

    assert (new_set_ready_p);
    /* When we add non-start situations we need to have pairs
       (situation, the corresponding distance) without duplicates
       because we also forms core_symb_vect at that time. */
    for (i = new_n_start_sits; i < new_core->n_sits; i++)
        if (new_sits[i] == sit)
            return;
    /* Remember we do not store distance for non-start situations. */
    OS_TOP_ADD_MEMORY (set_sits_os, &sit, sizeof (struct sit *));
    new_sits = new_core->sits = (struct sit **) OS_TOP_BEGIN (set_sits_os);
    new_core->n_sits++;
}

/* Set up hash of distances of set S. */
static void
setup_set_dists_hash (hash_table_entry_t s)
{
    struct set *set = ((struct set *) s);
    int *dist_ptr = set->dists;
    int n_dists = set->core->n_start_sits;
    int *dist_bound;
    unsigned result;

    dist_bound = dist_ptr + n_dists;
    result = jauquet_prime_mod32;
    while (dist_ptr < dist_bound)
        result = result * hash_shift + *dist_ptr++;
    set->dists_hash = result;
}

/* Set up hash of core of set S. */
static void
setup_set_core_hash (hash_table_entry_t s)
{
    struct set_core *set_core = ((struct set *) s)->core;

    set_core->hash = sits_hash (set_core->n_start_sits, set_core->sits);
}

/* The new set should contain only start situations.  Sort situations,
   remove duplicates and insert set into the set table.  If the
   function returns TRUE then set contains new set core (there was no
   such core in the table). */
#if MAKE_INLINE
INLINE
#endif
static int
set_insert (void)
{
    hash_table_entry_t *entry;
    int result;

    OS_TOP_EXPAND (sets_os, sizeof (struct set));
    new_set = (struct set *) OS_TOP_BEGIN (sets_os);
    new_set->dists = new_dists;
    OS_TOP_EXPAND (set_cores_os, sizeof (struct set_core));
    new_set->core = new_core = (struct set_core *) OS_TOP_BEGIN (set_cores_os);
    new_core->n_start_sits = new_n_start_sits;
    new_core->sits = new_sits;
    new_set_ready_p = TRUE;
#if defined(USE_DIST_HASH_TABLE) || defined (USE_SET_HASH_TABLE)
    /* Insert dists into table. */
    setup_set_dists_hash (new_set);
    entry = find_hash_table_entry (set_dists_tab, new_set, TRUE);
    if (*entry != NULL)
    {
        new_dists = new_set->dists = ((struct set *) *entry)->dists;
        OS_TOP_NULLIFY (set_dists_os);
    }
    else
    {
        OS_TOP_FINISH (set_dists_os);
        *entry = (hash_table_entry_t) new_set;
        n_set_dists++;
        n_set_dists_len += new_n_start_sits;
    }
#else
    OS_TOP_FINISH (set_dists_os);
    n_set_dists++;
    n_set_dists_len += new_n_start_sits;
#endif
    /* Insert set core into table. */
    setup_set_core_hash (new_set);
    entry = find_hash_table_entry (set_core_tab, new_set, TRUE);
    if (*entry != NULL)
    {
        OS_TOP_NULLIFY (set_cores_os);
        new_set->core = new_core = ((struct set *) *entry)->core;
        new_sits = new_core->sits;
        OS_TOP_NULLIFY (set_sits_os);
        result = FALSE;
    }
    else
    {
        OS_TOP_FINISH (set_cores_os);
        new_core->num = n_set_cores++;
        new_core->n_sits = new_n_start_sits;
        new_core->n_all_dists = new_n_start_sits;
        new_core->parent_indexes = NULL;
        *entry = (hash_table_entry_t) new_set;
        n_set_core_start_sits += new_n_start_sits;
        result = TRUE;
    }
#ifdef USE_SET_HASH_TABLE
    /* Insert set into table. */
    entry = find_hash_table_entry (set_tab, new_set, TRUE);
    if (*entry == NULL)
    {
        *entry = (hash_table_entry_t) new_set;
        n_sets++;
        n_sets_start_sits += new_n_start_sits;
        OS_TOP_FINISH (sets_os);
    }
    else
    {
        new_set = (struct set *) *entry;
        OS_TOP_NULLIFY (sets_os);
    }
#else
    OS_TOP_FINISH (sets_os);
#endif
    return result;
}

/* The following function finishes work with set being formed. */
#if MAKE_INLINE
INLINE
#endif
static void
set_new_core_stop (void)
{
    OS_TOP_FINISH (set_sits_os);
    OS_TOP_FINISH (set_parent_indexes_os);
}

#ifndef NO_YAEP_DEBUG_PRINT

/* The following function prints SET to file F.  If NONSTART_P is TRUE
   then print all situations.  The situations are printed with the
   lookahead set if LOOKAHEAD_P.  SET_DIST is used to print absolute
   distances of non-start situations.  */
static void
set_print (FILE * f, struct set *set, int set_dist, int nonstart_p,
	   int lookahead_p)
{
    int i;
    int num, n_start_sits, n_sits, n_all_dists;
    struct sit **sits;
    int *dists, *parent_indexes;

    if (set == NULL && !new_set_ready_p)
    {
        /* The following is necessary if we call the function from a
           debugger.  In this case new_set, new_core and their members
           may be not set up yet. */
        num = -1;
        n_start_sits = n_sits = n_all_dists = new_n_start_sits;
        sits = new_sits;
        dists = new_dists;
        parent_indexes = NULL;
    }
    else
    {
        num = set->core->num;
        n_sits = set->core->n_sits;
        sits = set->core->sits;
        n_start_sits = set->core->n_start_sits;
        dists = set->dists;
        n_all_dists = set->core->n_all_dists;
        parent_indexes = set->core->parent_indexes;
        n_start_sits = set->core->n_start_sits;
    }
    fprintf (f, "  Set core = %d\n", num);
    for (i = 0; i < n_sits; i++)
    {
        fprintf (f, "    ");
        sit_print (f, sits[i], lookahead_p);
        fprintf (f, ", %d\n",
                 (i < n_start_sits
                  ? dists[i] : i < n_all_dists ? parent_indexes[i]
#ifndef ABSOLUTE_DISTANCES
                  : 0));
#else
        :		set_dist));
#endif
if (i == n_start_sits - 1)
{
    if (!nonstart_p)
        break;
    fprintf (f, "    -----------\n");
}
}
}

#endif /* #ifndef NO_YAEP_DEBUG_PRINT */

/* Finalize work with sets. */
static void
set_fin (void)
{
#ifdef TRANSITIVE_TRANSITION
    VLO_DELETE (core_symbol_queue_vlo);
    VLO_DELETE (core_symbols_vlo);
    VLO_DELETE (core_symbol_check_vlo);
#endif
    sit_dist_set_fin ();
    delete_hash_table (set_term_lookahead_tab);
    delete_hash_table (set_tab);
    delete_hash_table (set_dists_tab);
    delete_hash_table (set_core_tab);
    OS_DELETE (set_term_lookahead_os);
    OS_DELETE (sets_os);
    OS_DELETE (set_parent_indexes_os);
    OS_DELETE (set_sits_os);
    OS_DELETE (set_dists_os);
    OS_DELETE (set_cores_os);
}



/* This page is abstract data `parser list'. */

/* The following two variables represents Earley's parser list.  The
   values of pl_curr and array *pl can be read and modified
   externally. */
static struct set **pl;
static int pl_curr;

/* Initialize work with the parser list. */
static void
pl_init (void)
{
    pl = NULL;
}

/* The following function creates Earley's parser list. */
static void
pl_create (void)
{
    void *mem;

    /* Because of error recovery we may have sets 2 times more than tokens. */
    mem =
        yaep_malloc (grammar->alloc, sizeof (struct set *) * (toks_len + 1) * 2);
    pl = (struct set **) mem;
    pl_curr = -1;
}

/* Finalize work with the parser list. */
static void
pl_fin (void)
{
    if (pl != NULL)
        yaep_free (grammar->alloc, pl);
}




/* This page contains code for work with array of vlos.  It is used
   only to implement abstract data `core_symb_vect'. */

/* All vlos being formed are placed in the following object. */
static vlo_t vlo_array;

/* The following is current number of elements in vlo_array. */
static int vlo_array_len;

/* Initialize work with array of vlos. */
#if MAKE_INLINE
INLINE
#endif
static void
vlo_array_init (void)
{
    VLO_CREATE (vlo_array, grammar->alloc, 4096);
    vlo_array_len = 0;
}

/* The function forms new empty vlo at the end of the array of
   vlos. */
#if MAKE_INLINE
INLINE
#endif
static int
vlo_array_expand (void)
{
    vlo_t *vlo_ptr;

    if ((unsigned) vlo_array_len >= VLO_LENGTH (vlo_array) / sizeof (vlo_t))
    {
        VLO_EXPAND (vlo_array, sizeof (vlo_t));
        vlo_ptr = &((vlo_t *) VLO_BEGIN (vlo_array))[vlo_array_len];
        VLO_CREATE (*vlo_ptr, grammar->alloc, 64);
    }
    else
    {
        vlo_ptr = &((vlo_t *) VLO_BEGIN (vlo_array))[vlo_array_len];
        VLO_NULLIFY (*vlo_ptr);
    }
    return vlo_array_len++;
}

/* The function purges the array of vlos. */
#if MAKE_INLINE
INLINE
#endif
static void
vlo_array_nullify (void)
{
    vlo_array_len = 0;
}

/* The following function returns pointer to vlo with INDEX. */
#if MAKE_INLINE
INLINE
#endif
static vlo_t *
vlo_array_el (int index)
{
    assert (index >= 0 && vlo_array_len > index);
    return &((vlo_t *) VLO_BEGIN (vlo_array))[index];
}

/* Finalize work with array of vlos. */
#if MAKE_INLINE
INLINE
#endif
static void
vlo_array_fin (void)
{
    vlo_t *vlo_ptr;

    for (vlo_ptr = (vlo_t *) VLO_BEGIN (vlo_array);
         (char *) vlo_ptr < (char *) VLO_BOUND (vlo_array); vlo_ptr++)
        VLO_DELETE (*vlo_ptr);
    VLO_DELETE (vlo_array);
}



/* This page contains table for fast search for vector of indexes of
   situations with symbol after dot in given set core. */

struct vect
{
    /* The following member is used internally.  The value is
       nonnegative for core_symb_vect being formed.  It is index of vlo
       in vlos array which contains the vector elements. */
    int intern;
    /* The following memebers defines array of indexes of situations in
       given set core.  You should access to values through these
       members (in other words don't save the member values in another
       variable). */
    int len;
    int *els;
};

/* The following is element of the table. */
struct core_symb_vect
{
    /* The set core. */
    struct set_core *set_core;
    /* The symbol. */
    struct symb *symb;
    /* The following vector contains indexes of situations with given
       symb in situation after dot. */
    struct vect transitions;
#ifdef TRANSITIVE_TRANSITION
    /* The following vector contains indexes of situations with given
       symb in situation after the dot and situations from the same set
       core produced by reductions of the situations in this vector.
       For example, if we have

       N0 -> <...> . N1 <...>
       N1 -> .N2
       N2 -> .N3
       N3 -> .t

       the elements for t will contain also N1 -> .N2, N2 -> .N3, and
       N0 -> <...> . N1 <...>
    */
    struct vect transitive_transitions;
#endif
    /* The following vector contains indexes of reduce situations with
       given symb in lhs. */
    struct vect reduces;
};


/* The following are number of unique (set core, symbol) pairs and
   their summary (transitive) transition and reduce vectors length,
   unique (transitive) transition vectors and their summary length,
   and unique reduce vectors and their summary length.  The variables
   can be read externally. */
static int n_core_symb_pairs, n_core_symb_vect_len;
static int n_transition_vects, n_transition_vect_len;
#ifdef TRANSITIVE_TRANSITION
static int n_transitive_transition_vects, n_transitive_transition_vect_len;
#endif
static int n_reduce_vects, n_reduce_vect_len;

/* All triples (set core, symbol, vect) are placed in the following
   object. */
static os_t core_symb_vect_os;

/* Pointers to triples (set core, symbol, vect) being formed are
   placed in the following object. */
static vlo_t new_core_symb_vect_vlo;

/* All elements of vectors in the table (see
   (transitive_)transition_els_tab and reduce_els_tab) are placed in
   the following os. */
static os_t vect_els_os;

#ifdef USE_CORE_SYMB_HASH_TABLE
static hash_table_t core_symb_to_vect_tab;	/* key is set_core and symb. */
#else
/* The following two variables contains table (set core,
   symbol)->core_symb_vect implemented as two dimensional array. */
/* The following object contains pointers to the table rows for each
   set core. */
static vlo_t core_symb_table_vlo;

/* The following is always start of the previous object. */
static struct core_symb_vect ***core_symb_table;

/* The following contains rows of the table.  The element in the rows
   are indexed by symbol number. */
static os_t core_symb_tab_rows;
#endif

/* The following tables contains references for core_symb_vect which
   (through (transitive) transitions and reduces correspondingly)
   refers for elements which are in the tables.  Sequence elements are
   stored in one exemplar to save memory. */
static hash_table_t transition_els_tab;	/* key is elements. */
#ifdef TRANSITIVE_TRANSITION
static hash_table_t transitive_transition_els_tab;	/* key is elements. */
#endif
static hash_table_t reduce_els_tab;	/* key is elements. */

#ifdef USE_CORE_SYMB_HASH_TABLE
/* Hash of core_symb_vect. */
static unsigned
core_symb_vect_hash (hash_table_entry_t t)
{
    struct core_symb_vect *core_symb_vect = (struct core_symb_vect *) t;

    return ((jauquet_prime_mod32 * hash_shift
             + (unsigned) core_symb_vect->set_core) * hash_shift
            + (unsigned) core_symb_vect->symb);
}

/* Equality of core_symb_vects. */
static int
core_symb_vect_eq (hash_table_entry_t t1, hash_table_entry_t t2)
{
    struct core_symb_vect *core_symb_vect1 = (struct core_symb_vect *) t1;
    struct core_symb_vect *core_symb_vect2 = (struct core_symb_vect *) t2;

    return (core_symb_vect1->set_core == core_symb_vect2->set_core
            && core_symb_vect1->symb == core_symb_vect2->symb);
}
#endif

/* Return hash of vector V.  */
static unsigned
vect_els_hash (struct vect *v)
{
    unsigned result = jauquet_prime_mod32;
    int i;

    for (i = 0; i < v->len; i++)
        result = result * hash_shift + v->els[i];
    return result;
}

/* Return TRUE if V1 is equal to V2.  */
static unsigned
vect_els_eq (struct vect *v1, struct vect *v2)
{
    int i;
    if (v1->len != v2->len)
        return FALSE;

    for (i = 0; i < v1->len; i++)
        if (v1->els[i] != v2->els[i])
            return FALSE;
    return TRUE;
}

/* Hash of vector transition elements. */
static unsigned
transition_els_hash (hash_table_entry_t t)
{
    return vect_els_hash (&((struct core_symb_vect *) t)->transitions);
}

/* Equality of transition vector elements. */
static int
transition_els_eq (hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_els_eq (&((struct core_symb_vect *) t1)->transitions,
                        &((struct core_symb_vect *) t2)->transitions);
}

#ifdef TRANSITIVE_TRANSITION
/* Hash of vector transitive transition elements. */
static unsigned
transitive_transition_els_hash (hash_table_entry_t t)
{
    return vect_els_hash (&((struct core_symb_vect *) t)->
                          transitive_transitions);
}

/* Equality of transitive transition vector elements. */
static int
transitive_transition_els_eq (hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_els_eq (&((struct core_symb_vect *) t1)->transitive_transitions,
                        &((struct core_symb_vect *) t2)->
                        transitive_transitions);
}
#endif

/* Hash of reduce vector elements. */
static unsigned
reduce_els_hash (hash_table_entry_t t)
{
    return vect_els_hash (&((struct core_symb_vect *) t)->reduces);
}

/* Equality of reduce vector elements. */
static int
reduce_els_eq (hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_els_eq (&((struct core_symb_vect *) t1)->reduces,
                        &((struct core_symb_vect *) t2)->reduces);
}

/* Initialize work with the triples (set core, symbol, vector). */
static void
core_symb_vect_init (void)
{
    OS_CREATE (core_symb_vect_os, grammar->alloc, 0);
    VLO_CREATE (new_core_symb_vect_vlo, grammar->alloc, 0);
    OS_CREATE (vect_els_os, grammar->alloc, 0);

    vlo_array_init ();
#ifdef USE_CORE_SYMB_HASH_TABLE
    core_symb_to_vect_tab =
        create_hash_table (grammar->alloc, 3000, core_symb_vect_hash,
                           core_symb_vect_eq);
#else
    VLO_CREATE (core_symb_table_vlo, grammar->alloc, 4096);
    core_symb_table
        = (struct core_symb_vect ***) VLO_BEGIN (core_symb_table_vlo);
    OS_CREATE (core_symb_tab_rows, grammar->alloc, 8192);
#endif

    transition_els_tab =
        create_hash_table (grammar->alloc, 3000, transition_els_hash,
                           transition_els_eq);
#ifdef TRANSITIVE_TRANSITION
    transitive_transition_els_tab =
        create_hash_table (grammar->alloc, 5000, transitive_transition_els_hash,
                           transitive_transition_els_eq);
#endif
    reduce_els_tab =
        create_hash_table (grammar->alloc, 3000, reduce_els_hash, reduce_els_eq);

    n_core_symb_pairs = n_core_symb_vect_len = 0;
    n_transition_vects = n_transition_vect_len = 0;
#ifdef TRANSITIVE_TRANSITION
    n_transitive_transition_vects = n_transitive_transition_vect_len = 0;
#endif
    n_reduce_vects = n_reduce_vect_len = 0;
}

#ifdef USE_CORE_SYMB_HASH_TABLE

/* The following function returns entry in the table where pointer to
   corresponding triple with the same keys as TRIPLE ones is
   placed. */
#if MAKE_INLINE
INLINE
#endif
static struct core_symb_vect **
core_symb_vect_addr_get (struct core_symb_vect *triple, int reserv_p)
{
    struct core_symb_vect **result;

    if (triple->symb->cached_core_symb_vect != NULL
        && triple->symb->cached_core_symb_vect->set_core == triple->set_core)
        return &triple->symb->cached_core_symb_vect;
    result = ((struct core_symb_vect **)
              find_hash_table_entry (core_symb_to_vect_tab, triple, reserv_p));

    triple->symb->cached_core_symb_vect = *result;
    return result;
}

#else

/* The following function returns entry in the table where pointer to
   corresponding triple with SET_CORE and SYMB is placed. */
#if MAKE_INLINE
INLINE
#endif
static struct core_symb_vect **
core_symb_vect_addr_get (struct set_core *set_core, struct symb *symb)
{
    struct core_symb_vect ***core_symb_vect_ptr;

    core_symb_vect_ptr = core_symb_table + set_core->num;

    if ((char *) core_symb_vect_ptr >= (char *) VLO_BOUND (core_symb_table_vlo))
    {
            struct core_symb_vect ***ptr, ***bound;
            int diff, i;

            diff = ((char *) core_symb_vect_ptr
                    - (char *) VLO_BOUND (core_symb_table_vlo));
            diff += sizeof (struct core_symb_vect **);
            if (diff == sizeof (struct core_symb_vect **))
                diff *= 10;

            VLO_EXPAND (core_symb_table_vlo, diff);
            core_symb_table
                = (struct core_symb_vect ***) VLO_BEGIN (core_symb_table_vlo);
            core_symb_vect_ptr = core_symb_table + set_core->num;
            bound = (struct core_symb_vect ***) VLO_BOUND (core_symb_table_vlo);

            ptr = bound - diff / sizeof (struct core_symb_vect **);
            while (ptr < bound)
            {
                OS_TOP_EXPAND (core_symb_tab_rows,
                               (symbs_ptr->n_terms + symbs_ptr->n_nonterms)
                               * sizeof (struct core_symb_vect *));
                *ptr = (struct core_symb_vect **) OS_TOP_BEGIN (core_symb_tab_rows);
                OS_TOP_FINISH (core_symb_tab_rows);
                for (i = 0; i < symbs_ptr->n_terms + symbs_ptr->n_nonterms; i++)
                    (*ptr)[i] = NULL;
                ptr++;
            }
        }
    return &(*core_symb_vect_ptr)[symb->num];
}
#endif

/* The following function returns the triple (if any) for given
   SET_CORE and SYMB. */
#if MAKE_INLINE
INLINE
#endif
static struct core_symb_vect *
core_symb_vect_find (struct set_core *set_core, struct symb *symb)
{
#ifdef USE_CORE_SYMB_HASH_TABLE
    struct core_symb_vect core_symb_vect;

    core_symb_vect.set_core = set_core;
    core_symb_vect.symb = symb;
    return *core_symb_vect_addr_get (&core_symb_vect, FALSE);
#else
    return *core_symb_vect_addr_get (set_core, symb);
#endif
}

/* Add given triple (SET_CORE, TERM, ...) to the table and return
   pointer to it. */
static struct core_symb_vect *
core_symb_vect_new (struct set_core *set_core, struct symb *symb)
{
    struct core_symb_vect *triple;
    struct core_symb_vect **addr;
    vlo_t *vlo_ptr;

    /* Create table element. */
    OS_TOP_EXPAND (core_symb_vect_os, sizeof (struct core_symb_vect));
    triple = ((struct core_symb_vect *) OS_TOP_BEGIN (core_symb_vect_os));
    triple->set_core = set_core;
    triple->symb = symb;
    OS_TOP_FINISH (core_symb_vect_os);

#ifdef USE_CORE_SYMB_HASH_TABLE
    addr = core_symb_vect_addr_get (triple, TRUE);
#else
    addr = core_symb_vect_addr_get (set_core, symb);
#endif
    assert (*addr == NULL);
    *addr = triple;

    triple->transitions.intern = vlo_array_expand ();
    vlo_ptr = vlo_array_el (triple->transitions.intern);
    triple->transitions.len = 0;
    triple->transitions.els = (int *) VLO_BEGIN (*vlo_ptr);

#ifdef TRANSITIVE_TRANSITION
    triple->transitive_transitions.intern = vlo_array_expand ();
    vlo_ptr = vlo_array_el (triple->transitive_transitions.intern);
    triple->transitive_transitions.len = 0;
    triple->transitive_transitions.els = (int *) VLO_BEGIN (*vlo_ptr);
#endif

    triple->reduces.intern = vlo_array_expand ();
    vlo_ptr = vlo_array_el (triple->reduces.intern);
    triple->reduces.len = 0;
    triple->reduces.els = (int *) VLO_BEGIN (*vlo_ptr);
    VLO_ADD_MEMORY (new_core_symb_vect_vlo, &triple,
                    sizeof (struct core_symb_vect *));
    n_core_symb_pairs++;
    return triple;
}

/* Add EL to vector VEC.  */
static void
vect_new_add_el (struct vect *vec, int el)
{
    vlo_t *vlo_ptr;

    vec->len++;
    vlo_ptr = vlo_array_el (vec->intern);
    VLO_ADD_MEMORY (*vlo_ptr, &el, sizeof (int));
    vec->els = (int *) VLO_BEGIN (*vlo_ptr);
    n_core_symb_vect_len++;
}

/* Add index EL to the transition vector of CORE_SYMB_VECT being
   formed. */
static void
core_symb_vect_new_add_transition_el (struct core_symb_vect *core_symb_vect,
				      int el)
{
    vect_new_add_el (&core_symb_vect->transitions, el);
}

#ifdef TRANSITIVE_TRANSITION
/* Add index EL to the transition vector of CORE_SYMB_VECT being
   formed. */
static void
core_symb_vect_new_add_transitive_transition_el (struct core_symb_vect
						 *core_symb_vect, int el)
{
    vect_new_add_el (&core_symb_vect->transitive_transitions, el);
}
#endif

/* Add index EL to the reduce vector of CORE_SYMB_VECT being
   formed. */
static void
core_symb_vect_new_add_reduce_el (struct core_symb_vect *core_symb_vect,
				  int el)
{
    vect_new_add_el (&core_symb_vect->reduces, el);
}

/* Insert vector VEC from CORE_SYMB_VECT into table TAB.  Update
 *N_VECTS and INT *N_VECT_LEN if it is a new vector in the
 table.  */
static void
process_core_symb_vect_el (struct core_symb_vect *core_symb_vect,
			   struct vect *vec,
			   hash_table_t * tab, int *n_vects, int *n_vect_len)
{
    hash_table_entry_t *entry;

    if (vec->len == 0)
        vec->els = NULL;
    else
    {
        entry = find_hash_table_entry (*tab, core_symb_vect, TRUE);
        if (*entry != NULL)
            vec->els
                = (&core_symb_vect->transitions == vec
                   ? ((struct core_symb_vect *) *entry)->transitions.els
#ifdef TRANSITIVE_TRANSITION
                   : &core_symb_vect->transitive_transitions == vec
                   ? ((struct core_symb_vect *) *entry)->transitive_transitions.els
#endif
                   : ((struct core_symb_vect *) *entry)->reduces.els);
        else
	{
            *entry = (hash_table_entry_t) core_symb_vect;
            OS_TOP_ADD_MEMORY (vect_els_os, vec->els, vec->len * sizeof (int));
            vec->els = (int *) OS_TOP_BEGIN (vect_els_os);
            OS_TOP_FINISH (vect_els_os);
            (*n_vects)++;
            *n_vect_len += vec->len;
	}
    }
    vec->intern = -1;
}

/* Finish forming all new triples core_symb_vect. */
static void
core_symb_vect_new_all_stop (void)
{
    struct core_symb_vect **triple_ptr;

    for (triple_ptr = (struct core_symb_vect **) VLO_BEGIN (new_core_symb_vect_vlo);
         (char *) triple_ptr < (char *) VLO_BOUND (new_core_symb_vect_vlo);
         triple_ptr++)
        {
            process_core_symb_vect_el (*triple_ptr, &(*triple_ptr)->transitions,
                                       &transition_els_tab, &n_transition_vects,
                                       &n_transition_vect_len);
#ifdef TRANSITIVE_TRANSITION
            process_core_symb_vect_el
                (*triple_ptr, &(*triple_ptr)->transitive_transitions,
                 &transitive_transition_els_tab, &n_transitive_transition_vects,
                 &n_transitive_transition_vect_len);
#endif
            process_core_symb_vect_el (*triple_ptr, &(*triple_ptr)->reduces,
                                       &reduce_els_tab, &n_reduce_vects,
                                       &n_reduce_vect_len);
        }
    vlo_array_nullify ();
    VLO_NULLIFY (new_core_symb_vect_vlo);
}

/* Finalize work with all triples (set core, symbol, vector). */
static void
core_symb_vect_fin (void)
{
    delete_hash_table (transition_els_tab);
#ifdef TRANSITIVE_TRANSITION
    delete_hash_table (transitive_transition_els_tab);
#endif
    delete_hash_table (reduce_els_tab);

#ifdef USE_CORE_SYMB_HASH_TABLE
    delete_hash_table (core_symb_to_vect_tab);
#else
    OS_DELETE (core_symb_tab_rows);
    VLO_DELETE (core_symb_table_vlo);
#endif
    vlo_array_fin ();
    OS_DELETE (vect_els_os);
    VLO_DELETE (new_core_symb_vect_vlo);
    OS_DELETE (core_symb_vect_os);
}



/* Jump buffer for processing errors. */
static jmp_buf error_longjump_buff;

/* The following function stores error CODE and MESSAGE.  The function
   makes long jump after that.  So the function is designed to process
   only one error. */
static void
yaep_error (int code, const char *format, ...)
{
    va_list arguments;

    grammar->error_code = code;
    va_start (arguments, format);
    vsprintf (grammar->error_message, format, arguments);
    va_end (arguments);
    assert (strlen (grammar->error_message) < YAEP_MAX_ERROR_MESSAGE_LENGTH);
    longjmp (error_longjump_buff, code);
}

/* The following function processes allocation errors. */
static void
error_func_for_allocate (void *ignored)
{
    (void) ignored;

    yaep_error (YAEP_NO_MEMORY, "no memory");
}

/* The following function allocates memory for new grammar. */
struct grammar *
yaep_create_grammar (void)
{
    YaepAllocator *allocator;

    allocator = yaep_alloc_new (NULL, NULL, NULL, NULL);
    if (allocator == NULL)
    {
        return NULL;
    }
    grammar = NULL;
    grammar = (struct grammar *) yaep_malloc (allocator, sizeof (*grammar));
    if (grammar == NULL)
    {
        yaep_alloc_del (allocator);
        return NULL;
    }
    grammar->alloc = allocator;
    yaep_alloc_seterr (allocator, error_func_for_allocate,
                       yaep_alloc_getuserptr (allocator));
    if (setjmp (error_longjump_buff) != 0)
    {
        yaep_free_grammar (grammar);
        return NULL;
    }
    grammar->undefined_p = TRUE;
    grammar->error_code = 0;
    *grammar->error_message = '\0';
    grammar->debug_level = 0;
    grammar->lookahead_level = 1;
    grammar->one_parse_p = 1;
    grammar->cost_p = 0;
    grammar->error_recovery_p = 1;
    grammar->recovery_token_matches = DEFAULT_RECOVERY_TOKEN_MATCHES;
    grammar->symbs_ptr = NULL;
    grammar->term_sets_ptr = NULL;
    grammar->rules_ptr = NULL;
    grammar->symbs_ptr = symbs_ptr = symb_init ();
    grammar->term_sets_ptr = term_sets_ptr = term_set_init ();
    grammar->rules_ptr = rules_ptr = rule_init ();
    return grammar;
}

/* The following function makes grammar empty. */
static void
yaep_empty_grammar (void)
{
    if (grammar != NULL)
    {
        rule_empty (grammar->rules_ptr);
        term_set_empty (grammar->term_sets_ptr);
        symb_empty (grammar->symbs_ptr);
    }
}

/* The function returns the last occurred error code for given
   grammar. */
int
yaep_error_code (struct grammar *g)
{
    assert (g != NULL);
    return g->error_code;
}

/* The function returns message are always contains error message
   corresponding to the last occurred error code. */
const char *
yaep_error_message (struct grammar *g)
{
    assert (g != NULL);
    return g->error_message;
}

/* The following function creates sets FIRST and FOLLOW for all
   grammar nonterminals. */
static void
create_first_follow_sets (void)
{
    struct symb *symb, **rhs, *rhs_symb, *next_rhs_symb;
    struct rule *rule;
    int changed_p, first_continue_p;
    int i, j, k, rhs_len;

    for (i = 0; (symb = nonterm_get (i)) != NULL; i++)
    {
        symb->u.nonterm.first = term_set_create ();
        term_set_clear (symb->u.nonterm.first);
        symb->u.nonterm.follow = term_set_create ();
        term_set_clear (symb->u.nonterm.follow);
    }
    do
    {
        changed_p = 0;
        for (i = 0; (symb = nonterm_get (i)) != NULL; i++)
            for (rule = symb->u.nonterm.rules;
                 rule != NULL; rule = rule->lhs_next)
            {
                first_continue_p = TRUE;
                rhs = rule->rhs;
                rhs_len = rule->rhs_len;
                for (j = 0; j < rhs_len; j++)
                {
                    rhs_symb = rhs[j];
                    if (rhs_symb->term_p)
                    {
                        if (first_continue_p)
                            changed_p |= term_set_up (symb->u.nonterm.first,
                                                      rhs_symb->u.term.term_num);
                    }
                    else
                    {
                        if (first_continue_p)
                            changed_p |= term_set_or (symb->u.nonterm.first,
                                                      rhs_symb->u.nonterm.first);
                        for (k = j + 1; k < rhs_len; k++)
                        {
                            next_rhs_symb = rhs[k];
                            if (next_rhs_symb->term_p)
                                changed_p
                                    |= term_set_up (rhs_symb->u.nonterm.follow,
                                                    next_rhs_symb->u.term.term_num);
                            else
                                changed_p
                                    |= term_set_or (rhs_symb->u.nonterm.follow,
                                                    next_rhs_symb->u.nonterm.first);
                            if (!next_rhs_symb->empty_p)
                                break;
                        }
                        if (k == rhs_len)
                            changed_p |= term_set_or (rhs_symb->u.nonterm.follow,
                                                      symb->u.nonterm.follow);
                    }
                    if (!rhs_symb->empty_p)
                        first_continue_p = FALSE;
                }
            }
    }
    while (changed_p);
}

/* The following function sets up flags empty_p, access_p and
   derivation_p for all grammar symbols. */
static void
set_empty_access_derives (void)
{
    struct symb *symb, *rhs_symb;
    struct rule *rule;
    int empty_p, derivation_p;
    int empty_changed_p, derivation_changed_p, accessibility_change_p;
    int i, j;

    for (i = 0; (symb = symb_get (i)) != NULL; i++)
    {
        symb->empty_p = 0;
        symb->derivation_p = (symb->term_p ? 1 : 0);
        symb->access_p = 0;
    }
    grammar->axiom->access_p = 1;
    do
    {
        empty_changed_p = derivation_changed_p = accessibility_change_p = 0;
        for (i = 0; (symb = nonterm_get (i)) != NULL; i++)
            for (rule = symb->u.nonterm.rules;
                 rule != NULL; rule = rule->lhs_next)
            {
                empty_p = derivation_p = 1;
                for (j = 0; j < rule->rhs_len; j++)
                {
                    rhs_symb = rule->rhs[j];
                    if (symb->access_p)
                    {
                        accessibility_change_p |= rhs_symb->access_p ^ 1;
                        rhs_symb->access_p = 1;
                    }
                    empty_p &= rhs_symb->empty_p;
                    derivation_p &= rhs_symb->derivation_p;
                }
                if (empty_p)
                {
                    empty_changed_p |= symb->empty_p ^ empty_p;
                    symb->empty_p = empty_p;
                }
                if (derivation_p)
                {
                    derivation_changed_p |= symb->derivation_p ^ derivation_p;
                    symb->derivation_p = derivation_p;
                }
            }
    }
    while (empty_changed_p || derivation_changed_p || accessibility_change_p);

}

/* The following function sets up flags loop_p for nonterminals. */
static void
set_loop_p (void)
{
    struct symb *symb, *lhs;
    struct rule *rule;
    int i, j, k, loop_p, changed_p;

    /* Initialize accoding to minimal criteria: There is a rule in which
       the nonterminal stands and all the rest symbols can derive empty
       strings. */
    for (rule = rules_ptr->first_rule; rule != NULL; rule = rule->next)
        for (i = 0; i < rule->rhs_len; i++)
            if (!(symb = rule->rhs[i])->term_p)
            {
                for (j = 0; j < rule->rhs_len; j++)
                    if (i == j)
                        continue;
                    else if (!rule->rhs[j]->empty_p)
                        break;
                if (j >= rule->rhs_len)
                    symb->u.nonterm.loop_p = 1;
            }
    /* Major cycle: Check looped nonterminal that there is a rule with
       the nonterminal in lhs with a looped nonterminal in rhs and all
       the rest rhs symbols deriving empty string. */
    do
    {
        changed_p = FALSE;
        for (i = 0; (lhs = nonterm_get (i)) != NULL; i++)
            if (lhs->u.nonterm.loop_p)
            {
                loop_p = 0;
                for (rule = lhs->u.nonterm.rules;
                     rule != NULL; rule = rule->lhs_next)
                    for (j = 0; j < rule->rhs_len; j++)
                        if (!(symb = rule->rhs[j])->term_p && symb->u.nonterm.loop_p)
                        {
                            for (k = 0; k < rule->rhs_len; k++)
                                if (j == k)
                                    continue;
                                else if (!rule->rhs[k]->empty_p)
                                    break;
                            if (k >= rule->rhs_len)
                                loop_p = 1;
                        }
                if (!loop_p)
                    changed_p = TRUE;
                lhs->u.nonterm.loop_p = loop_p;
            }
    }
    while (changed_p);
}

/* The following function evaluates different sets and flags for
   grammar and checks the grammar on correctness. */
static void
check_grammar (int strict_p)
{
    struct symb *symb;
    int i;

    set_empty_access_derives ();
    set_loop_p ();
    if (strict_p)
    {
        for (i = 0; (symb = nonterm_get (i)) != NULL; i++)
	{
            if (!symb->derivation_p)
                yaep_error
                    (YAEP_NONTERM_DERIVATION,
                     "nonterm `%s' does not derive any term string", symb->repr);
            else if (!symb->access_p)
                yaep_error (YAEP_UNACCESSIBLE_NONTERM,
                            "nonterm `%s' is not accessible from axiom",
                            symb->repr);
	}
    }
    else if (!grammar->axiom->derivation_p)
        yaep_error (YAEP_NONTERM_DERIVATION,
                    "nonterm `%s' does not derive any term string",
                    grammar->axiom->repr);
    for (i = 0; (symb = nonterm_get (i)) != NULL; i++)
        if (symb->u.nonterm.loop_p)
            yaep_error
                (YAEP_LOOP_NONTERM,
                 "nonterm `%s' can derive only itself (grammar with loops)",
                 symb->repr);
    /* We should have correct flags empty_p here. */
    create_first_follow_sets ();
}

/* The following are names of additional symbols.  Don't use them in
   grammars. */
#define AXIOM_NAME "$S"
#define END_MARKER_NAME "$eof"
#define TERM_ERROR_NAME "error"

/* It should be negative. */
#define END_MARKER_CODE -1
#define TERM_ERROR_CODE -2

/* The following function reads terminals/rules.  The function returns
   pointer to the grammar (or NULL if there were errors in
   grammar). */
int
yaep_read_grammar (struct grammar *g, int strict_p,
		   const char *(*read_terminal) (int *code),
		   const char *(*read_rule) (const char ***rhs,
					     const char **abs_node,
					     int *anode_cost, int **transl, char *mark, char **marks))
{
    const char *name, *lhs, **rhs, *anode;
    struct symb *symb, *start;
    struct rule *rule;
    int anode_cost;
    int *transl;
    char mark;
    char *marks;
    int i, el, code;

    assert (g != NULL);
    grammar = g;
    symbs_ptr = g->symbs_ptr;
    term_sets_ptr = g->term_sets_ptr;
    rules_ptr = g->rules_ptr;
    if ((code = setjmp (error_longjump_buff)) != 0)
    {
        return code;
    }
    if (!grammar->undefined_p)
        yaep_empty_grammar ();
    while ((name = (*read_terminal) (&code)) != NULL)
    {
        // fprintf(stderr, "TERM >%s< %d\n", name, code); // DEBUGGING TODO REMOVE
        if (code < 0)
            yaep_error (YAEP_NEGATIVE_TERM_CODE,
                        "term `%s' has negative code", name);
        symb = symb_find_by_repr (name);
        if (symb != NULL)
            yaep_error (YAEP_REPEATED_TERM_DECL,
                        "repeated declaration of term `%s'", name);
        if (symb_find_by_code (code) != NULL)
            yaep_error (YAEP_REPEATED_TERM_CODE,
                        "repeated code %d in term `%s'", code, name);
        symb_add_term (name, code);
    }

    /* Adding error symbol. */
    if (symb_find_by_repr (TERM_ERROR_NAME) != NULL)
        yaep_error (YAEP_FIXED_NAME_USAGE,
                    "do not use fixed name `%s'", TERM_ERROR_NAME);
    if (symb_find_by_code (TERM_ERROR_CODE) != NULL)
        abort ();
    grammar->term_error = symb_add_term (TERM_ERROR_NAME, TERM_ERROR_CODE);
    grammar->term_error_num = grammar->term_error->u.term.term_num;
    grammar->axiom = grammar->end_marker = NULL;
    while ((lhs = (*read_rule) (&rhs, &anode, &anode_cost, &transl, &mark, &marks)) != NULL)
    {
        // fprintf(stderr, "LHS >%s<\n", lhs); // DEBUGGING TODO REMOVE
        symb = symb_find_by_repr (lhs);
        if (symb == NULL)
            symb = symb_add_nonterm (lhs);
        else if (symb->term_p)
            yaep_error (YAEP_TERM_IN_RULE_LHS,
                        "term `%s' in the left hand side of rule", lhs);
        if (anode == NULL && transl != NULL && *transl >= 0 && transl[1] >= 0)
            yaep_error (YAEP_INCORRECT_TRANSLATION,
                        "rule for `%s' has incorrect translation", lhs);
        if (anode != NULL && anode_cost < 0)
            yaep_error (YAEP_NEGATIVE_COST,
                        "translation for `%s' has negative cost", lhs);
        if (grammar->axiom == NULL)
	{
            /* We made this here becuase we want that the start rule has
               number 0. */
            /* Add axiom and end marker. */
            start = symb;
            grammar->axiom = symb_find_by_repr (AXIOM_NAME);
            if (grammar->axiom != NULL)
                yaep_error (YAEP_FIXED_NAME_USAGE,
                            "do not use fixed name `%s'", AXIOM_NAME);
            grammar->axiom = symb_add_nonterm (AXIOM_NAME);
            grammar->end_marker = symb_find_by_repr (END_MARKER_NAME);
            if (grammar->end_marker != NULL)
                yaep_error (YAEP_FIXED_NAME_USAGE,
                            "do not use fixed name `%s'", END_MARKER_NAME);
            if (symb_find_by_code (END_MARKER_CODE) != NULL)
                abort ();
            grammar->end_marker = symb_add_term (END_MARKER_NAME,
                                                 END_MARKER_CODE);
            /* Add rules for start */
            rule = rule_new_start (grammar->axiom, NULL, 0);
            rule_new_symb_add (symb);
            rule_new_symb_add (grammar->end_marker);
            rule_new_stop ();
            rule->order[0] = 0;
            rule->trans_len = 1;
	}
        rule = rule_new_start (symb, anode, (anode != NULL ? anode_cost : 0));
        size_t rhs_len = 0;
        while (*rhs != NULL)
	{
            rhs_len++;
            symb = symb_find_by_repr (*rhs);
            if (symb == NULL)
                symb = symb_add_nonterm (*rhs);
            rule_new_symb_add (symb);
            rhs++;
	}
        rule_new_stop ();
        // IXML
        rule->mark = mark;
        rule->marks = (char*)calloc(rhs_len, sizeof(char));
        memcpy(rule->marks, marks, rhs_len);

        if (transl != NULL)
	{
            for (i = 0; (el = transl[i]) >= 0; i++)
            {
                if (el >= rule->rhs_len)
                {
                    if (el != YAEP_NIL_TRANSLATION_NUMBER)
                        yaep_error
                            (YAEP_INCORRECT_SYMBOL_NUMBER,
                             "translation symbol number %d in rule for `%s' is out of range",
                             el, lhs);
                    else
                        rule->trans_len++;
                }
                else if (rule->order[el] >= 0)
                    yaep_error
                        (YAEP_REPEATED_SYMBOL_NUMBER,
                         "repeated translation symbol number %d in rule for `%s'",
                         el, lhs);
                else
                {
                    rule->order[el] = i;
                    rule->trans_len++;
                }
                assert (i < rule->rhs_len || transl[i] < 0);
            }
	}
    }
    if (grammar->axiom == NULL)
        yaep_error (YAEP_NO_RULES, "grammar does not contains rules");
    assert (start != NULL);
    /* Adding axiom : error $eof if it is neccessary. */
    for (rule = start->u.nonterm.rules; rule != NULL; rule = rule->lhs_next)
        if (rule->rhs[0] == grammar->term_error)
            break;
    if (rule == NULL)
    {
        rule = rule_new_start (grammar->axiom, NULL, 0);
        rule_new_symb_add (grammar->term_error);
        rule_new_symb_add (grammar->end_marker);
        rule_new_stop ();
        rule->trans_len = 0;
    }
    check_grammar (strict_p);
#ifdef SYMB_CODE_TRANS_VECT
    symb_finish_adding_terms ();
#endif
#ifndef NO_YAEP_DEBUG_PRINT
    if (grammar->debug_level > 2)
    {
        /* Print rules. */
        fprintf (stderr, "Rules:\n");
        for (rule = rules_ptr->first_rule; rule != NULL; rule = rule->next)
	{
            fprintf (stderr, "  ");
            rule_print (stderr, rule, TRUE);
	}
        fprintf (stderr, "\n");
        /* Print symbol sets. */
        for (i = 0; (symb = nonterm_get (i)) != NULL; i++)
	{
            fprintf (stderr, "Nonterm %s:  Empty=%s , Access=%s, Derive=%s\n",
                     symb->repr, (symb->empty_p ? "Yes" : "No"),
                     (symb->access_p ? "Yes" : "No"),
                     (symb->derivation_p ? "Yes" : "No"));
            if (grammar->debug_level > 3)
	    {
                fprintf (stderr, "  First: ");
                term_set_print (stderr, symb->u.nonterm.first);
                fprintf (stderr, "\n  Follow: ");
                term_set_print (stderr, symb->u.nonterm.follow);
                fprintf (stderr, "\n\n");
	    }
	}
    }
#endif
    grammar->undefined_p = FALSE;
    return 0;
}

//include "sgramm.c"

/* The following functions set up parameter which affect parser work
   and return the previous parameter value. */

int
yaep_set_lookahead_level (struct grammar *grammar, int level)
{
    int old;

    assert (grammar != NULL);
    old = grammar->lookahead_level;
    grammar->lookahead_level = (level < 0 ? 0 : level > 2 ? 2 : level);
    return old;
}

int
yaep_set_debug_level (struct grammar *grammar, int level)
{
    int old;

    assert (grammar != NULL);
    old = grammar->debug_level;
    grammar->debug_level = level;
    return old;
}

int
yaep_set_one_parse_flag (struct grammar *grammar, int flag)
{
    int old;

    assert (grammar != NULL);
    old = grammar->one_parse_p;
    grammar->one_parse_p = flag;
    return old;
}

int
yaep_set_cost_flag (struct grammar *grammar, int flag)
{
    int old;

    assert (grammar != NULL);
    old = grammar->cost_p;
    grammar->cost_p = flag;
    return old;
}

int
yaep_set_error_recovery_flag (struct grammar *grammar, int flag)
{
    int old;

    assert (grammar != NULL);
    old = grammar->error_recovery_p;
    grammar->error_recovery_p = flag;
    return old;
}

int
yaep_set_recovery_match (struct grammar *grammar, int n_toks)
{
    int old;

    assert (grammar != NULL);
    old = grammar->recovery_token_matches;
    grammar->recovery_token_matches = n_toks;
    return old;
}

/* The function initializes all internal data for parser for N_TOKS
   tokens. */
static void
yaep_parse_init (int n_toks)
{
    struct rule *rule;

    sit_init ();
    set_init (n_toks);
    core_symb_vect_init ();
#ifdef USE_CORE_SYMB_HASH_TABLE
    {
        int i;
        struct symb *symb;

        for (i = 0; (symb = symb_get (i)) != NULL; i++)
            symb->cached_core_symb_vect = NULL;
    }
#endif
    for (rule = rules_ptr->first_rule; rule != NULL; rule = rule->next)
        rule->caller_anode = NULL;
}

/* The function should be called the last (it frees all allocated
   data for parser). */
static void
yaep_parse_fin (void)
{
    core_symb_vect_fin ();
    set_fin ();
    sit_fin ();
}

/* The following function reads all input tokens. */
static void
read_toks (void)
{
    int code;
    void *attr;

    while ((code = read_token (&attr)) >= 0)
        tok_add (code, attr);
    tok_add (END_MARKER_CODE, NULL);
}

/* The following function add start situations which is formed from
   given start situation SIT with distance DIST by reducing symbol
   which can derivate empty string and which is placed after dot in
   given situation.  The function returns TRUE if the dot is placed on
   the last position in given situation or in the added situations. */
#if MAKE_INLINE
INLINE
#endif
static void
add_derived_nonstart_sits (struct sit *sit, int parent)
{
    struct symb *symb;
    struct rule *rule = sit->rule;
    int context = sit->context;
    int i;

    for (i = sit->pos; (symb = rule->rhs[i]) != NULL && symb->empty_p; i++)
        set_add_new_nonstart_sit (sit_create (rule, i + 1, context), parent);
}

#ifdef TRANSITIVE_TRANSITION
/* Collect all symbols before the dot in new situations in
   CORE_SYMBOL_VLO.  */
static void
collect_core_symbols (void)
{
    int i;
    int *core_symbol_check_vec = (int *) VLO_BEGIN (core_symbol_check_vlo);
    struct symb *symb;
    struct sit *sit;

    for (i = 0; i < new_core->n_sits; i++)
    {
        sit = new_sits[i];
        if (sit->pos >= sit->rule->rhs_len)
            continue;
        /* There is a symbol after dot in the situation. */
        symb = sit->rule->rhs[sit->pos];
        if (core_symbol_check_vec[symb->num] == core_symbol_check
            || symb == grammar->term_error)
            continue;
        core_symbol_check_vec[symb->num] = core_symbol_check;
        VLO_ADD_MEMORY (core_symbols_vlo, &symb, sizeof (symb));
    }
}

/* Create transitive transition vectors for the new situations.
   Transition vectors should be already created.  */
static void
form_transitive_transition_vectors (void)
{
    int i, j, k, sit_ind;
    struct symb *symb, *new_symb;
    struct sit *sit;
    struct core_symb_vect *core_symb_vect, *symb_core_symb_vect;

    core_symbol_check++;
    expand_int_vlo (&core_symbol_check_vlo,
                    symbs_ptr->n_terms + symbs_ptr->n_nonterms);
    VLO_NULLIFY (core_symbols_vlo);
    collect_core_symbols ();
    for (i = 0; i < VLO_LENGTH (core_symbols_vlo) / sizeof (struct symb *); i++)
    {
        symb = ((struct symb **) VLO_BEGIN (core_symbols_vlo))[i];
        core_symb_vect = core_symb_vect_find (new_core, symb);
        if (core_symb_vect == NULL)
            core_symb_vect = core_symb_vect_new (new_core, symb);
        core_symbol_check++;
        VLO_NULLIFY (core_symbol_queue_vlo);
        /* Put the symbol into the queue.  */
        VLO_ADD_MEMORY (core_symbol_queue_vlo, &symb, sizeof (symb));
        for (j = 0;
             j < VLO_LENGTH (core_symbol_queue_vlo) / sizeof (struct symb *);
             j++)
	{
            symb = ((struct symb **) VLO_BEGIN (core_symbol_queue_vlo))[j];
            symb_core_symb_vect = core_symb_vect_find (new_core, symb);
            if (symb_core_symb_vect == NULL)
                continue;
            for (k = 0; k < symb_core_symb_vect->transitions.len; k++)
	    {
                sit_ind = symb_core_symb_vect->transitions.els[k];
                core_symb_vect_new_add_transitive_transition_el (core_symb_vect,
                                                                 sit_ind);
                if (sit_ind < new_core->n_all_dists)
                    /* This situation is originated from other sets --
                       stop.  */
                    continue;
                sit = new_sits[sit_ind];
                sit = sit_create (sit->rule, sit->pos + 1, sit->context);
                if (sit->empty_tail_p)
		{
                    new_symb = sit->rule->lhs;
                    if (((int *) VLO_BEGIN (core_symbol_check_vlo))[new_symb->
                                                                    num] !=
                        core_symbol_check)
		    {
                        /* Put the LHS symbol into queue.  */
                        VLO_ADD_MEMORY (core_symbol_queue_vlo,
                                        &new_symb, sizeof (new_symb));
                        ((int *) VLO_BEGIN (core_symbol_check_vlo))[new_symb->
                                                                    num] =
                            core_symbol_check;
		    }
		}
	    }
	}
    }
}
#endif

/* The following function adds the rest (non-start) situations to the
   new set and and forms triples (set core, symbol, indexes) for
   further fast search of start situations from given core by
   transition on given symbol (see comment for abstract data
   `core_symb_vect'). */
#if MAKE_INLINE
INLINE
#endif
static void
expand_new_start_set (void)
{
    struct sit *sit;
    struct symb *symb;
    struct core_symb_vect *core_symb_vect;
    struct rule *rule;
    int i;

    /* Add non start situations with nonzero distances. */
    for (i = 0; i < new_n_start_sits; i++)
        add_derived_nonstart_sits (new_sits[i], i);
    /* Add non start situations and form transitions vectors. */
    for (i = 0; i < new_core->n_sits; i++)
    {
        sit = new_sits[i];
        if (sit->pos < sit->rule->rhs_len)
	{
            /* There is a symbol after dot in the situation. */
            symb = sit->rule->rhs[sit->pos];
            core_symb_vect = core_symb_vect_find (new_core, symb);
            if (core_symb_vect == NULL)
	    {
                core_symb_vect = core_symb_vect_new (new_core, symb);
                if (!symb->term_p)
                    for (rule = symb->u.nonterm.rules;
                         rule != NULL; rule = rule->lhs_next)
                        set_new_add_initial_sit (sit_create (rule, 0, 0));
	    }
            core_symb_vect_new_add_transition_el (core_symb_vect, i);
            if (symb->empty_p && i >= new_core->n_all_dists)
                set_new_add_initial_sit (sit_create (sit->rule, sit->pos + 1, 0));
	}
    }
    /* Now forming reduce vectors. */
    for (i = 0; i < new_core->n_sits; i++)
    {
        sit = new_sits[i];
        if (sit->pos == sit->rule->rhs_len)
	{
            symb = sit->rule->lhs;
            core_symb_vect = core_symb_vect_find (new_core, symb);
            if (core_symb_vect == NULL)
                core_symb_vect = core_symb_vect_new (new_core, symb);
            core_symb_vect_new_add_reduce_el (core_symb_vect, i);
	}
    }
#ifdef TRANSITIVE_TRANSITION
    form_transitive_transition_vectors ();
#endif
    if (grammar->lookahead_level > 1)
    {
        struct sit *new_sit, *shifted_sit;
        term_set_el_t *context_set;
        int changed_p, sit_ind, context, j;

        /* Now we have incorrect initial situations because their
           context is not correct. */
        context_set = term_set_create ();
        do
	{
            changed_p = FALSE;
            for (i = new_core->n_all_dists; i < new_core->n_sits; i++)
	    {
                term_set_clear (context_set);
                new_sit = new_sits[i];
                core_symb_vect = core_symb_vect_find (new_core,
                                                      new_sit->rule->lhs);
                for (j = 0; j < core_symb_vect->transitions.len; j++)
		{
                    sit_ind = core_symb_vect->transitions.els[j];
                    sit = new_sits[sit_ind];
                    shifted_sit = sit_create (sit->rule, sit->pos + 1,
                                              sit->context);
                    term_set_or (context_set, shifted_sit->lookahead);
		}
                context = term_set_insert (context_set);
                if (context >= 0)
                    context_set = term_set_create ();
                else
                    context = -context - 1;
                sit = sit_create (new_sit->rule, new_sit->pos, context);
                if (sit != new_sit)
		{
                    new_sits[i] = sit;
                    changed_p = TRUE;
		}
	    }
	}
        while (changed_p);
    }
    set_new_core_stop ();
    core_symb_vect_new_all_stop ();
}

/* The following function forms the 1st set. */
static void
build_start_set (void)
{
    struct rule *rule;
    struct sit *sit;
    term_set_el_t *context_set;
    int context;

    set_new_start ();
    if (grammar->lookahead_level <= 1)
        context = 0;
    else
    {
        context_set = term_set_create ();
        term_set_clear (context_set);
        context = term_set_insert (context_set);
        /* Empty context in the table has always number zero. */
        assert (context == 0);
    }
    for (rule = grammar->axiom->u.nonterm.rules;
         rule != NULL; rule = rule->lhs_next)
    {
        sit = sit_create (rule, 0, context);
        set_new_add_start_sit (sit, 0);
    }
    if (!set_insert ())
        assert (FALSE);
    expand_new_start_set ();
    pl[0] = new_set;
#ifndef NO_YAEP_DEBUG_PRINT
    if (grammar->debug_level > 2)
    {
        fprintf (stderr, "\nParsing start...\n");
        if (grammar->debug_level > 3)
            set_print (stderr, new_set, 0, grammar->debug_level > 4,
                       grammar->debug_level > 5);
    }
#endif
}

/* The following function builds new set by shifting situations of SET
   given in CORE_SYMB_VECT with given lookahead terminal number.  If
   the number is negative, we ignore lookahead at all. */
static void
build_new_set (struct set *set, struct core_symb_vect *core_symb_vect,
	       int lookahead_term_num)
{
    struct set *prev_set;
    struct set_core *set_core, *prev_set_core;
    struct sit *sit, *new_sit, **prev_sits;
    struct core_symb_vect *prev_core_symb_vect;
    int local_lookahead_level, dist, sit_ind, new_dist;
    int i, place;
    struct vect *transitions;

    local_lookahead_level = (lookahead_term_num < 0
                             ? 0 : grammar->lookahead_level);
    set_core = set->core;
    set_new_start ();
#ifdef TRANSITIVE_TRANSITION
    curr_sit_check++;
    transitions = &core_symb_vect->transitive_transitions;
#else
    transitions = &core_symb_vect->transitions;
#endif
    empty_sit_dist_set ();
    for (i = 0; i < transitions->len; i++)
    {
        sit_ind = transitions->els[i];
        sit = set_core->sits[sit_ind];
        new_sit = sit_create (sit->rule, sit->pos + 1, sit->context);
        if (local_lookahead_level != 0
            && !term_set_test (new_sit->lookahead, lookahead_term_num)
            && !term_set_test (new_sit->lookahead, grammar->term_error_num))
            continue;
#ifndef ABSOLUTE_DISTANCES
        dist = 0;
#else
        dist = pl_curr;
#endif
        if (sit_ind >= set_core->n_all_dists)
#ifdef TRANSITIVE_TRANSITION
            new_sit->sit_check = curr_sit_check;
#else
	;
#endif
        else if (sit_ind < set_core->n_start_sits)
            dist = set->dists[sit_ind];
        else
            dist = set->dists[set_core->parent_indexes[sit_ind]];
#ifndef ABSOLUTE_DISTANCES
        dist++;
#endif
        if (sit_dist_insert (new_sit, dist))
            set_new_add_start_sit (new_sit, dist);
    }
    for (i = 0; i < new_n_start_sits; i++)
    {
        new_sit = new_sits[i];
        if (new_sit->empty_tail_p
#ifdef TRANSITIVE_TRANSITION
            && new_sit->sit_check != curr_sit_check
#endif
            )
	{
            int *curr_el, *bound;

            /* All tail in new sitiation may derivate empty string so
               make reduce and add new situations. */
            new_dist = new_dists[i];
#ifndef ABSOLUTE_DISTANCES
            place = pl_curr + 1 - new_dist;
#else
            place = new_dist;
#endif
            prev_set = pl[place];
            prev_set_core = prev_set->core;
            prev_core_symb_vect = core_symb_vect_find (prev_set_core,
                                                       new_sit->rule->lhs);
            if (prev_core_symb_vect == NULL)
	    {
                assert (new_sit->rule->lhs == grammar->axiom);
                continue;
	    }
#ifdef TRANSITIVE_TRANSITION
            curr_el = prev_core_symb_vect->transitive_transitions.els;
            bound = curr_el + prev_core_symb_vect->transitive_transitions.len;
#else
            curr_el = prev_core_symb_vect->transitions.els;
            bound = curr_el + prev_core_symb_vect->transitions.len;
#endif
            assert (curr_el != NULL);
            prev_sits = prev_set_core->sits;
            do
	    {
                sit_ind = *curr_el++;
                sit = prev_sits[sit_ind];
                new_sit = sit_create (sit->rule, sit->pos + 1, sit->context);
                if (local_lookahead_level != 0
                    && !term_set_test (new_sit->lookahead, lookahead_term_num)
                    && !term_set_test (new_sit->lookahead,
                                       grammar->term_error_num))
                    continue;
#ifndef ABSOLUTE_DISTANCES
                dist = 0;
#else
                dist = new_dist;
#endif
                if (sit_ind >= prev_set_core->n_all_dists)
#ifdef TRANSITIVE_TRANSITION
                    new_sit->sit_check = curr_sit_check;
#else
		;
#endif
                else if (sit_ind < prev_set_core->n_start_sits)
                    dist = prev_set->dists[sit_ind];
                else
                    dist =
                        prev_set->dists[prev_set_core->parent_indexes[sit_ind]];
#ifndef ABSOLUTE_DISTANCES
                dist += new_dist;
#endif
                if (sit_dist_insert (new_sit, dist))
                    set_new_add_start_sit (new_sit, dist);
	    }
            while (curr_el < bound);
	}
    }
    if (set_insert ())
    {
        expand_new_start_set ();
        new_core->term = core_symb_vect->symb;
    }
}



/* This page contains error recovery code.  This code finds minimal
   cost error recovery.  The cost of error recovery is number of
   tokens ignored by error recovery.  The error recovery is successful
   when we match at least RECOVERY_TOKEN_MATCHES tokens. */

/* The following strucrture describes an error recovery state (an
   error recovery alternative. */
struct recovery_state
{
    /* The following three members define start pl used to given error
       recovery state (alternative). */
    /* The following members define what part of original (at the error
       recovery start) pl will be head of error recovery state.  The
       head will be all states from original pl with indexes in range
       [0, last_original_pl_el]. */
    int last_original_pl_el;
    /* The following two members define tail of pl for this error
       recovery state. */
    int pl_tail_length;
    struct set **pl_tail;
    /* The following member is index of start token for given error
       recovery state. */
    int start_tok;
    /* The following member value is number of tokens already ignored in
       order to achieved given error recovery state. */
    int backward_move_cost;
};

/* All tail sets of error recovery are saved in the following os. */
static os_t recovery_state_tail_sets;

/* The following variable values is pl_curr and tok_curr at error
   recovery start (when the original syntax error has been fixed). */
static int start_pl_curr, start_tok_curr;

/* The following variable value means that all error sets in pl with
   indexes [back_pl_frontier, start_pl_curr] are being processed or
   have been processed. */
static int back_pl_frontier;

/* The following variable stores original pl tail in reversed order.
   This object only grows.  The last object sets may be used to
   restore original pl in order to try another error recovery state
   (alternative). */
static vlo_t original_pl_tail_stack;

/* The following variable value is last pl element which is original
   set (set before the error_recovery start). */
static int original_last_pl_el;

/* The following function may be called if you know that pl has
   original sets upto LAST element (including it).  Such call can
   decrease number of restored sets. */
#if MAKE_INLINE
INLINE
#endif
static void
set_original_set_bound (int last)
{
    assert (last >= 0 && last <= start_pl_curr
            && original_last_pl_el <= start_pl_curr);
    original_last_pl_el = last;
}

/* The following function guarantees that original pl tail sets
   starting with pl_curr (including the state) is saved.  The function
   should be called after any decreasing pl_curr with subsequent
   writing to pl [pl_curr]. */
static void
save_original_sets (void)
{
    int length, curr_pl;

    assert (pl_curr >= 0 && original_last_pl_el <= start_pl_curr);
    length = VLO_LENGTH (original_pl_tail_stack) / sizeof (struct set *);
    for (curr_pl = start_pl_curr - length; curr_pl >= pl_curr; curr_pl--)
    {
        VLO_ADD_MEMORY (original_pl_tail_stack, &pl[curr_pl],
                        sizeof (struct set *));
#ifndef NO_YAEP_DEBUG_PRINT
        if (grammar->debug_level > 2)
	{
            fprintf (stderr, "++++Save original set=%d\n", curr_pl);
            if (grammar->debug_level > 3)
	    {
                set_print (stderr, pl[curr_pl], curr_pl,
                           grammar->debug_level > 4, grammar->debug_level > 5);
                fprintf (stderr, "\n");
	    }
	}
#endif
    }
    original_last_pl_el = pl_curr - 1;
}

/* If it is necessary, the following function restores original pl
   part with states in range [0, last_pl_el]. */
static void
restore_original_sets (int last_pl_el)
{
    assert (last_pl_el <= start_pl_curr
            && original_last_pl_el <= start_pl_curr);
    if (original_last_pl_el >= last_pl_el)
    {
        original_last_pl_el = last_pl_el;
        return;
    }
    for (;;)
    {
        original_last_pl_el++;
        pl[original_last_pl_el]
            = ((struct set **) VLO_BEGIN (original_pl_tail_stack))
            [start_pl_curr - original_last_pl_el];
#ifndef NO_YAEP_DEBUG_PRINT
        if (grammar->debug_level > 2)
	{
            fprintf (stderr, "++++++Restore original set=%d\n",
                     original_last_pl_el);
            if (grammar->debug_level > 3)
	    {
                set_print (stderr, pl[original_last_pl_el], original_last_pl_el,
                           grammar->debug_level > 4, grammar->debug_level > 5);
                fprintf (stderr, "\n");
	    }
	}
#endif
        if (original_last_pl_el >= last_pl_el)
            break;
    }
}

/* The following function looking backward in pl starting with element
   START_PL_EL and returns pl element which refers set with situation
   containing `. error'.  START_PL_EL should be non negative.
   Remember that zero pl set contains `.error' because we added such
   rule if it is necessary.  The function returns number of terminals
   (not taking error into account) on path (result, start_pl_set]. */
#if MAKE_INLINE
INLINE
#endif
static int
find_error_pl_set (int start_pl_set, int *cost)
{
    int curr_pl;

    assert (start_pl_set >= 0);
    *cost = 0;
    for (curr_pl = start_pl_set; curr_pl >= 0; curr_pl--)
        if (core_symb_vect_find (pl[curr_pl]->core, grammar->term_error) != NULL)
            break;
        else if (pl[curr_pl]->core->term != grammar->term_error)
            (*cost)++;
    assert (curr_pl >= 0);
    return curr_pl;
}

/* The following function creates and returns new error recovery state
   with charcteristics (LAST_ORIGINAL_PL_EL, BACKWARD_MOVE_COST,
   pl_curr, tok_curr). */
static struct recovery_state
new_recovery_state (int last_original_pl_el, int backward_move_cost)
{
    struct recovery_state state;
    int i;

    assert (backward_move_cost >= 0);
#ifndef NO_YAEP_DEBUG_PRINT
    if (grammar->debug_level > 2)
    {
        fprintf (stderr,
                 "++++Creating recovery state: original set=%d, tok=%d, ",
                 last_original_pl_el, tok_curr);
        symb_print (stderr, toks[tok_curr].symb, TRUE);
        fprintf (stderr, "\n");
    }
#endif
    state.last_original_pl_el = last_original_pl_el;
    state.pl_tail_length = pl_curr - last_original_pl_el;
    assert (state.pl_tail_length >= 0);
    for (i = last_original_pl_el + 1; i <= pl_curr; i++)
    {
        OS_TOP_ADD_MEMORY (recovery_state_tail_sets, &pl[i], sizeof (pl[i]));
#ifndef NO_YAEP_DEBUG_PRINT
        if (grammar->debug_level > 3)
	{
            fprintf (stderr, "++++++Saving set=%d\n", i);
            set_print (stderr, pl[i], i, grammar->debug_level > 4,
                       grammar->debug_level > 5);
            fprintf (stderr, "\n");
	}
#endif
    }
    state.pl_tail = (struct set **) OS_TOP_BEGIN (recovery_state_tail_sets);
    OS_TOP_FINISH (recovery_state_tail_sets);
    state.start_tok = tok_curr;
    state.backward_move_cost = backward_move_cost;
    return state;
}

/* The following vlo is error recovery states stack.  The stack
   contains error recovery state which should be investigated to find
   the best error recovery. */
static vlo_t recovery_state_stack;

/* The following function creates new error recovery state and pushes
   it on the states stack top. */
#if MAKE_INLINE
INLINE
#endif
static void
push_recovery_state (int last_original_pl_el, int backward_move_cost)
{
    struct recovery_state state;

    state = new_recovery_state (last_original_pl_el, backward_move_cost);
#ifndef NO_YAEP_DEBUG_PRINT
    if (grammar->debug_level > 2)
    {
        fprintf (stderr, "++++Push recovery state: original set=%d, tok=%d, ",
                 last_original_pl_el, tok_curr);
        symb_print (stderr, toks[tok_curr].symb, TRUE);
        fprintf (stderr, "\n");
    }
#endif
    VLO_ADD_MEMORY (recovery_state_stack, &state, sizeof (state));
}

/* The following function sets up parser state (pl, pl_curr, tok_curr)
   according to error recovery STATE. */
static void
set_recovery_state (struct recovery_state *state)
{
    int i;

    tok_curr = state->start_tok;
    restore_original_sets (state->last_original_pl_el);
    pl_curr = state->last_original_pl_el;
#ifndef NO_YAEP_DEBUG_PRINT
    if (grammar->debug_level > 2)
    {
        fprintf (stderr, "++++Set recovery state: set=%d, tok=%d, ",
                 pl_curr, tok_curr);
        symb_print (stderr, toks[tok_curr].symb, TRUE);
        fprintf (stderr, "\n");
    }
#endif
    for (i = 0; i < state->pl_tail_length; i++)
    {
        pl[++pl_curr] = state->pl_tail[i];
#ifndef NO_YAEP_DEBUG_PRINT
        if (grammar->debug_level > 3)
	{
            fprintf (stderr, "++++++Add saved set=%d\n", pl_curr);
            set_print (stderr, pl[pl_curr], pl_curr, grammar->debug_level > 4,
                       grammar->debug_level > 5);
            fprintf (stderr, "\n");
	}
#endif
    }
}

/* The following function pops the top error recovery state from
   states stack.  The current parser state will be setup according to
   the state. */
#if MAKE_INLINE
INLINE
#endif
static struct recovery_state
pop_recovery_state (void)
{
    struct recovery_state *state;

    state = &((struct recovery_state *) VLO_BOUND (recovery_state_stack))[-1];
    VLO_SHORTEN (recovery_state_stack, sizeof (struct recovery_state));
#ifndef NO_YAEP_DEBUG_PRINT
    if (grammar->debug_level > 2)
        fprintf (stderr, "++++Pop error recovery state\n");
#endif
    set_recovery_state (state);
    return *state;
}

/* The following function is major function of syntax error recovery.
   It searches for minimal cost error recovery.  The function returns
   in the parameter number of start token which is ignored and number
   of the first token which is not ignored.  If the number of ignored
   tokens is zero, *START will be equal to *STOP and number of token
   on which the error occurred. */
static void
error_recovery (int *start, int *stop)
{
    struct set *set;
    struct core_symb_vect *core_symb_vect;
    struct recovery_state best_state, state;
    int best_cost, cost, lookahead_term_num, n_matched_toks;
    int back_to_frontier_move_cost, backward_move_cost;

#ifndef NO_YAEP_DEBUG_PRINT
    if (grammar->debug_level > 2)
        fprintf (stderr, "\n++Error recovery start\n");
#endif
    *stop = *start = -1;
    OS_CREATE (recovery_state_tail_sets, grammar->alloc, 0);
    VLO_NULLIFY (original_pl_tail_stack);
    VLO_NULLIFY (recovery_state_stack);
    start_pl_curr = pl_curr;
    start_tok_curr = tok_curr;
    /* Initialize error recovery state stack. */
    pl_curr
        = back_pl_frontier = find_error_pl_set (pl_curr, &backward_move_cost);
    back_to_frontier_move_cost = backward_move_cost;
    save_original_sets ();
    push_recovery_state (back_pl_frontier, backward_move_cost);
    best_cost = 2 * toks_len;
    while (VLO_LENGTH (recovery_state_stack) > 0)
    {
        state = pop_recovery_state ();
        cost = state.backward_move_cost;
        assert (cost >= 0);
        /* Advance back frontier. */
        if (back_pl_frontier > 0)
	{
            int saved_pl_curr = pl_curr, saved_tok_curr = tok_curr;

            /* Advance back frontier. */
            pl_curr = find_error_pl_set (back_pl_frontier - 1,
                                         &backward_move_cost);
#ifndef NO_YAEP_DEBUG_PRINT
            if (grammar->debug_level > 2)
                fprintf (stderr, "++++Advance back frontier: old=%d, new=%d\n",
                         back_pl_frontier, pl_curr);
#endif
            if (best_cost >= back_to_frontier_move_cost + backward_move_cost)
	    {
                back_pl_frontier = pl_curr;
                tok_curr = start_tok_curr;
                save_original_sets ();
                back_to_frontier_move_cost += backward_move_cost;
                push_recovery_state (back_pl_frontier,
                                     back_to_frontier_move_cost);
                set_original_set_bound (state.last_original_pl_el);
                tok_curr = saved_tok_curr;
	    }
            pl_curr = saved_pl_curr;
	}
        /* Advance head frontier. */
        if (best_cost >= cost + 1)
	{
            tok_curr++;
            if (tok_curr < toks_len)
	    {
#ifndef NO_YAEP_DEBUG_PRINT
                if (grammar->debug_level > 2)
		{
                    fprintf (stderr,
                             "++++Advance head frontier (one pos): tok=%d, ",
                             tok_curr);
                    symb_print (stderr, toks[tok_curr].symb, TRUE);
                    fprintf (stderr, "\n");
#endif
		}
                push_recovery_state (state.last_original_pl_el, cost + 1);
	    }
            tok_curr--;
	}
        set = pl[pl_curr];
#ifndef NO_YAEP_DEBUG_PRINT
        if (grammar->debug_level > 2)
	{
            fprintf (stderr, "++++Trying set=%d, tok=%d, ", pl_curr, tok_curr);
            symb_print (stderr, toks[tok_curr].symb, TRUE);
            fprintf (stderr, "\n");
	}
#endif
        /* Shift error: */
        core_symb_vect = core_symb_vect_find (set->core, grammar->term_error);
        assert (core_symb_vect != NULL);
#ifndef NO_YAEP_DEBUG_PRINT
        if (grammar->debug_level > 2)
            fprintf (stderr, "++++Making error shift in set=%d\n", pl_curr);
#endif
        build_new_set (set, core_symb_vect, -1);
        pl[++pl_curr] = new_set;
#ifndef NO_YAEP_DEBUG_PRINT
        if (grammar->debug_level > 2)
	{
            fprintf (stderr, "++Trying new set=%d\n", pl_curr);
            if (grammar->debug_level > 3)
	    {
                set_print (stderr, new_set, pl_curr, grammar->debug_level > 4,
                           grammar->debug_level > 5);
                fprintf (stderr, "\n");
	    }
	}
#endif
        /* Search the first right token. */
        while (tok_curr < toks_len)
	{
            core_symb_vect = core_symb_vect_find (new_core,
                                                  toks[tok_curr].symb);
            if (core_symb_vect != NULL)
                break;
#ifndef NO_YAEP_DEBUG_PRINT
            if (grammar->debug_level > 2)
	    {
                fprintf (stderr, "++++++Skipping=%d ", tok_curr);
                symb_print (stderr, toks[tok_curr].symb, TRUE);
                fprintf (stderr, "\n");
	    }
#endif
            cost++;
            tok_curr++;
            if (cost >= best_cost)
                /* This state is worse.  Reject it. */
                break;
	}
        if (cost >= best_cost)
	{
#ifndef NO_YAEP_DEBUG_PRINT
            if (grammar->debug_level > 2)
                fprintf
                    (stderr,
                     "++++Too many ignored tokens %d (already worse recovery)\n",
                     cost);
#endif
            /* This state is worse.  Reject it. */
            continue;
	}
        if (tok_curr >= toks_len)
	{
#ifndef NO_YAEP_DEBUG_PRINT
            if (grammar->debug_level > 2)
                fprintf
                    (stderr,
                     "++++We achieved EOF without matching -- reject this state\n");
#endif
            /* Go to the next recovery state.  To guarantee that pl does
               not grows to much we don't push secondary error recovery
               states without matching in primary error recovery state.
               So we can say that pl length at most twice length of
               tokens array. */
            continue;
	}
        /* Shift the found token. */
        lookahead_term_num = (tok_curr + 1 < toks_len
                              ? toks[tok_curr + 1].symb->u.term.term_num : -1);
        build_new_set (new_set, core_symb_vect, lookahead_term_num);
        pl[++pl_curr] = new_set;
#ifndef NO_YAEP_DEBUG_PRINT
        if (grammar->debug_level > 3)
	{
            fprintf (stderr, "++++++++Building new set=%d\n", pl_curr);
            if (grammar->debug_level > 3)
                set_print (stderr, new_set, pl_curr, grammar->debug_level > 4,
                           grammar->debug_level > 5);
	}
#endif
        n_matched_toks = 0;
        for (;;)
	{
#ifndef NO_YAEP_DEBUG_PRINT
            if (grammar->debug_level > 2)
	    {
                fprintf (stderr, "++++++Matching=%d ", tok_curr);
                symb_print (stderr, toks[tok_curr].symb, TRUE);
                fprintf (stderr, "\n");
	    }
#endif
            n_matched_toks++;
            if (n_matched_toks >= grammar->recovery_token_matches)
                break;
            tok_curr++;
            if (tok_curr >= toks_len)
                break;
            /* Push secondary recovery state (with error in set). */
            if (core_symb_vect_find (new_core, grammar->term_error) != NULL)
	    {
#ifndef NO_YAEP_DEBUG_PRINT
                if (grammar->debug_level > 2)
		{
                    fprintf
                        (stderr,
                         "++++Found secondary state: original set=%d, tok=%d, ",
                         state.last_original_pl_el, tok_curr);
                    symb_print (stderr, toks[tok_curr].symb, TRUE);
                    fprintf (stderr, "\n");
		}
#endif
                push_recovery_state (state.last_original_pl_el, cost);
	    }
            core_symb_vect
                = core_symb_vect_find (new_core, toks[tok_curr].symb);
            if (core_symb_vect == NULL)
                break;
            lookahead_term_num = (tok_curr + 1 < toks_len
                                  ? toks[tok_curr + 1].symb->u.term.term_num
                                  : -1);
            build_new_set (new_set, core_symb_vect, lookahead_term_num);
            pl[++pl_curr] = new_set;
	}
        if (n_matched_toks >= grammar->recovery_token_matches
            || tok_curr >= toks_len)
	{
            /* We found an error recovery.  Compare costs. */
            if (best_cost > cost)
	    {
#ifndef NO_YAEP_DEBUG_PRINT
                if (grammar->debug_level > 2)
                    fprintf
                        (stderr,
                         "++++Ignore %d tokens (the best recovery now): Save it:\n",
                         cost);
#endif
                best_cost = cost;
                if (tok_curr == toks_len)
                    tok_curr--;
                best_state = new_recovery_state (state.last_original_pl_el,
                                                 /* It may be any constant here
                                                    because it is not used. */
                                                 0);
                *start = start_tok_curr - state.backward_move_cost;
                *stop = *start + cost;
	    }
#ifndef NO_YAEP_DEBUG_PRINT
            else if (grammar->debug_level > 2)
                fprintf (stderr, "++++Ignore %d tokens (worse recovery)\n", cost);
#endif
	}
#ifndef NO_YAEP_DEBUG_PRINT
        else if (cost < best_cost && grammar->debug_level > 2)
            fprintf (stderr, "++++No %d matched tokens  -- reject this state\n",
                     grammar->recovery_token_matches);
#endif
    }
#ifndef NO_YAEP_DEBUG_PRINT
    if (grammar->debug_level > 2)
        fprintf (stderr, "\n++Finishing error recovery: Restore best state\n");
#endif
    set_recovery_state (&best_state);
#ifndef NO_YAEP_DEBUG_PRINT
    if (grammar->debug_level > 2)
    {
        fprintf (stderr, "\n++Error recovery end: curr token %d=", tok_curr);
        symb_print (stderr, toks[tok_curr].symb, TRUE);
        fprintf (stderr, ", Current set=%d:\n", pl_curr);
        if (grammar->debug_level > 3)
            set_print (stderr, pl[pl_curr], pl_curr, grammar->debug_level > 4,
                       grammar->debug_level > 5);
    }
#endif
    OS_DELETE (recovery_state_tail_sets);
}

/* Initialize work with error recovery. */
static void
error_recovery_init (void)
{
    VLO_CREATE (original_pl_tail_stack, grammar->alloc, 4096);
    VLO_CREATE (recovery_state_stack, grammar->alloc, 4096);
}

/* Finalize work with error recovery. */
static void
error_recovery_fin (void)
{
    VLO_DELETE (recovery_state_stack);
    VLO_DELETE (original_pl_tail_stack);
}




#if MAKE_INLINE
INLINE
#endif
/* Return TRUE if goto set SET from parsing list PLACE can be used as
   the next set.  The criterium is that all origin sets of start
   situations are the same as from PLACE.  */
static int
check_cached_transition_set (struct set *set, int place)
{
    int i, dist;
    int *dists = set->dists;

    for (i = set->core->n_start_sits - 1; i >= 0; i--)
    {
        if ((dist = dists[i]) <= 1)
            continue;
        /* Sets at origins of situations with distance one are supposed
           to be the same.  */
        if (pl[pl_curr + 1 - dist] != pl[place + 1 - dist])
            return FALSE;
    }
    return TRUE;
}

/* How many times we reuse Earley's sets without their
   recalculation.  */
static int n_goto_successes;

/* The following function is major function forming parsing list in
   Earley's algorithm. */
static void
build_pl (void)
{
    int i;
    struct symb *term;
    struct set *set;
    struct core_symb_vect *core_symb_vect;
    int lookahead_term_num;
#ifdef USE_SET_HASH_TABLE
    hash_table_entry_t *entry;
    struct set_term_lookahead *new_set_term_lookahead;
#endif

    error_recovery_init ();
    build_start_set ();
    lookahead_term_num = -1;
    for (tok_curr = pl_curr = 0; tok_curr < toks_len; tok_curr++)
    {
        term = toks[tok_curr].symb;
        if (grammar->lookahead_level != 0)
            lookahead_term_num = (tok_curr < toks_len - 1
                                  ? toks[tok_curr +
                                         1].symb->u.term.term_num : -1);

#ifndef NO_YAEP_DEBUG_PRINT
        if (grammar->debug_level > 2)
	{
            fprintf (stderr, "\nReading %d=", tok_curr);
            symb_print (stderr, term, TRUE);
            fprintf (stderr, ", Current set=%d\n", pl_curr);
	}
#endif
        set = pl[pl_curr];
        new_set = NULL;
#ifdef USE_SET_HASH_TABLE
        OS_TOP_EXPAND (set_term_lookahead_os,
                       sizeof (struct set_term_lookahead));
        new_set_term_lookahead =
            (struct set_term_lookahead *) OS_TOP_BEGIN (set_term_lookahead_os);
        new_set_term_lookahead->set = set;
        new_set_term_lookahead->term = term;
        new_set_term_lookahead->lookahead = lookahead_term_num;
        for (i = 0; i < MAX_CACHED_GOTO_RESULTS; i++)
            new_set_term_lookahead->result[i] = NULL;
        new_set_term_lookahead->curr = 0;
        entry =
            find_hash_table_entry (set_term_lookahead_tab, new_set_term_lookahead,
                                   TRUE);
        if (*entry != NULL)
	{
            struct set *tab_set;

            OS_TOP_NULLIFY (set_term_lookahead_os);
            for (i = 0; i < MAX_CACHED_GOTO_RESULTS; i++)
                if ((tab_set =
                     ((struct set_term_lookahead *) *entry)->result[i]) == NULL)
                    break;
                else if (check_cached_transition_set
                         (tab_set,
                          ((struct set_term_lookahead *) *entry)->place[i]))
                {
                    new_set = tab_set;
                    n_goto_successes++;
                    break;
                }
	}
        else
	{
            OS_TOP_FINISH (set_term_lookahead_os);
            *entry = (hash_table_entry_t) new_set_term_lookahead;
            n_set_term_lookaheads++;
	}

#endif
        if (new_set == NULL)
	{
            core_symb_vect = core_symb_vect_find (set->core, term);
            if (core_symb_vect == NULL)
	    {
                int saved_tok_curr, start, stop;

                /* Error recovery.  We do not check transition vector
                   because for terminal transition vector is never NULL
                   and reduce is always NULL. */
                saved_tok_curr = tok_curr;
                if (grammar->error_recovery_p)
		{
                    error_recovery (&start, &stop);
                    syntax_error (saved_tok_curr, toks[saved_tok_curr].attr,
                                  start, toks[start].attr, stop,
                                  toks[stop].attr);
                    continue;
		}
                else
		{
                    syntax_error (saved_tok_curr, toks[saved_tok_curr].attr,
                                  -1, NULL, -1, NULL);
                    break;
		}
	    }
            build_new_set (set, core_symb_vect, lookahead_term_num);
#ifdef USE_SET_HASH_TABLE
            /* Save (set, term, lookahead) -> new_set in the table.  */
            i = ((struct set_term_lookahead *) *entry)->curr;
            ((struct set_term_lookahead *) *entry)->result[i] = new_set;
            ((struct set_term_lookahead *) *entry)->place[i] = pl_curr;
            ((struct set_term_lookahead *) *entry)->lookahead =
                lookahead_term_num;
            ((struct set_term_lookahead *) *entry)->curr =
                (i + 1) % MAX_CACHED_GOTO_RESULTS;
#endif
	}
        pl[++pl_curr] = new_set;
#ifndef NO_YAEP_DEBUG_PRINT
        if (grammar->debug_level > 2)
	{
            fprintf (stderr, "New set=%d\n", pl_curr);
            if (grammar->debug_level > 3)
                set_print (stderr, new_set, pl_curr, grammar->debug_level > 4,
                           grammar->debug_level > 5);
	}
#endif
    }
    error_recovery_fin ();
}



/* This page contains code to work with parse states. */

/* The following describes parser state. */
struct parse_state
{
    /* The rule which we are processing. */
    struct rule *rule;
    /* Position in the rule where we are now. */
    int pos;
    /* The rule origin (start point of derivated string from rule rhs)
       and parser list in which we are now. */
    int orig, pl_ind;
    /* If the following value is NULL, then we do not need to create
       translation for this rule.  If we should create abstract node
       for this rule, the value refers for the abstract node and the
       displacement is undefined.  Otherwise, the two members is
       place into which we should place the translation of the rule.
       The following member is used only for states in the stack. */
    struct parse_state *parent_anode_state;
    int parent_disp;
    /* The following is used only for states in the table. */
    struct yaep_tree_node *anode;
};

/* The following os contains all allocated parser states. */
static os_t parse_state_os;

/* The following variable refers to head of chain of already allocated
   and then freed parser states. */
static struct parse_state *free_parse_state;

/* The following table is used to make translation for ambiguous
   grammar more compact.  It is used only when we want all
   translations. */
static hash_table_t parse_state_tab;	/* Key is rule, orig, pl_ind. */

/* Hash of parse state. */
static unsigned
parse_state_hash (hash_table_entry_t s)
{
    struct parse_state *state = ((struct parse_state *) s);

    /* The table contains only states with dot at the end of rule. */
    assert (state->pos == state->rule->rhs_len);
    return (((jauquet_prime_mod32 * hash_shift +
              (unsigned) (size_t) state->rule) * hash_shift +
             state->orig) * hash_shift + state->pl_ind);
}

/* Equality of parse states. */
static int
parse_state_eq (hash_table_entry_t s1, hash_table_entry_t s2)
{
    struct parse_state *state1 = ((struct parse_state *) s1);
    struct parse_state *state2 = ((struct parse_state *) s2);

    /* The table contains only states with dot at the end of rule. */
    assert (state1->pos == state1->rule->rhs_len
            && state2->pos == state2->rule->rhs_len);
    return (state1->rule == state2->rule && state1->orig == state2->orig
            && state1->pl_ind == state2->pl_ind);
}

/* The following function initializes work with parser states. */
static void
parse_state_init (void)
{
    free_parse_state = NULL;
    OS_CREATE (parse_state_os, grammar->alloc, 0);
    if (!grammar->one_parse_p)
        parse_state_tab =
            create_hash_table (grammar->alloc, toks_len * 2, parse_state_hash,
                               parse_state_eq);
}

/* The following function returns new parser state. */
#if MAKE_INLINE
INLINE
#endif
static struct parse_state *
parse_state_alloc (void)
{
    struct parse_state *result;

    if (free_parse_state == NULL)
    {
        OS_TOP_EXPAND (parse_state_os, sizeof (struct parse_state));
        result = (struct parse_state *) OS_TOP_BEGIN (parse_state_os);
        OS_TOP_FINISH (parse_state_os);
    }
    else
    {
        result = free_parse_state;
        free_parse_state = (struct parse_state *) free_parse_state->rule;
    }
    return result;
}

/* The following function frees STATE. */
#if MAKE_INLINE
INLINE
#endif
static void
parse_state_free (struct parse_state *state)
{
    state->rule = (struct rule *) free_parse_state;
    free_parse_state = state;
}

/* The following function searches for state in the table with the
   same characteristics as *STATE.  If it found it, it returns pointer
   to the state in the table.  Otherwise the function makes copy of
   *STATE, inserts into the table and returns pointer to copied state.
   In the last case, the function also sets up *NEW_P. */
#if MAKE_INLINE
INLINE
#endif
static struct parse_state *
parse_state_insert (struct parse_state *state, int *new_p)
{
    hash_table_entry_t *entry;

    entry = find_hash_table_entry (parse_state_tab, state, TRUE);

    *new_p = FALSE;
    if (*entry != NULL)
        return (struct parse_state *) *entry;
    *new_p = TRUE;
    /* We make copy because pl_ind can be changed in further processing
       state. */
    *entry = parse_state_alloc ();
    *(struct parse_state *) *entry = *state;
    return (struct parse_state *) *entry;
}

/* The following function finalizes work with parser states. */
static void
parse_state_fin (void)
{
    if (!grammar->one_parse_p)
        delete_hash_table (parse_state_tab);
    OS_DELETE (parse_state_os);
}



#ifndef NO_YAEP_DEBUG_PRINT

/* This page conatins code to traverse translation. */

/* To make better traversing and don't waist tree parse memory,
   we use the following structures to enumerate the tree node. */
struct trans_visit_node
{
    /* The following member is order number of the node.  This value is
       negative if we did not visit the node yet. */
    int num;
    /* The tree node itself. */
    struct yaep_tree_node *node;
};

/* The key of the following table is node itself. */
static hash_table_t trans_visit_nodes_tab;

/* All translation visit nodes are placed in the following stack.  All
   the nodes are in the table. */
static os_t trans_visit_nodes_os;

/* The following value is number of translation visit nodes. */
static int n_trans_visit_nodes;

/* Hash of translation visit node. */
static unsigned
trans_visit_node_hash (hash_table_entry_t n)
{
    return (size_t) ((struct trans_visit_node *) n)->node;
}

/* Equality of translation visit nodes. */
static int
trans_visit_node_eq (hash_table_entry_t n1, hash_table_entry_t n2)
{
    return (((struct trans_visit_node *) n1)->node
            == ((struct trans_visit_node *) n2)->node);
}

/* The following function checks presence translation visit node with
   given NODE in the table and if it is not present in the table, the
   function creates the translation visit node and inserts it into
   the table. */
static struct trans_visit_node *
visit_node (struct yaep_tree_node *node)
{
    struct trans_visit_node trans_visit_node;
    hash_table_entry_t *entry;

    trans_visit_node.node = node;
    entry = find_hash_table_entry (trans_visit_nodes_tab,
                                   &trans_visit_node, TRUE);

    if (*entry == NULL)
    {
        /* If it is the new node, we did not visit it yet. */
        trans_visit_node.num = -1 - n_trans_visit_nodes;
        n_trans_visit_nodes++;
        OS_TOP_ADD_MEMORY (trans_visit_nodes_os,
                           &trans_visit_node, sizeof (trans_visit_node));
        *entry = (hash_table_entry_t) OS_TOP_BEGIN (trans_visit_nodes_os);
        OS_TOP_FINISH (trans_visit_nodes_os);
    }
    return (struct trans_visit_node *) *entry;
}

/* The following function returns the positive order number of node
   with number NUM. */
#if MAKE_INLINE
INLINE
#endif
static int
canon_node_num (int num)
{
    return (num < 0 ? -num - 1 : num);
}

/* The following recursive function prints NODE into file F and prints
   all its children (if debug_level < 0 output format is for
   graphviz). */
static void
print_yaep_node (FILE * f, struct yaep_tree_node *node)
{
    struct trans_visit_node *trans_visit_node;
    struct yaep_tree_node *child;
    int i;

    assert (node != NULL);
    trans_visit_node = visit_node (node);
    if (trans_visit_node->num >= 0)
        return;
    trans_visit_node->num = -trans_visit_node->num - 1;
    if (grammar->debug_level > 0)
        fprintf (f, "%7d: ", trans_visit_node->num);
    switch (node->type)
    {
    case YAEP_NIL:
        if (grammar->debug_level > 0)
            fprintf (f, "EMPTY\n");
        break;
    case YAEP_ERROR:
        if (grammar->debug_level > 0)
            fprintf (f, "ERROR\n");
        break;
    case YAEP_TERM:
        if (grammar->debug_level > 0)
            fprintf (f, "TERMINAL: code=%d, repr=%s, mark=%d %c\n", node->val.term.code,
                     symb_find_by_code (node->val.term.code)->repr, node->val.term.mark, node->val.term.mark>32?node->val.term.mark:' ');
        break;
    case YAEP_ANODE:
        if (grammar->debug_level > 0)
	{
            fprintf (f, "ABSTRACT: %c%s (", node->val.anode.mark?node->val.anode.mark:' ', node->val.anode.name);
            for (i = 0; (child = node->val.anode.children[i]) != NULL; i++)
                fprintf (f, " %d", canon_node_num (visit_node (child)->num));
	}
        else
	{
            for (i = 0; (child = node->val.anode.children[i]) != NULL; i++)
	    {
                fprintf (f, "  \"%d: %s\" -> \"%d: ", trans_visit_node->num,
                         node->val.anode.name,
                         canon_node_num (visit_node (child)->num));
                switch (child->type)
		{
		case YAEP_NIL:
                    fprintf (f, "EMPTY");
                    break;
		case YAEP_ERROR:
                    fprintf (f, "ERROR");
                    break;
		case YAEP_TERM:
                    fprintf (f, "%s",
                             symb_find_by_code (child->val.term.code)->repr);
                    break;
		case YAEP_ANODE:
                    fprintf (f, "%s", child->val.anode.name);
                    break;
		case YAEP_ALT:
                    fprintf (f, "ALT");
                    break;
		default:
                    assert (FALSE);
		}
                fprintf (f, "\";\n");
	    }
	}
        for (i = 0; (child = node->val.anode.children[i]) != NULL; i++)
            print_yaep_node (f, child);
        break;
    case YAEP_ALT:
        if (grammar->debug_level > 0)
	{
            fprintf (f, "ALTERNATIVE: node=%d, next=",
                     canon_node_num (visit_node (node->val.alt.node)->num));
            if (node->val.alt.next != NULL)
                fprintf (f, "%d\n",
                         canon_node_num (visit_node (node->val.alt.next)->num));
            else
                fprintf (f, "nil\n");
	}
        else
	{
            fprintf (f, "  \"%d: ALT\" -> \"%d: ", trans_visit_node->num,
                     canon_node_num (visit_node (node->val.alt.node)->num));
            switch (node->val.alt.node->type)
	    {
	    case YAEP_NIL:
                fprintf (f, "EMPTY");
                break;
	    case YAEP_ERROR:
                fprintf (f, "ERROR");
                break;
	    case YAEP_TERM:
                fprintf (f, "%s",
                         symb_find_by_code (node->val.alt.node->val.term.code)->
                         repr);
                break;
	    case YAEP_ANODE:
                fprintf (f, "%s", node->val.alt.node->val.anode.name);
                break;
	    case YAEP_ALT:
                fprintf (f, "ALT");
                break;
	    default:
                assert (FALSE);
	    }
            fprintf (f, "\";\n");
            if (node->val.alt.next != NULL)
                fprintf (f, "  \"%d: ALT\" -> \"%d: ALT\";\n",
                         trans_visit_node->num,
                         canon_node_num (visit_node (node->val.alt.next)->num));
	}
        print_yaep_node (f, node->val.alt.node);
        if (node->val.alt.next != NULL)
            print_yaep_node (f, node->val.alt.next);
        break;
    default:
        assert (FALSE);
    }
}

/* The following function prints parse tree with ROOT. */
static void
print_parse (FILE * f, struct yaep_tree_node *root)
{
    trans_visit_nodes_tab =
        create_hash_table (grammar->alloc, toks_len * 2, trans_visit_node_hash,
                           trans_visit_node_eq);

    n_trans_visit_nodes = 0;
    OS_CREATE (trans_visit_nodes_os, grammar->alloc, 0);
    print_yaep_node (f, root);
    OS_DELETE (trans_visit_nodes_os);
    delete_hash_table (trans_visit_nodes_tab);
}

#endif

/* The following is number of created terminal, abstract, and
   alternative nodes. */
static int n_parse_term_nodes, n_parse_abstract_nodes, n_parse_alt_nodes;

/* The following function places translation NODE into *PLACE and
   creates alternative nodes if it is necessary. */
#if MAKE_INLINE
INLINE
#endif
static void
place_translation (struct yaep_tree_node **place, struct yaep_tree_node *node)
{
    struct yaep_tree_node *alt, *next_alt;

    assert (place != NULL);
    if (*place == NULL)
    {
        *place = node;
        return;
    }
    /* We need an alternative. */
#ifndef NO_YAEP_DEBUG_PRINT
    n_parse_alt_nodes++;
#endif
    alt = (struct yaep_tree_node *) (*parse_alloc) (sizeof
                                                    (struct yaep_tree_node));
    alt->type = YAEP_ALT;
    alt->val.alt.node = node;
    if ((*place)->type == YAEP_ALT)
        alt->val.alt.next = *place;
    else
    {
        /* We need alternative node for the 1st
           alternative too. */
        n_parse_alt_nodes++;
        next_alt = alt->val.alt.next
            = ((struct yaep_tree_node *)
               (*parse_alloc) (sizeof (struct yaep_tree_node)));
        next_alt->type = YAEP_ALT;
        next_alt->val.alt.node = *place;
        next_alt->val.alt.next = NULL;
    }
    *place = alt;
}

static struct yaep_tree_node *
copy_anode (struct yaep_tree_node **place, struct yaep_tree_node *anode,
	    struct rule *rule, int disp)
{
    struct yaep_tree_node *node;
    int i;

    node = ((struct yaep_tree_node *)
            (*parse_alloc) (sizeof (struct yaep_tree_node)
                            + sizeof (struct yaep_tree_node *)
                            * (rule->trans_len + 1)));
    *node = *anode;
    node->val.anode.children
        = ((struct yaep_tree_node **)
           ((char *) node + sizeof (struct yaep_tree_node)));
    for (i = 0; i <= rule->trans_len; i++)
        node->val.anode.children[i] = anode->val.anode.children[i];
    node->val.anode.children[disp] = NULL;
    place_translation (place, node);
    return node;
}

/* The following table is used to find allocated memory which should
   not be freed. */
static hash_table_t reserv_mem_tab;

/* The hash of the memory reference. */
static unsigned
reserv_mem_hash (hash_table_entry_t m)
{
    return (size_t) m;
}

/* The equity of the memory reference. */
static int
reserv_mem_eq (hash_table_entry_t m1, hash_table_entry_t m2)
{
    return m1 == m2;
}

/* The following vlo will contain references to memory which should be
   freed.  The same reference can be represented more on time. */
static vlo_t tnodes_vlo;

/* The following function sets up minimal cost for each abstract node.
   The function returns minimal translation corresponding to NODE.
   The function also collects references to memory which can be
   freed. Remeber that the translation is DAG, altenatives form lists
   (alt node may not refer for another alternative). */
static struct yaep_tree_node *
prune_to_minimal (struct yaep_tree_node *node, int *cost)
{
    struct yaep_tree_node *child, *alt, *next_alt, *result = NULL;
    int i, min_cost = INT_MAX;

    assert (node != NULL);
    switch (node->type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
    case YAEP_TERM:
        if (parse_free != NULL)
            VLO_ADD_MEMORY (tnodes_vlo, &node, sizeof (node));
        *cost = 0;
        return node;
    case YAEP_ANODE:
        if (node->val.anode.cost >= 0)
	{
            if (parse_free != NULL)
                VLO_ADD_MEMORY (tnodes_vlo, &node, sizeof (node));
            for (i = 0; (child = node->val.anode.children[i]) != NULL; i++)
	    {
                node->val.anode.children[i] = prune_to_minimal (child, cost);
                node->val.anode.cost += *cost;
	    }
            *cost = node->val.anode.cost;
            node->val.anode.cost = -node->val.anode.cost - 1;	/* flag of visit */
	}
        return node;
    case YAEP_ALT:
        for (alt = node; alt != NULL; alt = next_alt)
	{
            if (parse_free != NULL)
                VLO_ADD_MEMORY (tnodes_vlo, &alt, sizeof (alt));
            next_alt = alt->val.alt.next;
            alt->val.alt.node = prune_to_minimal (alt->val.alt.node, cost);
            if (alt == node || min_cost > *cost)
	    {
                min_cost = *cost;
                alt->val.alt.next = NULL;
                result = alt;
	    }
            else if (min_cost == *cost && !grammar->one_parse_p)
	    {
                alt->val.alt.next = result;
                result = alt;
	    }
	}
        *cost = min_cost;
        return (result->val.alt.next == NULL ? result->val.alt.node : result);
    default:
        assert (FALSE);
    }
    *cost = 0;
    return NULL;
}

/* The following function traverses the translation collecting
   reference to memory which may not be freed. */
static void
traverse_pruned_translation (struct yaep_tree_node *node)
{
    struct yaep_tree_node *child;
    hash_table_entry_t *entry;
    int i;

next:
    assert (node != NULL);
    if (parse_free != NULL
        && *(entry =
             find_hash_table_entry (reserv_mem_tab, node, TRUE)) == NULL)
        *entry = (hash_table_entry_t) node;
    switch (node->type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
    case YAEP_TERM:
        break;
    case YAEP_ANODE:
        if (parse_free != NULL
            && *(entry = find_hash_table_entry (reserv_mem_tab,
                                                node->val.anode.name,
                                                TRUE)) == NULL)
            *entry = (hash_table_entry_t) node->val.anode.name;
        for (i = 0; (child = node->val.anode.children[i]) != NULL; i++)
            traverse_pruned_translation (child);
        assert (node->val.anode.cost < 0);
        node->val.anode.cost = -node->val.anode.cost - 1;
        break;
    case YAEP_ALT:
        traverse_pruned_translation (node->val.alt.node);
        if ((node = node->val.alt.next) != NULL)
            goto next;
        break;
    default:
        assert (FALSE);
    }
    return;
}

/* The function finds and returns a minimal cost parse(s). */
static struct yaep_tree_node *
find_minimal_translation (struct yaep_tree_node *root)
{
    struct yaep_tree_node **node_ptr;
    int cost;

    if (parse_free != NULL)
    {
        reserv_mem_tab =
            create_hash_table (grammar->alloc, toks_len * 4, reserv_mem_hash,
                               reserv_mem_eq);

        VLO_CREATE (tnodes_vlo, grammar->alloc, toks_len * 4 * sizeof (void *));
    }
    root = prune_to_minimal (root, &cost);
    traverse_pruned_translation (root);
    if (parse_free != NULL)
    {
        for (node_ptr = (struct yaep_tree_node **) VLO_BEGIN (tnodes_vlo);
             node_ptr < (struct yaep_tree_node **) VLO_BOUND (tnodes_vlo);
             node_ptr++)
            if (*find_hash_table_entry (reserv_mem_tab, *node_ptr, TRUE) == NULL)
            {
                if ((*node_ptr)->type == YAEP_ANODE
                    && *find_hash_table_entry (reserv_mem_tab,
					       (*node_ptr)->val.anode.name,
					       TRUE) == NULL)
                {
                    (*parse_free) ((void *) (*node_ptr)->val.anode.name);
                }
                (*parse_free) (*node_ptr);
            }
        VLO_DELETE (tnodes_vlo);
        delete_hash_table (reserv_mem_tab);
    }
    return root;
}

/* The following function finds parse tree of parsed input.  The
   function sets up *AMBIGUOUS_P if we found that the grammer is
   ambigous (it works even we asked only one parse tree without
   alternatives). */
static struct yaep_tree_node *
make_parse (int *ambiguous_p)
{
    struct set *set, *check_set;
    struct set_core *set_core, *check_set_core;
    struct sit *sit, *check_sit;
    struct rule *rule, *sit_rule;
    struct symb *symb;
    struct core_symb_vect *core_symb_vect, *check_core_symb_vect;
    int i, j, k, found, pos, orig, pl_ind, n_candidates, disp;
    int sit_ind, check_sit_ind, sit_orig, check_sit_orig, new_p;
    struct parse_state *state, *orig_state, *curr_state;
    struct parse_state *table_state, *parent_anode_state;
    struct parse_state root_state;
    struct yaep_tree_node *result, *empty_node, *node, *error_node;
    struct yaep_tree_node *parent_anode, *anode, root_anode;
    int parent_disp;
    int saved_one_parse_p;
    struct yaep_tree_node **term_node_array = NULL;
    vlo_t stack, orig_states;

    n_parse_term_nodes = n_parse_abstract_nodes = n_parse_alt_nodes = 0;
    set = pl[pl_curr];
    assert (grammar->axiom != NULL);
    /* We have only one start situation: "$S : <start symb> $eof .".  */
    sit = (set->core->sits != NULL ? set->core->sits[0] : NULL);
    if (sit == NULL
#ifndef ABSOLUTE_DISTANCES
        || set->dists[0] != pl_curr
#else
        || set->dists[0] != 0
#endif
        || sit->rule->lhs != grammar->axiom || sit->pos != sit->rule->rhs_len)
    {
        /* It is possible only if error recovery is switched off.
           Because we always adds rule `axiom: error $eof'. */
        assert (!grammar->error_recovery_p);
        return NULL;
    }
    saved_one_parse_p = grammar->one_parse_p;
    if (grammar->cost_p)
        /* We need all parses to choose the minimal one */
        grammar->one_parse_p = FALSE;
    sit = set->core->sits[0];
    parse_state_init ();
    if (!grammar->one_parse_p)
    {
        void *mem;

        /* We need this array to reuse terminal nodes only for
           generation of several parses. */
        mem =
            yaep_malloc (grammar->alloc,
                         sizeof (struct yaep_tree_node *) * toks_len);
        term_node_array = (struct yaep_tree_node **) mem;
        for (i = 0; i < toks_len; i++)
            term_node_array[i] = NULL;
        /* The following is used to check necessity to create current
           state with different pl_ind. */
        VLO_CREATE (orig_states, grammar->alloc, 0);
    }
    VLO_CREATE (stack, grammar->alloc, 10000);
    VLO_EXPAND (stack, sizeof (struct parse_state *));
    state = parse_state_alloc ();
    ((struct parse_state **) VLO_BOUND (stack))[-1] = state;
    rule = state->rule = sit->rule;
    state->pos = sit->pos;
    state->orig = 0;
    state->pl_ind = pl_curr;
    result = NULL;
    root_state.anode = &root_anode;
    root_anode.val.anode.children = &result;
    state->parent_anode_state = &root_state;
    state->parent_disp = 0;
    state->anode = NULL;
    /* Create empty and error node: */
    empty_node = ((struct yaep_tree_node *)
                  (*parse_alloc) (sizeof (struct yaep_tree_node)));
    empty_node->type = YAEP_NIL;
    empty_node->val.nil.used = 0;
    error_node = ((struct yaep_tree_node *)
                  (*parse_alloc) (sizeof (struct yaep_tree_node)));
    error_node->type = YAEP_ERROR;
    error_node->val.error.used = 0;
    while (VLO_LENGTH (stack) != 0)
    {
#if !defined (NDEBUG) && !defined (NO_YAEP_DEBUG_PRINT)
        if ((grammar->debug_level > 2 && state->pos == state->rule->rhs_len)
            || grammar->debug_level > 3)
	{
            fprintf (stderr, "Processing top %ld, set place = %d, sit = ",
                     (long) VLO_LENGTH (stack) / sizeof (struct parse_state *) - 1,
                     state->pl_ind);
            rule_dot_print (stderr, state->rule, state->pos);
            fprintf (stderr, ", %d\n", state->orig);
	}
#endif
        pos = --state->pos;
        rule = state->rule;
        parent_anode_state = state->parent_anode_state;
        parent_anode = parent_anode_state->anode;
        parent_disp = state->parent_disp;
        anode = state->anode;
        disp = rule->order[pos];
        pl_ind = state->pl_ind;
        orig = state->orig;
        if (pos < 0)
	{
            /* We've processed all rhs of the rule. */
#if !defined (NDEBUG) && !defined (NO_YAEP_DEBUG_PRINT)
            if ((grammar->debug_level > 2 && state->pos == state->rule->rhs_len)
                || grammar->debug_level > 3)
	    {
                fprintf (stderr, "Poping top %ld, set place = %d, sit = ",
                         (long) VLO_LENGTH (stack) / sizeof (struct parse_state *) - 1,
                         state->pl_ind);
                rule_dot_print (stderr, state->rule, 0);
                fprintf (stderr, ", %d\n", state->orig);
	    }
#endif
            parse_state_free (state);
            VLO_SHORTEN (stack, sizeof (struct parse_state *));
            if (VLO_LENGTH (stack) != 0)
                state = ((struct parse_state **) VLO_BOUND (stack))[-1];
            if (parent_anode != NULL && rule->trans_len == 0 && anode == NULL)
	    {
                /* We do produce nothing but we should.  So write empty
                   node. */
                place_translation (parent_anode->val.anode.children +
                                   parent_disp, empty_node);
                empty_node->val.nil.used = 1;
	    }
            else if (anode != NULL)
	    {
                /* Change NULLs into empty nodes.  We can not make it
                   the first time because when building several parses
                   the NULL means flag of absence of translations (see
                   function `place_translation'). */
                for (i = 0; i < rule->trans_len; i++)
                    if (anode->val.anode.children[i] == NULL)
                    {
                        anode->val.anode.children[i] = empty_node;
                        empty_node->val.nil.used = 1;
                    }
	    }
            continue;
	}
        assert (pos >= 0);
        if ((symb = rule->rhs[pos])->term_p)
	{
            /* Terminal before dot: */
            pl_ind--;		/* l */
#ifndef ABSOLUTE_DISTANCES
            /* Because of error recovery toks [pl_ind].symb may be not
               equal to symb. */
            assert (toks[pl_ind].symb == symb);
#endif
            if (parent_anode != NULL && disp >= 0)
	    {
                /* We should generate and use the translation of the
                   terminal.  Add reference to the current node. */
                if (symb == grammar->term_error)
		{
                    node = error_node;
                    error_node->val.error.used = 1;
		}
                else if (!grammar->one_parse_p
                         && (node = term_node_array[pl_ind]) != NULL)
                    ;
                else
		{
                    n_parse_term_nodes++;
                    node = ((struct yaep_tree_node *)
                            (*parse_alloc) (sizeof (struct yaep_tree_node)));
                    node->type = YAEP_TERM;
                    node->val.term.code = symb->u.term.code;
                    // IXML
                    if (rule->marks && rule->marks[pos])
                    {
                        // Copy the mark from the rhs position on to the terminal.
                        node->val.term.mark = rule->marks[pos];
                    }
                    node->val.term.attr = toks[pl_ind].attr;
                    if (!grammar->one_parse_p)
                        term_node_array[pl_ind] = node;
		}
                place_translation
                    (anode != NULL ? anode->val.anode.children + disp
                     : parent_anode->val.anode.children + parent_disp, node);
	    }
            if (pos != 0)
                state->pl_ind = pl_ind;
            continue;
	}
        /* Nonterminal before dot: */
        set = pl[pl_ind];
        set_core = set->core;
        core_symb_vect = core_symb_vect_find (set_core, symb);
        assert (core_symb_vect->reduces.len != 0);
        n_candidates = 0;
        orig_state = state;
        if (!grammar->one_parse_p)
            VLO_NULLIFY (orig_states);
        for (i = 0; i < core_symb_vect->reduces.len; i++)
	{
            sit_ind = core_symb_vect->reduces.els[i];
            sit = set_core->sits[sit_ind];
            if (sit_ind < set_core->n_start_sits)
#ifndef ABSOLUTE_DISTANCES
                sit_orig = pl_ind - set->dists[sit_ind];
#else
	    sit_orig = set->dists[sit_ind];
#endif
            else if (sit_ind < set_core->n_all_dists)
#ifndef ABSOLUTE_DISTANCES
                sit_orig = pl_ind - set->dists[set_core->parent_indexes[sit_ind]];
#else
	    sit_orig = set->dists[set_core->parent_indexes[sit_ind]];
#endif
            else
                sit_orig = pl_ind;
#if !defined (NDEBUG) && !defined (NO_YAEP_DEBUG_PRINT)
            if (grammar->debug_level > 3)
	    {
                fprintf (stderr, "    Trying set place = %d, sit = ", pl_ind);
                sit_print (stderr, sit, grammar->debug_level > 5);
                fprintf (stderr, ", %d\n", sit_orig);
	    }
#endif
            check_set = pl[sit_orig];
            check_set_core = check_set->core;
            check_core_symb_vect = core_symb_vect_find (check_set_core, symb);
            assert (check_core_symb_vect != NULL);
            found = FALSE;
            for (j = 0; j < check_core_symb_vect->transitions.len; j++)
	    {
                check_sit_ind = check_core_symb_vect->transitions.els[j];
                check_sit = check_set->core->sits[check_sit_ind];
                if (check_sit->rule != rule || check_sit->pos != pos)
                    continue;
                check_sit_orig = sit_orig;
                if (check_sit_ind < check_set_core->n_all_dists)
		{
                    if (check_sit_ind < check_set_core->n_start_sits)
#ifndef ABSOLUTE_DISTANCES
                        check_sit_orig
                            = sit_orig - check_set->dists[check_sit_ind];
#else
		    check_sit_orig = check_set->dists[check_sit_ind];
#endif
                    else
#ifndef ABSOLUTE_DISTANCES
                        check_sit_orig
                            = (sit_orig
                               - check_set->dists[check_set_core->parent_indexes
                                                  [check_sit_ind]]);
#else
		    check_sit_orig
                        = check_set->dists[check_set_core->parent_indexes
                                           [check_sit_ind]];
#endif
		}
                if (check_sit_orig == orig)
		{
                    found = TRUE;
                    break;
		}
	    }
            if (!found)
                continue;
            if (n_candidates != 0)
	    {
                *ambiguous_p = TRUE;
                if (grammar->one_parse_p)
                    break;
	    }
            sit_rule = sit->rule;
            if (n_candidates == 0)
                orig_state->pl_ind = sit_orig;
            if (parent_anode != NULL && disp >= 0)
	    {
                /* We should generate and use the translation of the
                   nonterminal. */
                curr_state = orig_state;
                anode = orig_state->anode;
                /* We need translation of the rule. */
                if (n_candidates != 0)
		{
                    assert (!grammar->one_parse_p);
                    if (n_candidates == 1)
		    {
                        VLO_EXPAND (orig_states, sizeof (struct parse_state *));
                        ((struct parse_state **) VLO_BOUND (orig_states))[-1]
                            = orig_state;
		    }
                    for (j = (VLO_LENGTH (orig_states)
                              / sizeof (struct parse_state *) - 1); j >= 0; j--)
                        if (((struct parse_state **)
                             VLO_BEGIN (orig_states))[j]->pl_ind == sit_orig)
                            break;
                    if (j >= 0)
		    {
                        /* [A -> x., n] & [A -> y., n] */
                        curr_state = ((struct parse_state **)
                                      VLO_BEGIN (orig_states))[j];
                        anode = curr_state->anode;
		    }
                    else
		    {
                        /* [A -> x., n] & [A -> y., m] where n != m. */
                        /* It is different from the previous ones so add
                           it to process. */
                        state = parse_state_alloc ();
                        VLO_EXPAND (stack, sizeof (struct parse_state *));
                        ((struct parse_state **) VLO_BOUND (stack))[-1] = state;
                        *state = *orig_state;
                        state->pl_ind = sit_orig;
                        if (anode != NULL)
                            state->anode
                                = copy_anode (parent_anode->val.anode.children
                                              + parent_disp, anode, rule, disp);
                        VLO_EXPAND (orig_states, sizeof (struct parse_state *));
                        ((struct parse_state **) VLO_BOUND (orig_states))[-1]
                            = state;
#if !defined (NDEBUG) && !defined (NO_YAEP_DEBUG_PRINT)
                        if (grammar->debug_level > 3)
			{
                            fprintf (stderr,
                                     "  Adding top %ld, set place = %d, modified sit = ",
                                     (long) VLO_LENGTH (stack) /
                                     sizeof (struct parse_state *) - 1,
                                     sit_orig);
                            rule_dot_print (stderr, state->rule, state->pos);
                            fprintf (stderr, ", %d\n", state->orig);
			}
#endif
                        curr_state = state;
                        anode = state->anode;
		    }
		}		/* if (n_candidates != 0) */
                if (sit_rule->anode != NULL)
		{
                    /* This rule creates abstract node. */
                    state = parse_state_alloc ();
                    state->rule = sit_rule;
                    state->pos = sit->pos;
                    state->orig = sit_orig;
                    state->pl_ind = pl_ind;
                    table_state = NULL;
                    if (!grammar->one_parse_p)
                        table_state = parse_state_insert (state, &new_p);
                    if (table_state == NULL || new_p)
		    {
                        /* We need new abtract node. */
                        n_parse_abstract_nodes++;
                        node
                            = ((struct yaep_tree_node *)
                               (*parse_alloc) (sizeof (struct yaep_tree_node)
                                               + sizeof (struct yaep_tree_node *)
                                               * (sit_rule->trans_len + 1)));
                        state->anode = node;
                        if (table_state != NULL)
                            table_state->anode = node;
                        node->type = YAEP_ANODE;
                        if (sit_rule->caller_anode == NULL)
			{
                            sit_rule->caller_anode
                                = ((char *)
                                   (*parse_alloc) (strlen (sit_rule->anode) + 1));
                            strcpy (sit_rule->caller_anode, sit_rule->anode);
			}
                        node->val.anode.name = sit_rule->caller_anode;
                        node->val.anode.cost = sit_rule->anode_cost;
                        // IXML Copy the rule name -to the generated abstract node.
                        node->val.anode.mark = sit_rule->mark;
                        if (rule->marks && rule->marks[pos])
                        {
                            // But override the mark with the rhs mark!
                            node->val.anode.mark = rule->marks[pos];
                        }
                        /////////
                        node->val.anode.children
                            = ((struct yaep_tree_node **)
                               ((char *) node + sizeof (struct yaep_tree_node)));
                        for (k = 0; k <= sit_rule->trans_len; k++)
                            node->val.anode.children[k] = NULL;
                        VLO_EXPAND (stack, sizeof (struct parse_state *));
                        ((struct parse_state **) VLO_BOUND (stack))[-1] = state;
                        if (anode == NULL)
			{
                            state->parent_anode_state
                                = curr_state->parent_anode_state;
                            state->parent_disp = parent_disp;
			}
                        else
			{
                            state->parent_anode_state = curr_state;
                            state->parent_disp = disp;
			}
#if !defined (NDEBUG) && !defined (NO_YAEP_DEBUG_PRINT)
                        if (grammar->debug_level > 3)
			{
                            fprintf (stderr,
                                     "  Adding top %ld, set place = %d, sit = ",
                                     (long) VLO_LENGTH (stack) /
                                     sizeof (struct parse_state *) - 1, pl_ind);
                            sit_print (stderr, sit, grammar->debug_level > 5);
                            fprintf (stderr, ", %d\n", sit_orig);
			}
#endif
		    }
                    else
		    {
                        /* We allready have the translation. */
                        assert (!grammar->one_parse_p);
                        parse_state_free (state);
                        state = ((struct parse_state **) VLO_BOUND (stack))[-1];
                        node = table_state->anode;
                        assert (node != NULL);
#if !defined (NDEBUG) && !defined (NO_YAEP_DEBUG_PRINT)
                        if (grammar->debug_level > 3)
			{
                            fprintf (stderr,
                                     "  Found prev. translation: set place = %d, sit = ",
                                     pl_ind);
                            sit_print (stderr, sit, grammar->debug_level > 5);
                            fprintf (stderr, ", %d\n", sit_orig);
			}
#endif
		    }
                    place_translation (anode == NULL
                                       ? parent_anode->val.anode.children
                                       + parent_disp
                                       : anode->val.anode.children + disp,
                                       node);
		}		/* if (sit_rule->anode != NULL) */
                else if (sit->pos != 0)
		{
                    /* We should generate and use the translation of the
                       nonterminal.  Add state to get a translation. */
                    state = parse_state_alloc ();
                    VLO_EXPAND (stack, sizeof (struct parse_state *));
                    ((struct parse_state **) VLO_BOUND (stack))[-1] = state;
                    state->rule = sit_rule;
                    state->pos = sit->pos;
                    state->orig = sit_orig;
                    state->pl_ind = pl_ind;
                    state->parent_anode_state = (anode == NULL
                                                 ? curr_state->
                                                 parent_anode_state :
                                                 curr_state);
                    state->parent_disp = anode == NULL ? parent_disp : disp;
                    state->anode = NULL;
#if !defined (NDEBUG) && !defined (NO_YAEP_DEBUG_PRINT)
                    if (grammar->debug_level > 3)
		    {
                        fprintf (stderr,
                                 "  Adding top %ld, set place = %d, sit = ",
                                 (long) VLO_LENGTH (stack) /
                                 sizeof (struct parse_state *) - 1, pl_ind);
                        sit_print (stderr, sit, grammar->debug_level > 5);
                        fprintf (stderr, ", %d\n", sit_orig);
		    }
#endif
		}
                else
		{
                    /* Empty rule should produce something not abtract
                       node.  So place empty node. */
                    place_translation (anode == NULL
                                       ? parent_anode->val.anode.children
                                       + parent_disp
                                       : anode->val.anode.children + disp,
                                       empty_node);
                    empty_node->val.nil.used = 1;
		}
	    }			/* if (parent_anode != NULL && disp >= 0) */
            n_candidates++;
	}			/* For all reduces of the nonterminal. */
        /* We should have a parse. */
        assert (n_candidates != 0
                && (!grammar->one_parse_p || n_candidates == 1));
    }				/* For all parser states. */
    VLO_DELETE (stack);
    if (!grammar->one_parse_p)
    {
        VLO_DELETE (orig_states);
        yaep_free (grammar->alloc, term_node_array);
    }
    parse_state_fin ();
    grammar->one_parse_p = saved_one_parse_p;
    if (grammar->cost_p && *ambiguous_p)
        /* We can not build minimal tree during building parsing list
           because we have not the translation yet.  We can not make it
           during parsing because the abstract nodes are created before
           their children. */
        result = find_minimal_translation (result);
#ifndef NO_YAEP_DEBUG_PRINT
    if (grammar->debug_level > 1)
    {
        fprintf (stderr, "Translation:\n");
        print_parse (stderr, result);
        fprintf (stderr, "\n");
    }
    else if (grammar->debug_level < 0)
    {
        fprintf (stderr, "digraph CFG {\n");
        fprintf (stderr, "  node [shape=ellipse, fontsize=200];\n");
        fprintf (stderr, "  ratio=fill;\n");
        fprintf (stderr, "  ordering=out;\n");
        fprintf (stderr, "  page = \"8.5, 11\"; // inches\n");
        fprintf (stderr, "  size = \"7.5, 10\"; // inches\n\n");
        print_parse (stderr, result);
        fprintf (stderr, "}\n");
    }
#endif

    /* Free empty and error node if they have not been used */
    if (parse_free != NULL)
    {
        if (!empty_node->val.nil.used)
	{
            parse_free (empty_node);
	}
        if (!error_node->val.error.used)
	{
            parse_free (error_node);
	}
    }

    assert (result != NULL
            && (!grammar->one_parse_p || n_parse_alt_nodes == 0));
    return result;
}

static void *
parse_alloc_default (int nmemb)
{
    void *result;

    assert (nmemb > 0);

    result = malloc (nmemb);
    if (result == NULL)
    {
        exit (1);
    }

    return result;
}

static void
parse_free_default (void *mem)
{
    free (mem);
}

/* The following function parses input according read grammar.
   ONE_PARSE_FLAG means build only one parse tree.  For unambiguous
   grammar the flag does not affect the result.  LA_LEVEL means usage
   of static (if 1) or dynamic (2) lookahead to decrease size of sets.
   Static lookaheads gives the best results with the point of space
   and speed, dynamic ones does sligthly worse, and no usage of
   lookaheds does the worst.  D_LEVEL says what debugging information
   to output (it works only if we compiled without defined macro
   NO_YAEP_DEBUG_PRINT).  The function returns the error code (which
   will be also in error_code).  The function sets up
   *AMBIGUOUS_P if we found that the grammer is ambigous (it works even
   we asked only one parse tree without alternatives). */
int
yaep_parse (struct grammar *g,
	    int (*read) (void **attr),
	    void (*error) (int err_tok_num, void *err_tok_attr,
			   int start_ignored_tok_num,
			   void *start_ignored_tok_attr,
			   int start_recovered_tok_num,
			   void *start_recovered_tok_attr),
	    void *(*alloc) (int nmemb),
	    void (*free) (void *mem),
	    struct yaep_tree_node **root, int *ambiguous_p)
{
    int code, tok_init_p, parse_init_p;
    int tab_collisions, tab_searches;

    /* Set up parse allocation */
    if (alloc == NULL)
    {
        if (free != NULL)
	{
            /* Cannot allocate memory with a null function */
            return YAEP_NO_MEMORY;
	}
        /* Set up defaults */
        alloc = parse_alloc_default;
        free = parse_free_default;
    }

    grammar = g;
    assert (grammar != NULL);
    symbs_ptr = g->symbs_ptr;
    term_sets_ptr = g->term_sets_ptr;
    rules_ptr = g->rules_ptr;
    read_token = read;
    syntax_error = error;
    parse_alloc = alloc;
    parse_free = free;
    *root = NULL;
    *ambiguous_p = FALSE;
    pl_init ();
    tok_init_p = parse_init_p = FALSE;
    if ((code = setjmp (error_longjump_buff)) != 0)
    {
        pl_fin ();
        if (parse_init_p)
            yaep_parse_fin ();
        if (tok_init_p)
            tok_fin ();
        return code;
    }
    if (grammar->undefined_p)
        yaep_error (YAEP_UNDEFINED_OR_BAD_GRAMMAR, "undefined or bad grammar");
    n_goto_successes = 0;
    tok_init ();
    tok_init_p = TRUE;
    read_toks ();
    yaep_parse_init (toks_len);
    parse_init_p = TRUE;
    pl_create ();
    tab_collisions = get_all_collisions ();
    tab_searches = get_all_searches ();

    build_pl ();
    *root = make_parse (ambiguous_p);

    tab_collisions = get_all_collisions () - tab_collisions;
    tab_searches = get_all_searches () - tab_searches;

#ifndef NO_YAEP_DEBUG_PRINT
    if (grammar->debug_level > 0)
    {
        fprintf (stderr, "%sGrammar: #terms = %d, #nonterms = %d, ",
                 *ambiguous_p ? "AMBIGUOUS " : "",
                 symbs_ptr->n_terms, symbs_ptr->n_nonterms);
        fprintf (stderr, "#rules = %d, rules size = %d\n",
                 rules_ptr->n_rules,
                 rules_ptr->n_rhs_lens + rules_ptr->n_rules);
        fprintf (stderr, "Input: #tokens = %d, #unique situations = %d\n",
                 toks_len, n_all_sits);
        fprintf (stderr, "       #terminal sets = %d, their size = %d\n",
                 term_sets_ptr->n_term_sets, term_sets_ptr->n_term_sets_size);
        fprintf (stderr,
                 "       #unique set cores = %d, #their start situations = %d\n",
                 n_set_cores, n_set_core_start_sits);
        fprintf (stderr,
                 "       #parent indexes for some non start situations = %d\n",
                 n_parent_indexes);
        fprintf (stderr,
                 "       #unique set dist. vects = %d, their length = %d\n",
                 n_set_dists, n_set_dists_len);
        fprintf (stderr,
                 "       #unique sets = %d, #their start situations = %d\n",
                 n_sets, n_sets_start_sits);
        fprintf (stderr,
                 "       #unique triples (set, term, lookahead) = %d, goto successes=%d\n",
                 n_set_term_lookaheads, n_goto_successes);
        fprintf (stderr,
                 "       #pairs(set core, symb) = %d, their trans+reduce vects length = %d\n",
                 n_core_symb_pairs, n_core_symb_vect_len);
        fprintf (stderr,
                 "       #unique transition vectors = %d, their length = %d\n",
                 n_transition_vects, n_transition_vect_len);
#ifdef TRANSITIVE_TRANSITION
        fprintf (stderr,
                 "       #unique transitive transition vectors = %d, their length = %d\n",
                 n_transitive_transition_vects,
                 n_transitive_transition_vect_len);
#endif
        fprintf (stderr,
                 "       #unique reduce vectors = %d, their length = %d\n",
                 n_reduce_vects, n_reduce_vect_len);
        fprintf (stderr,
                 "       #term nodes = %d, #abstract nodes = %d\n",
                 n_parse_term_nodes, n_parse_abstract_nodes);
        fprintf (stderr,
                 "       #alternative nodes = %d, #all nodes = %d\n",
                 n_parse_alt_nodes,
                 n_parse_term_nodes + n_parse_abstract_nodes
                 + n_parse_alt_nodes);
        if (tab_searches == 0)
            tab_searches++;
        fprintf (stderr,
                 "       #table collisions = %.2g%% (%d out of %d)\n",
                 tab_collisions * 100.0 / tab_searches,
                 tab_collisions, tab_searches);
    }
#endif
    yaep_parse_fin ();
    tok_fin ();
    return 0;
}

/* The following function frees memory allocated for the grammar. */
void
yaep_free_grammar (struct grammar *g)
{
    YaepAllocator *allocator;

    if (g != NULL)
    {
        allocator = g->alloc;
        pl_fin ();
        rule_fin (g->rules_ptr);
        term_set_fin (g->term_sets_ptr);
        symb_fin (g->symbs_ptr);
        yaep_free (allocator, g);
        yaep_alloc_del (allocator);
    }
    grammar = NULL;
}

static void
free_tree_reduce (struct yaep_tree_node *node)
{
    enum yaep_tree_node_type type;
    struct yaep_tree_node **childp;
    size_t numChildren, pos, freePos;

    assert (node != NULL);
    assert ((node->type & _yaep_VISITED) == 0);

    type = node->type;
    node->type = (enum yaep_tree_node_type) (node->type | _yaep_VISITED);

    switch (type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
    case YAEP_TERM:
        break;

    case YAEP_ANODE:
        if (node->val.anode.name[0] == '\0')
	{
            /* We have already seen the node name */
            node->val.anode.name = NULL;
	}
        else
	{
            /* Mark the node name as seen */
            node->val._anode_name.name[0] = '\0';
	}
        for (numChildren = 0, childp = node->val.anode.children;
             *childp != NULL; ++numChildren, ++childp)
	{
            if ((*childp)->type & _yaep_VISITED)
	    {
                *childp = NULL;
	    }
            else
	    {
                free_tree_reduce (*childp);
	    }
	}
        /* Compactify children array */
        for (freePos = 0, pos = 0; pos != numChildren; ++pos)
	{
            if (node->val.anode.children[pos] != NULL)
	    {
                if (freePos < pos)
		{
                    node->val.anode.children[freePos] =
                        node->val.anode.children[pos];
                    node->val.anode.children[pos] = NULL;
		}
                ++freePos;
	    }
	}
        break;

    case YAEP_ALT:
        if (node->val.alt.node->type & _yaep_VISITED)
	{
            node->val.alt.node = NULL;
	}
        else
	{
            free_tree_reduce (node->val.alt.node);
	}
        while ((node->val.alt.next != NULL)
               && (node->val.alt.next->type & _yaep_VISITED))
	{
            assert (node->val.alt.next->type == (YAEP_ALT | _yaep_VISITED));
            node->val.alt.next = node->val.alt.next->val.alt.next;
	}
        if (node->val.alt.next != NULL)
	{
            assert ((node->val.alt.next->type & _yaep_VISITED) == 0);
            free_tree_reduce (node->val.alt.next);
	}
        break;

    default:
        assert ("This should not happen" == NULL);
    }
}

static void
free_tree_sweep (struct yaep_tree_node *node, void (*parse_free) (void *),
		 void (*termcb) (struct yaep_term *))
{
    enum yaep_tree_node_type type;
    struct yaep_tree_node *next;
    struct yaep_tree_node **childp;

    if (node == NULL)
    {
        return;
    }

    assert (node->type & _yaep_VISITED);
    type = (enum yaep_tree_node_type) (node->type & ~_yaep_VISITED);

    switch (type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
        break;

    case YAEP_TERM:
        if (termcb != NULL)
	{
            termcb (&node->val.term);
	}
        break;

    case YAEP_ANODE:
        parse_free (node->val._anode_name.name);
        for (childp = node->val.anode.children; *childp != NULL; ++childp)
	{
            free_tree_sweep (*childp, parse_free, termcb);
	}
        break;

    case YAEP_ALT:
        free_tree_sweep (node->val.alt.node, parse_free, termcb);
        next = node->val.alt.next;
        parse_free (node);
        free_tree_sweep (next, parse_free, termcb);
        return;			/* Tail recursion */

    default:
        assert ("This should not happen" == NULL);
    }

    parse_free (node);
}

void
yaep_free_tree (struct yaep_tree_node *root, void (*parse_free) (void *),
		void (*termcb) (struct yaep_term *))
{
    if (root == NULL)
    {
        return;
    }
    if (parse_free == NULL)
    {
        parse_free = parse_free_default;
    }

    /* Since the parse tree is actually a DAG, we must carefully avoid
     * double free errors. Therefore, we walk the parse tree twice.
     * On the first walk, we reduce the DAG to an actual tree.
     * On the second walk, we recursively free the tree nodes. */
    free_tree_reduce (root);
    free_tree_sweep (root, parse_free, termcb);
}

/* This page contains a test code for Earley's algorithm.  To use it,
   define macro YAEP_TEST during compilation. */

#ifdef YAEP_TEST

/* All parse_alloc memory is contained here. */
static os_t mem_os;

static void *
test_parse_alloc (int size)
{
    void *result;

    OS_TOP_EXPAND (mem_os, size);
    result = OS_TOP_BEGIN (mem_os);
    OS_TOP_FINISH (mem_os);
    return result;
}

/* The following variable is the current number of next input grammar
   terminal. */
static int nterm;

/* The following function imported by Earley's algorithm (see comments
   in the interface file). */
const char *
read_terminal (int *code)
{
    nterm++;
    switch (nterm)
    {
    case 1:
        *code = 'a';
        return "a";
    case 2:
        *code = '+';
        return "+";
    case 3:
        *code = '*';
        return "*";
    case 4:
        *code = '(';
        return "(";
    case 5:
        *code = ')';
        return ")";
    default:
        return NULL;
    }
}

/* The following variable is the current number of next rule grammar
   terminal. */
static int nrule;

/* The following function imported by Earley's algorithm (see comments
   in the interface file). */
const char *
read_rule (const char ***rhs, const char **anode, int *anode_cost,
	   int **transl)
{
    static const char *rhs_1[] = { "T", NULL };
    static int tr_1[] = { 0, -1 };
    static const char *rhs_2[] = { "E", "+", "T", NULL };
    static int tr_2[] = { 0, 2, -1 };
    static const char *rhs_3[] = { "F", NULL };
    static int tr_3[] = { 0, -1 };
    static const char *rhs_4[] = { "T", "*", "F", NULL };
    static int tr_4[] = { 0, 2, -1 };
    static const char *rhs_5[] = { "a", NULL };
    static int tr_5[] = { 0, -1 };
    static const char *rhs_6[] = { "(", "E", ")", NULL };
    static int tr_6[] = { 1, -1 };

    nrule++;
    switch (nrule)
    {
    case 1:
        *rhs = rhs_1;
        *anode = NULL;
        *anode_cost = 0;
        *transl = tr_1;
        return "E";
    case 2:
        *rhs = rhs_2;
        *anode = "plus";
        *transl = tr_2;
        return "E";
    case 3:
        *rhs = rhs_3;
        *anode = NULL;
        *anode_cost = 0;
        *transl = tr_3;
        return "T";
    case 4:
        *rhs = rhs_4;
        *anode = "mult";
        *transl = tr_4;
        return "T";
    case 5:
        *rhs = rhs_5;
        *anode = NULL;
        *anode_cost = 0;
        *transl = tr_5;
        return "F";
    case 6:
        *rhs = rhs_6;
        *anode = NULL;
        *anode_cost = 0;
        *transl = tr_6;
        return "F";
    default:
        return NULL;
    }
}

/* The following variable is the current number of next input
   token. */
static int ntok;

/* The following function imported by Earley's algorithm (see comments
   in the interface file). */
static int
test_read_token (void **attr)
{
    const char input[] = "a+a*(a*a+a)";

    ntok++;
    *attr = NULL;
    if (ntok < sizeof (input))
        return input[ntok - 1];
    else
        return -1;
}

/* Printing syntax error. */
static void
test_syntax_error (int err_tok_num, void *err_tok_attr,
		   int start_ignored_tok_num, void *start_ignored_tok_attr,
		   int start_recovered_tok_num,
		   void *start_recovered_tok_attr)
{
    if (start_ignored_tok_num < 0)
        fprintf (stderr, "Syntax error on token %d\n", err_tok_num);
    else
        fprintf
            (stderr,
             "Syntax error on token %d:ignore %d tokens starting with token = %d\n",
             err_tok_num, start_recovered_tok_num - start_ignored_tok_num,
             start_ignored_tok_num);
}

#ifndef NOT_DEFINED
/* The following two functions calls earley parser with two different
   ways of forming grammars. */
static void
use_functions (int argc, char **argv)
{
    struct grammar *g;
    struct yaep_tree_node *root;
    int ambiguous_p;

    nterm = nrule = 0;
    fprintf (stderr, "Use functions\n");
    if ((g = yaep_create_grammar ()) == NULL)
    {
        fprintf (stderr, "No memory\n");
        exit (1);
    }
    OS_CREATE (mem_os, grammar->alloc, 0);
    yaep_set_one_parse_flag (g, FALSE);
    if (argc > 1)
        yaep_set_lookahead_level (g, atoi (argv[1]));
    if (argc > 2)
        yaep_set_debug_level (g, atoi (argv[2]));
    else
        yaep_set_debug_level (g, 3);
    if (argc > 3)
        yaep_set_error_recovery_flag (g, atoi (argv[3]));
    if (argc > 4)
        yaep_set_one_parse_flag (g, atoi (argv[4]));
    if (yaep_read_grammar (g, TRUE, read_terminal, read_rule) != 0)
    {
        fprintf (stderr, "%s\n", yaep_error_message (g));
        OS_DELETE (mem_os);
        exit (1);
    }
    ntok = 0;
    if (yaep_parse (g, test_read_token, test_syntax_error, test_parse_alloc,
                    NULL, &root, &ambiguous_p))
        fprintf (stderr, "yaep_parse: %s\n", yaep_error_message (g));
    OS_DELETE (mem_os);
    yaep_free_grammar (g);
}
#endif

static const char *description =
    "\n"
    "TERM;\n"
    "E : T         # 0\n"
    "  | E '+' T   # plus (0 2)\n"
    "  ;\n"
    "T : F         # 0\n"
    "  | T '*' F   # mult (0 2)\n"
    "  ;\n"
    "F : 'a'       # 0\n"
    "  | '(' E ')' # 1\n"
    "  ;\n";

#ifndef NOT_DEFINED
static void
use_description (int argc, char **argv)
{
    struct grammar *g;
    struct yaep_tree_node *root;
    int ambiguous_p;

    fprintf (stderr, "Use description\n");
    if ((g = yaep_create_grammar ()) == NULL)
    {
        fprintf (stderr, "yaep_create_grammar: No memory\n");
        exit (1);
    }
    OS_CREATE (mem_os, grammar->alloc, 0);
    yaep_set_one_parse_flag (g, FALSE);
    if (argc > 1)
        yaep_set_lookahead_level (g, atoi (argv[1]));
    if (argc > 2)
        yaep_set_debug_level (g, atoi (argv[2]));
    else
        yaep_set_debug_level (g, 3);
    if (argc > 3)
        yaep_set_error_recovery_flag (g, atoi (argv[3]));
    if (argc > 4)
        yaep_set_one_parse_flag (g, atoi (argv[4]));
    if (yaep_parse_grammar (g, TRUE, description) != 0)
    {
        fprintf (stderr, "%s\n", yaep_error_message (g));
        OS_DELETE (mem_os);
        exit (1);
    }
    if (yaep_parse (g, test_read_token, test_syntax_error, test_parse_alloc,
                    NULL, &root, &ambiguous_p))
        fprintf (stderr, "yaep_parse: %s\n", yaep_error_message (g));
    OS_DELETE (mem_os);
    yaep_free_grammar (g);
}
#endif

int
main (int argc, char **argv)
{
    /* Don't expect the same statistics output because the order of
       situations in sets may be different in different calls because of
       memory allocations/freeing (we use situation address to order
       situations in sets) . */
    if (argc <= 1)
        use_description (argc, argv);
    else if (atoi (argv[1]))
        use_description (argc - 1, argv + 1);
    else
        use_functions (argc - 1, argv + 1);
    exit (0);
}

#endif /* #ifdef YAEP_TEST */
/****************** YAEP parser single source file end **********************/
#endif // YAEP_MODULE
