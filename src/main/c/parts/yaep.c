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
  YAEP(Yet Another Earley Parser)

  Copyright(c) 1997-2018  Vladimir Makarov <vmakarov@gcc.gnu.org>
  Copyright(c) 2024 Fredrik Öhrström <oehrstroem@gmail.com>

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files(the
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

/* This file implements parsing any context free grammar with minimal error recovery
   and syntax directed translation.  The algorithm is originated from
   Earley's algorithm.  The algorithm is sufficiently fast to be used
   in serious language processors.*/

#include <assert.h>

#ifndef NDEBUG
#define NDEBUG 1
#endif

#ifdef YAEP_DEBUG
#undef NDEBUG
#endif

#define TRACE_F
//fprintf(stderr, "TRACE %s\n", __func__)
#define TRACE_FA(cformat, ...)
//fprintf(stderr, "TRACE %s " cformat "\n", __func__, __VA_ARGS__)

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

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/* Terminals are stored a in term set using bits in a bit array.
   The array consists of long ints, typedefed as term_set_el_t.
   A long int is 8 bytes, ie 64 bits. */
typedef long int term_set_el_t;

/* Calculate the number of required term set elements from the number of bits we want to store. */
#define CALC_NUM_ELEMENTS(num_bits) ((num_bits+63)/64)

#ifndef YAEP_MAX_ERROR_MESSAGE_LENGTH
#define YAEP_MAX_ERROR_MESSAGE_LENGTH 200
#endif

/* As of Unicode 16 there are 155_063 allocated unicode code points.
   Lets pick 200_000 as the max, it shrinks to max-min code point anyway.*/
#define MAX_SYMB_CODE_TRANS_VECT_SIZE 200000

/* The initial length of array(in tokens) in which input tokens are placed.*/
#ifndef YAEP_INIT_TOKENS_NUMBER
#define YAEP_INIT_TOKENS_NUMBER 10000
#endif

/* The following is default number of tokens sucessfully matched to
   stop error recovery alternative(state).*/
#define DEFAULT_RECOVERY_TOKEN_MATCHES 3

/* Define this if you want to reuse already calculated Earley's sets
   and fast their reproduction.  It considerably speed up the parser. */
#define USE_SET_HASH_TABLE

/* Maximal goto sets saved for triple(set, terminal, lookahead). */
#define MAX_CACHED_GOTO_RESULTS 3

/* Prime number(79087987342985798987987) mod 32 used for hash calculations. */
static const unsigned jauquet_prime_mod32 = 2053222611;

/* Shift used for hash calculations. */
static const unsigned hash_shift = 611;

struct YaepSymb;
typedef struct YaepSymb YaepSymb;

struct YaepVocabulary;
typedef struct YaepVocabulary YaepVocabulary;

struct YaepTabTermSet;
typedef struct YaepTabTermSet YaepTabTermSet;

struct YaepTermStorage;
typedef struct YaepTermStorage YaepTermStorage;

struct YaepRule;
typedef struct YaepRule YaepRule;

struct YaepRuleStorage;
typedef struct YaepRuleStorage YaepRuleStorage;

struct YaepVect;
typedef struct YaepVect YaepVect;

struct YaepCoreSymbVect;
typedef struct YaepCoreSymbVect YaepCoreSymbVect;

struct YaepSetCore;
typedef struct YaepSetCore YaepSetCore;

struct YaepSet;
typedef struct YaepSet YaepSet;

struct YaepSituation;
typedef struct YaepSituation YaepSituation;

struct YaepTok;
typedef struct YaepTok YaepTok;

struct YaepSetTermLookAhead;
typedef struct YaepSetTermLookAhead YaepSetTermLookAhead;

struct YaepInternalParseState;
typedef struct YaepInternalParseState YaepInternalParseState;

struct YaepTransVisitNode;
typedef struct YaepTransVisitNode YaepTransVisitNode;

// Structure definitions ////////////////////////////////////////////////////

struct YaepGrammar
{
    /* The following member is TRUE if the grammar is undefined(you
       should set up the grammar by yaep_read_grammar or
       yaep_parse_grammar) or bad(error was occured in setting up the
       grammar).*/
    int undefined_p;

    /* This member always contains the last occurred error code for given grammar.*/
    int error_code;

    /* This member contains message are always contains error message
       corresponding to the last occurred error code.*/
    char error_message[YAEP_MAX_ERROR_MESSAGE_LENGTH + 1];

    /* The following is grammar axiom.  There is only one rule with axiom in lhs.*/
    YaepSymb *axiom;

    /* The following auxiliary symbol denotes EOF.*/
    YaepSymb *end_marker;

    /* The following auxiliary symbol is used for describing error recovery.*/
    YaepSymb *term_error;

    /* And its internal number.*/
    int term_error_num;

    /* The level of usage of lookaheads:
       0    - no usage
       1    - statik lookaheads
       >= 2 - dynamic lookaheads*/
    int lookahead_level;

    /* The following value means how much subsequent tokens should be
       successfuly shifted to finish error recovery.*/
    int recovery_token_matches;

    /* The following value is debug level:
       <0 - print translation for graphviz.
       0 - print nothing.
       1 - print statistics.
       2 - print parse tree.
       3 - print rules, parser list
       4 - print sets.
       5 - print also nonstart situations.
       6 - print additionally lookaheads.*/
    int debug_level;

    /* The following value is TRUE if we need only one parse.*/
    int one_parse_p;

    /* The following value is TRUE if we need parse(s) with minimal costs.*/
    int cost_p;

    /* The following value is TRUE if we need to make error recovery.*/
    int error_recovery_p;

    /* The following vocabulary used for this grammar.*/
    YaepVocabulary *symbs_ptr;

    /* The following rules used for this grammar.*/
    YaepRuleStorage *rules_ptr;

    /* The following terminal sets used for this grammar.*/
    YaepTermStorage *term_sets_ptr;

    /* Allocator.*/
    YaepAllocator *alloc;

    /* A user supplied pointer that is available to user callbacks through the grammar pointer. */
    void *user_data;
};

struct YaepSymb
{
    /* The following is external representation of the symbol.  It
       should be allocated by parse_alloc because the string will be
       referred from parse tree.*/
    const char *repr;
    union
    {
        struct
        {
            /* The following value is code of the terminal symbol.*/
            int code;
            /* The following member is order number of the terminal.*/
            int term_num;
        } term;
        struct
        {
            /* The following refers for all rules with the nonterminal
               symbol is in the left hand side of the rules.*/
            YaepRule*rules;
            /* The following member is order number of the nonterminal.*/
            int nonterm_num;
            /* The following value is nonzero if nonterminal may derivate
               itself.  In other words there is a grammar loop for this
               nonterminal.*/
            int loop_p;
            /* The following members are FIRST and FOLLOW sets of the nonterminal.*/
            term_set_el_t *first, *follow;
        } nonterm;
    } u;
    /* The following member is TRUE if it is nonterminal.*/
    char term_p;
    /* The following member value(if defined) is TRUE if the symbol is
       accessible(derivated) from the axiom.*/
    char access_p;
    /* The following member is TRUE if it is a termainal or it is a
       nonterminal which derivates a terminal string.*/
    char derivation_p;
    /* The following is TRUE if it is nonterminal which may derivate
       empty string.*/
    char empty_p;
    /* The following member is order number of symbol.*/
    int num;
#ifdef USE_CORE_SYMB_HASH_TABLE
    /* The following is used as cache for subsequent search for
       core_symb_vect with given symb.*/
    YaepCoreSymbVect*cached_core_symb_vect;
#endif
};

/* The following structure contians all information about grammar vocabulary.*/
struct YaepVocabulary
{
    /* The following is number of all symbols and terminals.  The
       variables can be read externally.*/
    int nn_terms, n_nonterms;

    /* All symbols are placed in the following object.*/
    os_t symbs_os;

    /* All references to the symbols, terminals, nonterminals are stored
       in the following vlos.  The indexes in the arrays are the same as
       corresponding symbol, terminal, and nonterminal numbers.*/
    vlo_t symbs_vlo;
    vlo_t terms_vlo;
    vlo_t nonterms_vlo;

    /* The following are tables to find terminal by its code and symbol by
       its representation.*/
    hash_table_t repr_to_symb_tab;	/* key is `repr'*/
    hash_table_t code_to_symb_tab;	/* key is `code'*/

    /* If terminal symbol codes are not spared(in this case the member
       value is not NULL, we use translation vector instead of hash table. */
    YaepSymb **symb_code_trans_vect;
    int symb_code_trans_vect_start;
    int symb_code_trans_vect_end;
};

/* A set of terminals represented as a bit array. */
struct YaepTabTermSet
{
    // Set identity.
    int id;

    // Number of long ints (term_set_el_t) used to store the bit array.
    int num_elements;

    // The bit array itself.
    term_set_el_t *set;
};

/* The following container for the abstract data.*/
struct YaepTermStorage
{
    /* All terminal sets are stored in the following os.*/
    os_t term_set_os;

    /* The following variables can be read externally.  Their values are
       number of terminal sets and their overall size.*/
    int n_term_sets, n_term_sets_size;

    /* The following is hash table of terminal sets(key is member
       `set').*/
    hash_table_t term_set_tab;

    /* References to all structure tab_term_set are stored in the
       following vlo.*/
    vlo_t tab_term_set_vlo;
};

/* This page contains table for fast search for vector of indexes of
   situations with symbol after dot in given set core.*/
struct YaepVect
{
    /* The following member is used internally.  The value is
       nonnegative for core_symb_vect being formed.  It is index of vlo
       in vlos array which contains the vector elements.*/
    int intern;
    /* The following memebers defines array of indexes of situations in
       given set core.  You should access to values through these
       members(in other words don't save the member values in another
       variable).*/
    int len;
    int*els;
};

/* The following is element of the table.*/
struct YaepCoreSymbVect
{
    /* The set core.*/
    YaepSetCore*set_core;
    /* The symbol.*/
    YaepSymb*symb;
    /* The following vector contains indexes of situations with given
       symb in situation after dot.*/
    YaepVect transitions;
    /* The following vector contains indexes of reduce situations with
       given symb in lhs.*/
    YaepVect reduces;
};

/* The following is set in Earley's algorithm without distance
   information.  Because there are many duplications of such
   structures we extract the set cores and store them in one
   exemplar.*/
struct YaepSetCore
{
    /* The following is unique number of the set core. It is defined
       only after forming all set.*/
    int num;
    /* The set core hash.  We save it as it is used several times. */
    unsigned int hash;
    /* The following is term shifting which resulted into this core.  It
       is defined only after forming all set.*/
    YaepSymb*term;
    /* The following are numbers of all situations and start situations
       in the following array.*/
    int n_sits;
    int n_start_sits;
    /* Array of situations.  Start situations are always placed the
       first in the order of their creation(with subsequent duplicates
       are removed), then nonstart noninitial(situation with at least
       one symbol before the dot) situations are placed and then initial
       situations are placed.  You should access to a set situation only
       through this member or variable `new_sits'(in other words don't
       save the member value in another variable).*/
    YaepSituation**sits;
    /* The following member is number of start situations and nonstart
      (noninitial) situations whose distance is defined from a start
       situation distance.  All nonstart initial situations have zero
       distances.  This distances are not stored. */
    int n_all_dists;
    /* The following is array containing number of start situation from
       which distance of(nonstart noninitial) situation with given
       index(between n_start_situations -> n_all_dists) is taken.*/
    int*parent_indexes;
};

/* The following describes set in Earley's algorithm.*/
struct YaepSet
{
    /* The following is set core of the set.  You should access to set
       core only through this member or variable `new_core'(in other
       words don't save the member value in another variable).*/
    YaepSetCore *core;
    /* Hash of the set distances.  We save it as it is used several
       times. */
    unsigned int dists_hash;
    /* The following is distances only for start situations.  Other
       situations have their distances equal to 0.  The start situation
       in set core and the corresponding distance has the same index.
       You should access to distances only through this member or
       variable `new_dists'(in other words don't save the member value
       in another variable).*/
    int*dists;
};

/* The following describes abstract data situation without distance of its original
   set.  To save memory we extract such structure because there are
   many duplicated structures. */
struct YaepSituation
{
    /* The following is the situation rule. */
    YaepRule*rule;
    /* The following is position of dot in rhs of the situation rule. */
    short pos;
    /* The following member is TRUE if the tail can derive empty string. */
    char empty_tail_p;
    /* unique situation number. */
    int sit_number;
    /* The following is number of situation context which is number of
       the corresponding terminal set in the table.  It is really used
       only for dynamic lookahead. */
    int context;
    /* The following member is the situation lookahead it is equal to
       FIRST(the situation tail || FOLLOW(lhs)) for statik lookaheads
       and FIRST(the situation tail || context) for dynamic ones. */
    term_set_el_t *lookahead;
};

/* The following describes input token.*/
struct YaepTok
{
    /* The following is symb correseponding to the token.*/
    YaepSymb*symb;
    /* The following is an attribute of the token.*/
    void*attr;
};

/* The triple and possible goto sets for it. */
struct YaepSetTermLookAhead
{
    YaepSet*set;
    YaepSymb*term;
    int lookahead;
    /* Saved goto sets form a queue.  The last goto is saved at the
       following array elements whose index is given by CURR. */
    int curr;
    /* Saved goto sets to which we can go from SET by the terminal with
       subsequent terminal LOOKAHEAD given by its code. */
    YaepSet*result[MAX_CACHED_GOTO_RESULTS];
    /* Corresponding places of the goto sets in the parsing list. */
    int place[MAX_CACHED_GOTO_RESULTS];
};

/* The following describes rule of grammar.*/
struct YaepRule
{
    /* The following is order number of rule.*/
    int num;
    /* The following is length of rhs.*/
    int rhs_len;
    /* The following is the next grammar rule.*/
    YaepRule*next;
    /* The following is the next grammar rule with the same nonterminal
       in lhs of the rule.*/
    YaepRule*lhs_next;
    /* The following is nonterminal in the left hand side of the
       rule.*/
    YaepSymb*lhs;
    /* The ixml default mark of the rule*/
    char mark;
    /* The following is symbols in the right hand side of the rule.*/
    YaepSymb**rhs;
    /* The ixml marks for all the terms in the right hand side of the rule.*/
    char*marks;
    /* The following three members define rule translation.*/
    const char*anode;		/* abstract node name if any.*/
    int anode_cost;		/* the cost of the abstract node if any, otherwise 0.*/
    int trans_len;		/* number of symbol translations in the rule translation.*/
    /* The following array elements correspond to element of rhs with
       the same index.  The element value is order number of the
       corresponding symbol translation in the rule translation.  If the
       symbol translation is rejected, the corresponding element value is
       negative.*/
    int*order;
    /* The following member value is equal to size of all previous rule
       lengths + number of the previous rules.  Imagine that all left
       hand symbol and right hand size symbols of the rules are stored
       in array.  Then the following member is index of the rule lhs in
       the array.*/
    int rule_start_offset;
    /* The following is the same string as anode but memory allocated in
       parse_alloc.*/
    char*caller_anode;
};

/* The following container for the abstract data.*/
struct YaepRuleStorage
{
    /* The following is number of all rules and their summary rhs
       length.  The variables can be read externally.*/
    int n_rules, n_rhs_lens;

    /* The following is the first rule.*/
    YaepRule*first_rule;

    /* The following is rule being formed.  It can be read
       externally.*/
    YaepRule*curr_rule;

    /* All rules are placed in the following object.*/
    os_t rules_os;
};

/* The following describes parser state.*/
struct YaepInternalParseState
{
    /* The rule which we are processing.*/
    YaepRule *rule;
    /* Position in the rule where we are now.*/
    int pos;
    /* The rule origin(start point of derivated string from rule rhs)
       and parser list in which we are now.*/
    int orig, pl_ind;
    /* If the following value is NULL, then we do not need to create
       translation for this rule.  If we should create abstract node
       for this rule, the value refers for the abstract node and the
       displacement is undefined.  Otherwise, the two members is
       place into which we should place the translation of the rule.
       The following member is used only for states in the stack.*/
    YaepInternalParseState *parent_anode_state;
    int parent_disp;
    /* The following is used only for states in the table.*/
    YaepTreeNode *anode;
};

/* To make better traversing and don't waist tree parse memory,
   we use the following structures to enumerate the tree node.*/
struct YaepTransVisitNode
{
    /* The following member is order number of the node.  This value is
       negative if we did not visit the node yet.*/
    int num;
    /* The tree node itself.*/
    YaepTreeNode*node;
};

struct YaepParseState
{
    YaepParseRun run;
    int magic_cookie; // Must be set to 736268273 when the state is created.

    /* The following says that new_set, new_core and their members are
       defined. Before this the access to data of the set being formed
       are possible only through the following variables. */
    int new_set_ready_p;

    /* The following variable is set being created. It is defined only when new_set_ready_p is TRUE. */
    YaepSet *new_set;

   /* The following variable is always set core of set being created.  It
      can be read externally.  Member core of new_set has always the
      following value.  It is defined only when new_set_ready_p is TRUE. */
    YaepSetCore *new_core;

   /* To optimize code we use the following variables to access to data
      of new set.  They are always defined and correspondingly
      situations, distances, and the current number of start situations
      of the set being formed.*/
    YaepSituation **new_sits;
    int *new_dists;
    int new_n_start_sits;

    /* The following are number of unique set cores and their start
       situations, unique distance vectors and their summary length, and
       number of parent indexes.  The variables can be read externally.*/
    int n_set_cores, n_set_core_start_sits;
    int n_set_dists, n_set_dists_len, n_parent_indexes;

    /* Number unique sets and their start situations. */
    int n_sets, n_sets_start_sits;

    /* Number unique triples(set, term, lookahead). */
    int n_set_term_lookaheads;

    /* The set cores of formed sets are placed in the following os.*/
    os_t set_cores_os;

    /* The situations of formed sets are placed in the following os.*/
    os_t set_sits_os;

    /* The indexes of the parent start situations whose distances are used
       to get distances of some nonstart situations are placed in the
       following os.*/
    os_t set_parent_indexes_os;

    /* The distances of formed sets are placed in the following os.*/
    os_t set_dists_os;

    /* The sets themself are placed in the following os.*/
    os_t sets_os;

    /* Container for triples(set, term, lookahead. */
    os_t set_term_lookahead_os;

    /* The following 3 tables contain references for sets which refers
       for set cores or distances or both which are in the tables.*/
    hash_table_t set_core_tab;	/* key is only start situations.*/
    hash_table_t set_dists_tab;	/* key is distances.*/
    hash_table_t set_tab;	/* key is(core, distances).*/

    /* Table for triples(set, term, lookahead). */
    hash_table_t set_term_lookahead_tab;	/* key is(core, distances, lookeahed).*/

    /* The following two variables contains all input tokens and their
       number.  The variables can be read externally.*/
    YaepTok *toks;
    int toks_len;
    int tok_curr;

    /* The following array contains all input tokens.*/
    vlo_t toks_vlo;

    /* The following contains current number of unique situations.  It can
       be read externally.*/
    int n_all_sits;

    /* The following two dimensional array(the first dimension is context
       number, the second one is situation number) contains references to
       all possible situations.*/
    YaepSituation ***sit_table;

    /* The following vlo is indexed by situation context number and gives
       array which is indexed by situation number
      (sit->rule->rule_start_offset + sit->pos).*/
    vlo_t sit_table_vlo;

    /* All situations are placed in the following object.*/
    os_t sits_os;

    /* Vector implementing map: sit number -> vlo of the distance check
       indexed by the distance. */
    vlo_t sit_dist_vec_vlo;

    /* The value used to check the validity of elements of check_dist
       structures. */
    int curr_sit_dist_vec_check;

    /* The following are number of unique(set core, symbol) pairs and
       their summary(transitive) transition and reduce vectors length,
       unique(transitive) transition vectors and their summary length,
       and unique reduce vectors and their summary length.  The variables
       can be read externally. */
    int n_core_symb_pairs, n_core_symb_vect_len;
    int n_transition_vects, n_transition_vect_len;
    int n_reduce_vects, n_reduce_vect_len;

    /* All triples(set core, symbol, vect) are placed in the following object. */
    os_t core_symb_vect_os;

    /* Pointers to triples(set core, symbol, vect) being formed are
       placed in the following object. */
    vlo_t new_core_symb_vect_vlo;

    /* All elements of vectors in the table(see
       (transitive_)transition_els_tab and reduce_els_tab) are placed in
       the following os. */
    os_t vect_els_os;

#ifdef USE_CORE_SYMB_HASH_TABLE
    hash_table_t core_symb_to_vect_tab;	/* key is set_core and symb.*/
#else
    /* The following two variables contains table(set core,
       symbol)->core_symb_vect implemented as two dimensional array.*/
    /* The following object contains pointers to the table rows for each
       set core.*/
    vlo_t core_symb_table_vlo;

    /* The following is always start of the previous object.*/
    YaepCoreSymbVect ***core_symb_table;

    /* The following contains rows of the table.  The element in the rows
       are indexed by symbol number.*/
    os_t core_symb_tab_rows;
#endif

    /* The following tables contains references for core_symb_vect which
       (through(transitive) transitions and reduces correspondingly)
       refers for elements which are in the tables.  Sequence elements are
       stored in one exemplar to save memory.*/
    hash_table_t transition_els_tab;	/* key is elements.*/
    hash_table_t reduce_els_tab;	/* key is elements.*/

    /* The following two variables represents Earley's parser list.  The
       values of pl_curr and array*pl can be read and modified
       externally.*/
    YaepSet **pl;
    int pl_curr;

    /* The following is number of created terminal, abstract, and
       alternative nodes.*/
    int n_parse_term_nodes, n_parse_abstract_nodes, n_parse_alt_nodes;

    /* All tail sets of error recovery are saved in the following os.*/
    os_t recovery_state_tail_sets;

    /* The following variable values is pl_curr and tok_curr at error
       recovery start(when the original syntax error has been fixed).*/
    int start_pl_curr, start_tok_curr;

    /* The following variable value means that all error sets in pl with
       indexes [back_pl_frontier, start_pl_curr] are being processed or
       have been processed.*/
    int back_pl_frontier;

    /* The following variable stores original pl tail in reversed order.
       This object only grows.  The last object sets may be used to
       restore original pl in order to try another error recovery state
       (alternative).*/
    vlo_t original_pl_tail_stack;

    /* The following variable value is last pl element which is original
       set(set before the error_recovery start).*/
    int original_last_pl_el;

    /// GURKA

    /* This page contains code for work with array of vlos.  It is used
       only to implement abstract data `core_symb_vect'.*/

    /* All vlos being formed are placed in the following object.*/
    vlo_t vlo_array;

    /* The following is current number of elements in vlo_array.*/
    int vlo_array_len;

    /* The following table is used to find allocated memory which should not be freed.*/
    hash_table_t reserv_mem_tab;

    /* The following vlo will contain references to memory which should be
       freed.  The same reference can be represented more on time.*/
    vlo_t tnodes_vlo;

    /* The key of the following table is node itself.*/
    hash_table_t trans_visit_nodes_tab;

    /* All translation visit nodes are placed in the following stack.  All
       the nodes are in the table.*/
    os_t trans_visit_nodes_os;

    /* The following value is number of translation visit nodes.*/
    int n_trans_visit_nodes;

    /* How many times we reuse Earley's sets without their recalculation. */
    int n_goto_successes;

    /* The following vlo is error recovery states stack.  The stack
       contains error recovery state which should be investigated to find
       the best error recovery.*/
    vlo_t recovery_state_stack;

    /* The following os contains all allocated parser states.*/
    os_t parse_state_os;

    /* The following variable refers to head of chain of already allocated
       and then freed parser states.*/
    YaepInternalParseState *free_parse_state;

    /* The following table is used to make translation for ambiguous
       grammar more compact.  It is used only when we want all
       translations.*/
    hash_table_t parse_state_tab;	/* Key is rule, orig, pl_ind.*/
};
typedef struct YaepParseState YaepParseState;

#define CHECK_PARSE_STATE_MAGIC(ps) (ps->magic_cookie == 736268273)
#define INSTALL_PARSE_STATE_MAGIC(ps) ps->magic_cookie=736268273

// Declarations ///////////////////////////////////////////////////

static void read_toks(YaepParseState *ps);
static void print_yaep_node(YaepParseState *ps, FILE *f, YaepTreeNode *node);
static void rule_dot_print(YaepParseState *ps, FILE *f, YaepRule *rule, int pos);
static void rule_print(YaepParseState *ps, FILE *f, YaepRule *rule, int trans_p);
static void set_print(YaepParseState *ps, FILE* f, YaepSet*set, int set_dist, int nonstart_p, int lookahead_p);
static void sit_print(YaepParseState *ps, FILE *f, YaepSituation *sit, int lookahead_p);
static YaepVocabulary *symb_init(YaepGrammar *g);
static void symb_empty(YaepParseState *ps, YaepVocabulary *symbs);
static void symb_finish_adding_terms(YaepParseState *ps);
static void symb_print(FILE* f, YaepSymb*symb, int code_p);
static void yaep_error(YaepParseState *ps, int code, const char*format, ...);

// Global variables /////////////////////////////////////////////////////

/* Jump buffer for processing errors.*/
jmp_buf error_longjump_buff;

// Implementations ////////////////////////////////////////////////////////////////////

/* Hash of symbol representation. */
static unsigned symb_repr_hash(hash_table_entry_t s)
{
    unsigned result = jauquet_prime_mod32;
    const char*str =((YaepSymb*) s)->repr;
    int i;

    for(i = 0; str[i] != '\0'; i++)
    {
        result = result* hash_shift +(unsigned) str[i];
    }

    TRACE_FA("%s -> %u", str, result);
    return result;
}

/* Equality of symbol representations. */
static int symb_repr_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    TRACE_FA("%s %s",((YaepSymb*) s1)->repr,((YaepSymb*) s2)->repr);

    return strcmp(((YaepSymb*) s1)->repr,((YaepSymb*) s2)->repr) == 0;
}

/* Hash of terminal code. */
static unsigned symb_code_hash(hash_table_entry_t s)
{
    YaepSymb*symb =((YaepSymb*) s);

    assert(symb->term_p);
    TRACE_FA("%d", symb->u.term.code);

    return symb->u.term.code;
}

/* Equality of terminal codes.*/
static int symb_code_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepSymb*symb1 =((YaepSymb*) s1);
    YaepSymb*symb2 =((YaepSymb*) s2);

    assert(symb1->term_p && symb2->term_p);
    TRACE_FA("%d %d", symb1->u.term.code, symb2->u.term.code);

    return symb1->u.term.code == symb2->u.term.code;
}

/* Initialize work with symbols and returns storage for the symbols.*/
static YaepVocabulary *symb_init(YaepGrammar *grammar)
{
    void*mem;
    YaepVocabulary*result;

    mem = yaep_malloc(grammar->alloc, sizeof(YaepVocabulary));
    result =(YaepVocabulary*) mem;
    OS_CREATE(result->symbs_os, grammar->alloc, 0);
    VLO_CREATE(result->symbs_vlo, grammar->alloc, 1024);
    VLO_CREATE(result->terms_vlo, grammar->alloc, 512);
    VLO_CREATE(result->nonterms_vlo, grammar->alloc, 512);
    result->repr_to_symb_tab = create_hash_table(grammar->alloc, 300, symb_repr_hash, symb_repr_eq);
    result->code_to_symb_tab = create_hash_table(grammar->alloc, 200, symb_code_hash, symb_code_eq);
    result->symb_code_trans_vect = NULL;
    result->n_nonterms = result->nn_terms = 0;

    TRACE_F;

    return result;
}

/* Return symbol(or NULL if it does not exist) whose representation is REPR.*/
static YaepSymb *symb_find_by_repr(YaepParseState *ps, const char*repr)
{
    YaepSymb symb;
    symb.repr = repr;
    YaepSymb*r = (YaepSymb*)*find_hash_table_entry(ps->run.grammar->symbs_ptr->repr_to_symb_tab, &symb, FALSE);

    TRACE_FA("%s -> %p", repr, r);

    return r;
}

/* Return symbol(or NULL if it does not exist) which is terminal with CODE. */
static YaepSymb *symb_find_by_code(YaepParseState *ps, int code)
{
    YaepSymb symb;

    if (ps->run.grammar->symbs_ptr->symb_code_trans_vect != NULL)
    {
        if ((code < ps->run.grammar->symbs_ptr->symb_code_trans_vect_start) ||(code >= ps->run.grammar->symbs_ptr->symb_code_trans_vect_end))
        {
            TRACE_FA("vec %d -> NULL", code);
            return NULL;
        }
        else
        {
            YaepSymb*r = ps->run.grammar->symbs_ptr->symb_code_trans_vect[code - ps->run.grammar->symbs_ptr->symb_code_trans_vect_start];
            TRACE_FA("vec %d -> %p", code, r);
            return r;
        }
    }

    symb.term_p = TRUE;
    symb.u.term.code = code;
    YaepSymb*r =(YaepSymb*)*find_hash_table_entry(ps->run.grammar->symbs_ptr->code_to_symb_tab, &symb, FALSE);

    TRACE_FA("hash %d -> %p", code, r);

    return r;
}

/* The function creates new terminal symbol and returns reference for
   it.  The symbol should be not in the tables.  The function should
   create own copy of name for the new symbol. */
static YaepSymb *symb_add_term(YaepParseState *ps, const char*name, int code)
{
    YaepSymb symb, *result;
    hash_table_entry_t *repr_entry, *code_entry;

    symb.repr = name;
    symb.term_p = TRUE;
    symb.num = ps->run.grammar->symbs_ptr->n_nonterms + ps->run.grammar->symbs_ptr->nn_terms;
    symb.u.term.code = code;
    symb.u.term.term_num = ps->run.grammar->symbs_ptr->nn_terms++;
    symb.empty_p = FALSE;
    repr_entry = find_hash_table_entry(ps->run.grammar->symbs_ptr->repr_to_symb_tab, &symb, TRUE);
    assert(*repr_entry == NULL);
    code_entry = find_hash_table_entry(ps->run.grammar->symbs_ptr->code_to_symb_tab, &symb, TRUE);
    assert(*code_entry == NULL);
    OS_TOP_ADD_STRING(ps->run.grammar->symbs_ptr->symbs_os, name);
    symb.repr =(char*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_os, &symb, sizeof(YaepSymb));
    result =(YaepSymb*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
   *repr_entry =(hash_table_entry_t) result;
   *code_entry =(hash_table_entry_t) result;
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_vlo, &result, sizeof(YaepSymb*));
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->terms_vlo, &result, sizeof(YaepSymb*));

    TRACE_FA("%s %d -> %p", name, code, result);

    return result;
}

/* The function creates new nonterminal symbol and returns reference
   for it.  The symbol should be not in the table. The function
   should create own copy of name for the new symbol. */
static YaepSymb *symb_add_nonterm(YaepParseState *ps, const char *name)
{
    YaepSymb symb,*result;
    hash_table_entry_t*entry;

    symb.repr = name;
    symb.term_p = FALSE;
    symb.num = ps->run.grammar->symbs_ptr->n_nonterms + ps->run.grammar->symbs_ptr->nn_terms;
    symb.u.nonterm.rules = NULL;
    symb.u.nonterm.loop_p = 0;
    symb.u.nonterm.nonterm_num = ps->run.grammar->symbs_ptr->n_nonterms++;
    entry = find_hash_table_entry(ps->run.grammar->symbs_ptr->repr_to_symb_tab, &symb, TRUE);
    assert(*entry == NULL);
    OS_TOP_ADD_STRING(ps->run.grammar->symbs_ptr->symbs_os, name);
    symb.repr =(char*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_os, &symb, sizeof(YaepSymb));
    result =(YaepSymb*) OS_TOP_BEGIN(ps->run.grammar->symbs_ptr->symbs_os);
    OS_TOP_FINISH(ps->run.grammar->symbs_ptr->symbs_os);
   *entry =(hash_table_entry_t) result;
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->symbs_vlo, &result, sizeof(YaepSymb*));
    VLO_ADD_MEMORY(ps->run.grammar->symbs_ptr->nonterms_vlo, &result, sizeof(YaepSymb*));

    TRACE_FA("%s -> %p", name, result);

    return result;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
static YaepSymb *symb_get(YaepParseState *ps, int n)
{
    if (n < 0 ||(VLO_LENGTH(ps->run.grammar->symbs_ptr->symbs_vlo) / sizeof(YaepSymb*) <=(size_t) n))
    {
        return NULL;
    }
    YaepSymb*symb =((YaepSymb**) VLO_BEGIN(ps->run.grammar->symbs_ptr->symbs_vlo))[n];
    assert(symb->num == n);

    TRACE_FA("%d -> %p", n, symb);

    return symb;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
static YaepSymb *term_get(YaepParseState *ps, int n)
{
    if (n < 0 || (VLO_LENGTH(ps->run.grammar->symbs_ptr->terms_vlo) / sizeof(YaepSymb*) <=(size_t) n))
    {
        return NULL;
    }
    YaepSymb*symb =((YaepSymb**) VLO_BEGIN(ps->run.grammar->symbs_ptr->terms_vlo))[n];
    assert(symb->term_p && symb->u.term.term_num == n);

    TRACE_FA("%d -> %p", n, symb);

    return symb;
}

/* The following function return N-th symbol(if any) or NULL otherwise. */
static YaepSymb *nonterm_get(YaepParseState *ps, int n)
{
    if (n < 0 ||(VLO_LENGTH(ps->run.grammar->symbs_ptr->nonterms_vlo) / sizeof(YaepSymb*) <=(size_t) n))
    {
        return NULL;
    }
    YaepSymb*symb =((YaepSymb**) VLO_BEGIN(ps->run.grammar->symbs_ptr->nonterms_vlo))[n];
    assert(!symb->term_p && symb->u.nonterm.nonterm_num == n);

    TRACE_FA("%d -> %p", n, symb);

    return symb;
}

static void symb_finish_adding_terms(YaepParseState *ps)
{
    int i, max_code, min_code;
    YaepSymb*symb;
    void*mem;

    for (min_code = max_code = i = 0;(symb = term_get(ps, i)) != NULL; i++)
    {
        if (i == 0 || min_code > symb->u.term.code) min_code = symb->u.term.code;
        if (i == 0 || max_code < symb->u.term.code) max_code = symb->u.term.code;
    }
    assert(i != 0);
    assert((max_code - min_code) < MAX_SYMB_CODE_TRANS_VECT_SIZE);

    ps->run.grammar->symbs_ptr->symb_code_trans_vect_start = min_code;
    ps->run.grammar->symbs_ptr->symb_code_trans_vect_end = max_code + 1;

    size_t num_codes = max_code - min_code + 1;
    size_t vec_size = sizeof(YaepSymb*)* num_codes;
    mem = yaep_malloc(ps->run.grammar->alloc, vec_size);

    ps->run.grammar->symbs_ptr->symb_code_trans_vect =(YaepSymb**)mem;

    for(i = 0;(symb = term_get(ps, i)) != NULL; i++)
    {
        ps->run.grammar->symbs_ptr->symb_code_trans_vect[symb->u.term.code - min_code] = symb;
    }

    TRACE_FA("num_codes=%zu size=%zu", num_codes, vec_size);
}

/* Free memory for symbols. */
static void symb_empty(YaepParseState *ps, YaepVocabulary *symbs)
{
    if (symbs == NULL) return;

    if (ps->run.grammar->symbs_ptr->symb_code_trans_vect != NULL)
    {
        yaep_free(ps->run.grammar->alloc, ps->run.grammar->symbs_ptr->symb_code_trans_vect);
        ps->run.grammar->symbs_ptr->symb_code_trans_vect = NULL;
    }

    empty_hash_table(symbs->repr_to_symb_tab);
    empty_hash_table(symbs->code_to_symb_tab);
    VLO_NULLIFY(symbs->nonterms_vlo);
    VLO_NULLIFY(symbs->terms_vlo);
    VLO_NULLIFY(symbs->symbs_vlo);
    OS_EMPTY(symbs->symbs_os);
    symbs->n_nonterms = symbs->nn_terms = 0;

    TRACE_FA("%p\n" , symbs);
}

/* Finalize work with symbols. */
static void symb_fin(YaepParseState *ps, YaepVocabulary *symbs)
{
    if (symbs == NULL) return;

    if (ps->run.grammar->symbs_ptr->symb_code_trans_vect != NULL)
    {
        yaep_free(ps->run.grammar->alloc, ps->run.grammar->symbs_ptr->symb_code_trans_vect);
    }

    delete_hash_table(ps->run.grammar->symbs_ptr->repr_to_symb_tab);
    delete_hash_table(ps->run.grammar->symbs_ptr->code_to_symb_tab);
    VLO_DELETE(ps->run.grammar->symbs_ptr->nonterms_vlo);
    VLO_DELETE(ps->run.grammar->symbs_ptr->terms_vlo);
    VLO_DELETE(ps->run.grammar->symbs_ptr->symbs_vlo);
    OS_DELETE(ps->run.grammar->symbs_ptr->symbs_os);
    yaep_free(ps->run.grammar->alloc, symbs);

    TRACE_FA("%p\n" , symbs);
}


/* Hash of table terminal set.*/
static unsigned term_set_hash(hash_table_entry_t s)
{
    YaepTabTermSet *ts = (YaepTabTermSet*)s;
    term_set_el_t *set = ts->set;
    int num_elements = ts->num_elements;
    term_set_el_t *bound = set + num_elements;
    unsigned result = jauquet_prime_mod32;

    while (set < bound)
    {
        result = result * hash_shift + *set++;
    }
    return result;
}

/* Equality of terminal sets. */
static int term_set_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepTabTermSet *ts1 = (YaepTabTermSet*)s1;
    YaepTabTermSet *ts2 = (YaepTabTermSet*)s2;
    term_set_el_t *i = ts1->set;
    term_set_el_t *j = ts2->set;

    assert(ts1->num_elements == ts2->num_elements);

    int num_elements = ts1->num_elements;
    term_set_el_t *i_bound = i + num_elements;

    while (i < i_bound)
    {
        if (*i++ != *j++)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* Initialize work with terminal sets and returns storage for terminal sets.*/
static YaepTermStorage *term_set_init(YaepGrammar *grammar)
{
    void*mem;
    YaepTermStorage*result;

    mem = yaep_malloc(grammar->alloc, sizeof(YaepTermStorage));
    result =(YaepTermStorage*) mem;
    OS_CREATE(result->term_set_os, grammar->alloc, 0);
    result->term_set_tab = create_hash_table(grammar->alloc, 1000, term_set_hash, term_set_eq);
    VLO_CREATE(result->tab_term_set_vlo, grammar->alloc, 4096);
    result->n_term_sets = result->n_term_sets_size = 0;

    return result;
}

/* Return new terminal SET.  Its value is undefined. */
static term_set_el_t *term_set_create(YaepParseState *ps, int num_terms)
{
    int size;
    term_set_el_t*result;

    assert(sizeof(term_set_el_t) <= 8);
    size = 8;
    /* Make it 64 bit multiple to have the same statistics for 64 bit
       machines. num_terms = global variable ps->run.grammar->symbs_ptr->n_terms*/
    size =((num_terms + CHAR_BIT* 8 - 1) /(CHAR_BIT* 8))* 8;
    OS_TOP_EXPAND(ps->run.grammar->term_sets_ptr->term_set_os, size);
    result =(term_set_el_t*) OS_TOP_BEGIN(ps->run.grammar->term_sets_ptr->term_set_os);
    OS_TOP_FINISH(ps->run.grammar->term_sets_ptr->term_set_os);
    ps->run.grammar->term_sets_ptr->n_term_sets++;
    ps->run.grammar->term_sets_ptr->n_term_sets_size += size;

    return result;
}

/* Make terminal SET empty.*/
static void term_set_clear(term_set_el_t* set, int num_terms)
{
    term_set_el_t*bound;
    int size;

    size =((num_terms + CHAR_BIT* sizeof(term_set_el_t) - 1)
            /(CHAR_BIT* sizeof(term_set_el_t)));
    bound = set + size;
    while(set < bound)
       *set++ = 0;
}

/* Copy SRC into DEST. */
static void term_set_copy(term_set_el_t *dest, term_set_el_t *src, int num_terms)
{
    term_set_el_t *bound;
    int size;

    size = ((num_terms + CHAR_BIT* sizeof(term_set_el_t) - 1) / (CHAR_BIT* sizeof(term_set_el_t)));
    bound = dest + size;

    while (dest < bound)
    {
       *dest++ = *src++;
    }
}

/* Add all terminals from set OP with to SET.  Return TRUE if SET has been changed.*/
static int term_set_or(term_set_el_t *set, term_set_el_t *op, int num_terms)
{
    term_set_el_t *bound;
    int size, changed_p;

    size = ((num_terms + CHAR_BIT* sizeof(term_set_el_t) - 1) / (CHAR_BIT* sizeof(term_set_el_t)));
    bound = set + size;
    changed_p = 0;
    while (set < bound)
    {
        if ((*set |*op) !=*set)
        {
            changed_p = 1;
        }
       *set++ |= *op++;
    }
    return changed_p;
}

/* Add terminal with number NUM to SET.  Return TRUE if SET has been changed.*/
static int term_set_up(term_set_el_t *set, int num, int num_terms)
{
    int ind, changed_p;
    term_set_el_t bit;

    assert(num < num_terms);

    ind = num / (CHAR_BIT* sizeof(term_set_el_t));
    bit = ((term_set_el_t) 1) << (num %(CHAR_BIT* sizeof(term_set_el_t)));
    changed_p =(set[ind] & bit ? 0 : 1);
    set[ind] |= bit;

    return changed_p;
}

/* Return TRUE if terminal with number NUM is in SET. */
static int term_set_test(term_set_el_t *set, int num, int num_terms)
{
    int ind;
    term_set_el_t bit;

    assert(num >= 0 && num < num_terms);

    ind = num /(CHAR_BIT* sizeof(term_set_el_t));
    bit = ((term_set_el_t) 1) << (num %(CHAR_BIT* sizeof(term_set_el_t)));

    return (set[ind] & bit) != 0;
}

/* The following function inserts terminal SET into the table and
   returns its number.  If the set is already in table it returns -its
   number - 1(which is always negative).  Don't set after
   insertion!!!*/
static int term_set_insert(YaepParseState *ps, term_set_el_t *set)
{
    hash_table_entry_t *entry;
    YaepTabTermSet tab_term_set,*tab_term_set_ptr;

    tab_term_set.set = set;
    entry = find_hash_table_entry(ps->run.grammar->term_sets_ptr->term_set_tab, &tab_term_set, TRUE);

    if (*entry != NULL)
    {
        return -((YaepTabTermSet*)*entry)->id - 1;
    }
    else
    {
        OS_TOP_EXPAND(ps->run.grammar->term_sets_ptr->term_set_os, sizeof(YaepTabTermSet));
        tab_term_set_ptr = (YaepTabTermSet*)OS_TOP_BEGIN(ps->run.grammar->term_sets_ptr->term_set_os);
        OS_TOP_FINISH(ps->run.grammar->term_sets_ptr->term_set_os);
       *entry =(hash_table_entry_t) tab_term_set_ptr;
        tab_term_set_ptr->set = set;
        tab_term_set_ptr->id = (VLO_LENGTH(ps->run.grammar->term_sets_ptr->tab_term_set_vlo) / sizeof(YaepTabTermSet*));
        tab_term_set_ptr->num_elements = CALC_NUM_ELEMENTS(ps->run.grammar->symbs_ptr->nn_terms);

        VLO_ADD_MEMORY(ps->run.grammar->term_sets_ptr->tab_term_set_vlo, &tab_term_set_ptr, sizeof(YaepTabTermSet*));

        return((YaepTabTermSet*)*entry)->id;
    }
}

/* The following function returns set which is in the table with number NUM.*/
static term_set_el_t *term_set_from_table(YaepParseState *ps, int num)
{
    assert(num >= 0);
    assert((long unsigned int)num < VLO_LENGTH(ps->run.grammar->term_sets_ptr->tab_term_set_vlo) / sizeof(YaepTabTermSet*));

    return ((YaepTabTermSet**)VLO_BEGIN(ps->run.grammar->term_sets_ptr->tab_term_set_vlo))[num]->set;
}

/* Print terminal SET into file F. */
static void term_set_print(YaepParseState *ps, FILE *f, term_set_el_t *set, int num_terms)
{
    int i;

    for (i = 0; i < num_terms; i++)
    {
        if (term_set_test(set, i, num_terms))
        {
            fprintf(f, " ");
            symb_print(f, term_get(ps, i), FALSE);
        }
    }
}

/* Free memory for terminal sets. */
static void term_set_empty(YaepTermStorage *term_sets)
{
    if (term_sets == NULL) return;

    VLO_NULLIFY(term_sets->tab_term_set_vlo);
    empty_hash_table(term_sets->term_set_tab);
    OS_EMPTY(term_sets->term_set_os);
    term_sets->n_term_sets = term_sets->n_term_sets_size = 0;
}

/* Finalize work with terminal sets. */
static void term_set_fin(YaepGrammar *grammar, YaepTermStorage *term_sets)
{
    if (term_sets == NULL) return;

    VLO_DELETE(term_sets->tab_term_set_vlo);
    delete_hash_table(term_sets->term_set_tab);
    OS_DELETE(term_sets->term_set_os);
    yaep_free(grammar->alloc, term_sets);
    term_sets = NULL;
}


/* Initialize work with rules and returns pointer to rules storage. */
static YaepRuleStorage *rule_init(YaepGrammar *grammar)
{
    void *mem;
    YaepRuleStorage *result;

    mem = yaep_malloc(grammar->alloc, sizeof(YaepRuleStorage));
    result = (YaepRuleStorage*)mem;
    OS_CREATE(result->rules_os, grammar->alloc, 0);
    result->first_rule = result->curr_rule = NULL;
    result->n_rules = result->n_rhs_lens = 0;

    return result;
}

/* Create new rule with LHS empty rhs. */
static YaepRule *rule_new_start(YaepParseState *ps, YaepSymb *lhs, const char *anode, int anode_cost)
{
    YaepRule *rule;
    YaepSymb *empty;

    assert(!lhs->term_p);

    OS_TOP_EXPAND(ps->run.grammar->rules_ptr->rules_os, sizeof(YaepRule));
    rule =(YaepRule*) OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
    OS_TOP_FINISH(ps->run.grammar->rules_ptr->rules_os);
    rule->lhs = lhs;
    if (anode == NULL)
    {
        rule->anode = NULL;
        rule->anode_cost = 0;
    }
    else
    {
        OS_TOP_ADD_STRING(ps->run.grammar->rules_ptr->rules_os, anode);
        rule->anode =(char*) OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
        OS_TOP_FINISH(ps->run.grammar->rules_ptr->rules_os);
        rule->anode_cost = anode_cost;
    }
    rule->trans_len = 0;
    rule->marks = NULL;
    rule->order = NULL;
    rule->next = NULL;
    if (ps->run.grammar->rules_ptr->curr_rule != NULL)
    {
        ps->run.grammar->rules_ptr->curr_rule->next = rule;
    }
    rule->lhs_next = lhs->u.nonterm.rules;
    lhs->u.nonterm.rules = rule;
    rule->rhs_len = 0;
    empty = NULL;
    OS_TOP_ADD_MEMORY(ps->run.grammar->rules_ptr->rules_os, &empty, sizeof(YaepSymb*));
    rule->rhs =(YaepSymb**) OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
    ps->run.grammar->rules_ptr->curr_rule = rule;
    if (ps->run.grammar->rules_ptr->first_rule == NULL)
    {
        ps->run.grammar->rules_ptr->first_rule = rule;
    }
    rule->rule_start_offset = ps->run.grammar->rules_ptr->n_rhs_lens + ps->run.grammar->rules_ptr->n_rules;
    rule->num = ps->run.grammar->rules_ptr->n_rules++;

    return rule;
}

/* Add SYMB at the end of current rule rhs. */
static void rule_new_symb_add(YaepParseState *ps, YaepSymb *symb)
{
    YaepSymb *empty;

    empty = NULL;
    OS_TOP_ADD_MEMORY(ps->run.grammar->rules_ptr->rules_os, &empty, sizeof(YaepSymb*));
    ps->run.grammar->rules_ptr->curr_rule->rhs = (YaepSymb**)OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
    ps->run.grammar->rules_ptr->curr_rule->rhs[ps->run.grammar->rules_ptr->curr_rule->rhs_len] = symb;
    ps->run.grammar->rules_ptr->curr_rule->rhs_len++;
    ps->run.grammar->rules_ptr->n_rhs_lens++;
}

/* The function should be called at end of forming each rule.  It
   creates and initializes situation cache.*/
static void rule_new_stop(YaepParseState *ps)
{
    int i;

    OS_TOP_FINISH(ps->run.grammar->rules_ptr->rules_os);
    OS_TOP_EXPAND(ps->run.grammar->rules_ptr->rules_os, ps->run.grammar->rules_ptr->curr_rule->rhs_len* sizeof(int));
    ps->run.grammar->rules_ptr->curr_rule->order = (int*)OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
    OS_TOP_FINISH(ps->run.grammar->rules_ptr->rules_os);
    for(i = 0; i < ps->run.grammar->rules_ptr->curr_rule->rhs_len; i++)
    {
        ps->run.grammar->rules_ptr->curr_rule->order[i] = -1;
    }

    OS_TOP_EXPAND(ps->run.grammar->rules_ptr->rules_os, ps->run.grammar->rules_ptr->curr_rule->rhs_len* sizeof(char));
    ps->run.grammar->rules_ptr->curr_rule->marks = (char*)OS_TOP_BEGIN(ps->run.grammar->rules_ptr->rules_os);
    OS_TOP_FINISH(ps->run.grammar->rules_ptr->rules_os);
}

/* The following function frees memory for rules.*/
static void rule_empty(YaepRuleStorage *rules)
{
    if (rules == NULL) return;

    OS_EMPTY(rules->rules_os);
    rules->first_rule = rules->curr_rule = NULL;
    rules->n_rules = rules->n_rhs_lens = 0;
}

/* Finalize work with rules.*/
static void rule_fin(YaepGrammar *grammar, YaepRuleStorage *rules)
{
    if (rules == NULL) return;

    OS_DELETE(rules->rules_os);
    yaep_free(grammar->alloc, rules);
    rules = NULL;
}

/* Initialize work with tokens.*/
static void tok_init(YaepParseState *ps)
{
    VLO_CREATE(ps->toks_vlo, ps->run.grammar->alloc, YAEP_INIT_TOKENS_NUMBER* sizeof(YaepTok));
    ps->toks_len = 0;
}

/* Add input token with CODE and attribute at the end of input tokens array.*/
static void tok_add(YaepParseState *ps, int code, void *attr)
{
    YaepTok tok;

    tok.attr = attr;
    tok.symb = symb_find_by_code(ps, code);
    if (tok.symb == NULL)
    {
        yaep_error(ps, YAEP_INVALID_TOKEN_CODE, "syntax error at offset %d '%c'", ps->toks_len, code);
    }
    VLO_ADD_MEMORY(ps->toks_vlo, &tok, sizeof(YaepTok));
    ps->toks = (YaepTok*)VLO_BEGIN(ps->toks_vlo);
    ps->toks_len++;
}

/* Finalize work with tokens. */
static void tok_fin(YaepParseState *ps)
{
    VLO_DELETE(ps->toks_vlo);
}

/* Initialize work with situations.*/
static void sit_init(YaepParseState *ps)
{
    ps->n_all_sits = 0;
    OS_CREATE(ps->sits_os, ps->run.grammar->alloc, 0);
    VLO_CREATE(ps->sit_table_vlo, ps->run.grammar->alloc, 4096);
    ps->sit_table = (YaepSituation***)VLO_BEGIN(ps->sit_table_vlo);
}

/* The following function sets up lookahead of situation SIT.  The
   function returns TRUE if the situation tail may derive empty
   string.*/
static int sit_set_lookahead(YaepParseState *ps, YaepSituation *sit)
{
    YaepSymb *symb, **symb_ptr;

    if (ps->run.grammar->lookahead_level == 0)
    {
        sit->lookahead = NULL;
    }
    else
    {
        sit->lookahead = term_set_create(ps, ps->run.grammar->symbs_ptr->nn_terms);
        term_set_clear(sit->lookahead, ps->run.grammar->symbs_ptr->nn_terms);
    }
    symb_ptr = &sit->rule->rhs[sit->pos];
    while ((symb =*symb_ptr) != NULL)
    {
        if (ps->run.grammar->lookahead_level != 0)
	{
            if (symb->term_p)
            {
                term_set_up(sit->lookahead, symb->u.term.term_num, ps->run.grammar->symbs_ptr->nn_terms);
            }
            else
            {
                term_set_or(sit->lookahead, symb->u.nonterm.first, ps->run.grammar->symbs_ptr->nn_terms);
            }
	}
        if (!symb->empty_p) break;
        symb_ptr++;
    }
    if (symb == NULL)
    {
        if (ps->run.grammar->lookahead_level == 1)
        {
            term_set_or(sit->lookahead, sit->rule->lhs->u.nonterm.follow, ps->run.grammar->symbs_ptr->nn_terms);
        }
        else if (ps->run.grammar->lookahead_level != 0)
        {
            term_set_or(sit->lookahead, term_set_from_table(ps, sit->context), ps->run.grammar->symbs_ptr->nn_terms);
        }
        return TRUE;
    }
    return FALSE;
}

/* The following function returns situations with given
   characteristics.  Remember that situations are stored in one
   exemplar.*/
static YaepSituation *sit_create(YaepParseState *ps, YaepRule *rule, int pos, int context)
{
    YaepSituation*sit;
    YaepSituation***context_sit_table_ptr;

    assert(context >= 0);
    context_sit_table_ptr = ps->sit_table + context;

    if ((char*) context_sit_table_ptr >=(char*) VLO_BOUND(ps->sit_table_vlo))
    {
        YaepSituation***bound,***ptr;
        int i, diff;

        assert((ps->run.grammar->lookahead_level <= 1 && context == 0) || (ps->run.grammar->lookahead_level > 1 && context >= 0));
        diff = (char*) context_sit_table_ptr -(char*) VLO_BOUND(ps->sit_table_vlo);
        diff += sizeof(YaepSituation**);
        if (ps->run.grammar->lookahead_level > 1 && diff == sizeof(YaepSituation**))
        {
            diff *= 10;
        }
        VLO_EXPAND(ps->sit_table_vlo, diff);
        ps->sit_table =(YaepSituation***) VLO_BEGIN(ps->sit_table_vlo);
        bound =(YaepSituation***) VLO_BOUND(ps->sit_table_vlo);
        context_sit_table_ptr = ps->sit_table + context;
        ptr = bound - diff / sizeof(YaepSituation**);
        while(ptr < bound)
	{
            OS_TOP_EXPAND(ps->sits_os,(ps->run.grammar->rules_ptr->n_rhs_lens + ps->run.grammar->rules_ptr->n_rules)
                          * sizeof(YaepSituation*));
           *ptr =(YaepSituation**) OS_TOP_BEGIN(ps->sits_os);
            OS_TOP_FINISH(ps->sits_os);
            for(i = 0; i < ps->run.grammar->rules_ptr->n_rhs_lens + ps->run.grammar->rules_ptr->n_rules; i++)
               (*ptr)[i] = NULL;
            ptr++;
	}
    }
    if ((sit = (*context_sit_table_ptr)[rule->rule_start_offset + pos]) != NULL)
    {
        return sit;
    }
    OS_TOP_EXPAND(ps->sits_os, sizeof(YaepSituation));
    sit =(YaepSituation*) OS_TOP_BEGIN(ps->sits_os);
    OS_TOP_FINISH(ps->sits_os);
    ps->n_all_sits++;
    sit->rule = rule;
    sit->pos = pos;
    sit->sit_number = ps->n_all_sits;
    sit->context = context;
    sit->empty_tail_p = sit_set_lookahead(ps, sit);
    (*context_sit_table_ptr)[rule->rule_start_offset + pos] = sit;

    return sit;
}


/* Return hash of sequence of N_SITS situations in array SITS. */
static unsigned sits_hash(int n_sits, YaepSituation **sits)
{
    int n, i;
    unsigned result;

    result = jauquet_prime_mod32;
    for(i = 0; i < n_sits; i++)
    {
        n = sits[i]->sit_number;
        result = result* hash_shift + n;
    }
    return result;
}

/* Finalize work with situations. */
static void sit_fin(YaepParseState *ps)
{
    VLO_DELETE(ps->sit_table_vlo);
    OS_DELETE(ps->sits_os);
}

/* Hash of set core. */
static unsigned set_core_hash(hash_table_entry_t s)
{
    return ((YaepSet*)s)->core->hash;
}

/* Equality of set cores. */
static int set_core_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepSetCore*set_core1 = ((YaepSet*) s1)->core;
    YaepSetCore*set_core2 = ((YaepSet*) s2)->core;
    YaepSituation **sit_ptr1, **sit_ptr2, **sit_bound1;

    if (set_core1->n_start_sits != set_core2->n_start_sits)
    {
        return FALSE;
    }
    sit_ptr1 = set_core1->sits;
    sit_bound1 = sit_ptr1 + set_core1->n_start_sits;
    sit_ptr2 = set_core2->sits;
    while(sit_ptr1 < sit_bound1)
    {
        if (*sit_ptr1++ !=*sit_ptr2++)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* Hash of set distances. */
static unsigned dists_hash(hash_table_entry_t s)
{
    return((YaepSet*) s)->dists_hash;
}

/* Equality of distances. */
static int dists_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    int *dists1 =((YaepSet*)s1)->dists;
    int *dists2 =((YaepSet*)s2)->dists;
    int n_dists =((YaepSet*)s1)->core->n_start_sits;
    int *bound;

    if (n_dists !=((YaepSet*)s2)->core->n_start_sits)
    {
        return FALSE;
    }
    bound = dists1 + n_dists;
    while (dists1 < bound)
    {
        if (*dists1++ !=*dists2++)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* Hash of set core and distances. */
static unsigned set_core_dists_hash(hash_table_entry_t s)
{
    return set_core_hash(s)* hash_shift + dists_hash(s);
}

/* Equality of set cores and distances. */
static int set_core_dists_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepSetCore *set_core1 = ((YaepSet*)s1)->core;
    YaepSetCore *set_core2 = ((YaepSet*)s2)->core;
    int*dists1 = ((YaepSet*)s1)->dists;
    int*dists2 = ((YaepSet*)s2)->dists;

    return set_core1 == set_core2 && dists1 == dists2;
}

/* Hash of triple(set, term, lookahead). */
static unsigned set_term_lookahead_hash(hash_table_entry_t s)
{
    YaepSet *set = ((YaepSetTermLookAhead*)s)->set;
    YaepSymb *term = ((YaepSetTermLookAhead*)s)->term;
    int lookahead = ((YaepSetTermLookAhead*)s)->lookahead;

    return ((set_core_dists_hash(set)* hash_shift + term->u.term.term_num)* hash_shift + lookahead);
}

/* Equality of tripes(set, term, lookahead).*/
static int set_term_lookahead_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepSet *set1 =((YaepSetTermLookAhead*)s1)->set;
    YaepSet *set2 =((YaepSetTermLookAhead*)s2)->set;
    YaepSymb *term1 =((YaepSetTermLookAhead*)s1)->term;
    YaepSymb *term2 =((YaepSetTermLookAhead*)s2)->term;
    int lookahead1 =((YaepSetTermLookAhead*)s1)->lookahead;
    int lookahead2 =((YaepSetTermLookAhead*)s2)->lookahead;

    return set1 == set2 && term1 == term2 && lookahead1 == lookahead2;
}

/* Initiate the set of pairs(sit, dist). */
static void sit_dist_set_init(YaepParseState *ps)
{
    VLO_CREATE(ps->sit_dist_vec_vlo, ps->run.grammar->alloc, 8192);
    ps->curr_sit_dist_vec_check = 0;
}

/* Make the set empty. */
static void empty_sit_dist_set(YaepParseState *ps)
{
    ps->curr_sit_dist_vec_check++;
}

/* Insert pair(SIT, DIST) into the set.  If such pair exists return
   FALSE, otherwise return TRUE. */
static int sit_dist_insert(YaepParseState *ps, YaepSituation*sit, int dist)
{
    int i, len, sit_number;
    vlo_t*check_dist_vlo;

    sit_number = sit->sit_number;
    /* Expand the set to accommodate possibly a new situation. */
    len = VLO_LENGTH(ps->sit_dist_vec_vlo) / sizeof(vlo_t);
    if (len <= sit_number)
    {
        VLO_EXPAND(ps->sit_dist_vec_vlo,(sit_number + 1 - len)* sizeof(vlo_t));
        for(i = len; i <= sit_number; i++)
            VLO_CREATE(((vlo_t*) VLO_BEGIN(ps->sit_dist_vec_vlo))[i],
                        ps->run.grammar->alloc, 64);
    }
    check_dist_vlo = &((vlo_t*) VLO_BEGIN(ps->sit_dist_vec_vlo))[sit_number];
    len = VLO_LENGTH(*check_dist_vlo) / sizeof(int);
    if (len <= dist)
    {
        VLO_EXPAND(*check_dist_vlo,(dist + 1 - len)* sizeof(int));
        for(i = len; i <= dist; i++)
           ((int*) VLO_BEGIN(*check_dist_vlo))[i] = 0;
    }
    if (((int*) VLO_BEGIN(*check_dist_vlo))[dist] == ps->curr_sit_dist_vec_check)
        return FALSE;
   ((int*) VLO_BEGIN(*check_dist_vlo))[dist] = ps->curr_sit_dist_vec_check;
    return TRUE;
}

/* Finish the set of pairs(sit, dist). */
static void sit_dist_set_fin(YaepParseState *ps)
{
    int i, len = VLO_LENGTH(ps->sit_dist_vec_vlo) / sizeof(vlo_t);

    for(i = 0; i < len; i++)
    {
        VLO_DELETE(((vlo_t*) VLO_BEGIN(ps->sit_dist_vec_vlo))[i]);
    }
    VLO_DELETE(ps->sit_dist_vec_vlo);
}

/* Initialize work with sets for parsing input with N_TOKS tokens.*/
static void set_init(YaepParseState *ps, int n_toks)
{
    int n = n_toks >> 3;

    OS_CREATE(ps->set_cores_os, ps->run.grammar->alloc, 0);
    OS_CREATE(ps->set_sits_os, ps->run.grammar->alloc, 2048);
    OS_CREATE(ps->set_parent_indexes_os, ps->run.grammar->alloc, 2048);
    OS_CREATE(ps->set_dists_os, ps->run.grammar->alloc, 2048);
    OS_CREATE(ps->sets_os, ps->run.grammar->alloc, 0);
    OS_CREATE(ps->set_term_lookahead_os, ps->run.grammar->alloc, 0);
    ps->set_core_tab = create_hash_table(ps->run.grammar->alloc, 2000, set_core_hash, set_core_eq);
    ps->set_dists_tab = create_hash_table(ps->run.grammar->alloc, n < 20000 ? 20000 : n, dists_hash, dists_eq);
    ps->set_tab = create_hash_table(ps->run.grammar->alloc, n < 20000 ? 20000 : n,
                                set_core_dists_hash, set_core_dists_eq);
    ps->set_term_lookahead_tab = create_hash_table(ps->run.grammar->alloc, n < 30000 ? 30000 : n,
                                               set_term_lookahead_hash, set_term_lookahead_eq);
    ps->n_set_cores = ps->n_set_core_start_sits = 0;
    ps->n_set_dists = ps->n_set_dists_len = ps->n_parent_indexes = 0;
    ps->n_sets = ps->n_sets_start_sits = 0;
    ps->n_set_term_lookaheads = 0;
    sit_dist_set_init(ps);
}

/* The following function starts forming of new set.*/
static void set_new_start(YaepParseState *ps)
{
    ps->new_set = NULL;
    ps->new_core = NULL;
    ps->new_set_ready_p = FALSE;
    ps->new_sits = NULL;
    ps->new_dists = NULL;
    ps->new_n_start_sits = 0;
}

/* Add start SIT with distance DIST at the end of the situation array
   of the set being formed.*/
static void set_new_add_start_sit(YaepParseState *ps, YaepSituation*sit, int dist)
{
    assert(!ps->new_set_ready_p);
    OS_TOP_EXPAND(ps->set_dists_os, sizeof(int));
    ps->new_dists =(int*) OS_TOP_BEGIN(ps->set_dists_os);
    OS_TOP_EXPAND(ps->set_sits_os, sizeof(YaepSituation*));
    ps->new_sits =(YaepSituation**) OS_TOP_BEGIN(ps->set_sits_os);
    ps->new_sits[ps->new_n_start_sits] = sit;
    ps->new_dists[ps->new_n_start_sits] = dist;
    ps->new_n_start_sits++;
}

/* Add nonstart, noninitial SIT with distance DIST at the end of the
   situation array of the set being formed.  If this is situation and
   there is already the same pair(situation, the corresponding
   distance), we do not add it.*/
static void set_add_new_nonstart_sit(YaepParseState *ps, YaepSituation*sit, int parent)
{
    int i;

    assert(ps->new_set_ready_p);
    /* When we add non-start situations we need to have pairs
      (situation, the corresponding distance) without duplicates
       because we also forms core_symb_vect at that time.*/
    for(i = ps->new_n_start_sits; i < ps->new_core->n_sits; i++)
    {
        if (ps->new_sits[i] == sit && ps->new_core->parent_indexes[i] == parent)
        {
            return;
        }
    }
    OS_TOP_EXPAND(ps->set_sits_os, sizeof(YaepSituation*));
    ps->new_sits = ps->new_core->sits =(YaepSituation**) OS_TOP_BEGIN(ps->set_sits_os);
    OS_TOP_EXPAND(ps->set_parent_indexes_os, sizeof(int));
    ps->new_core->parent_indexes = (int*)OS_TOP_BEGIN(ps->set_parent_indexes_os) - ps->new_n_start_sits;
    ps->new_sits[ps->new_core->n_sits++] = sit;
    ps->new_core->parent_indexes[ps->new_core->n_all_dists++] = parent;
    ps->n_parent_indexes++;
}

/* Add non-start(initial) SIT with zero distance at the end of the
   situation array of the set being formed.  If this is non-start
   situation and there is already the same pair(situation, zero
   distance), we do not add it.*/
static void set_new_add_initial_sit(YaepParseState *ps, YaepSituation*sit)
{
    int i;

    assert(ps->new_set_ready_p);
    /* When we add non-start situations we need to have pairs
      (situation, the corresponding distance) without duplicates
       because we also forms core_symb_vect at that time.*/
    for(i = ps->new_n_start_sits; i < ps->new_core->n_sits; i++)
        if (ps->new_sits[i] == sit)
            return;
    /* Remember we do not store distance for non-start situations.*/
    OS_TOP_ADD_MEMORY(ps->set_sits_os, &sit, sizeof(YaepSituation*));
    ps->new_sits = ps->new_core->sits =(YaepSituation**) OS_TOP_BEGIN(ps->set_sits_os);
    ps->new_core->n_sits++;
}

/* Set up hash of distances of set S.*/
static void setup_set_dists_hash(hash_table_entry_t s)
{
    YaepSet*set =((YaepSet*) s);
    int*dist_ptr = set->dists;
    int n_dists = set->core->n_start_sits;
    int*dist_bound;
    unsigned result;

    dist_bound = dist_ptr + n_dists;
    result = jauquet_prime_mod32;
    while(dist_ptr < dist_bound)
        result = result* hash_shift +*dist_ptr++;
    set->dists_hash = result;
}

/* Set up hash of core of set S.*/
static void setup_set_core_hash(hash_table_entry_t s)
{
    YaepSetCore*set_core =((YaepSet*) s)->core;

    set_core->hash = sits_hash(set_core->n_start_sits, set_core->sits);
}

/* The new set should contain only start situations.  Sort situations,
   remove duplicates and insert set into the set table.  If the
   function returns TRUE then set contains new set core(there was no
   such core in the table).*/
static int set_insert(YaepParseState *ps)
{
    hash_table_entry_t*entry;
    int result;

    OS_TOP_EXPAND(ps->sets_os, sizeof(YaepSet));
    ps->new_set = (YaepSet*)OS_TOP_BEGIN(ps->sets_os);
    ps->new_set->dists = ps->new_dists;
    OS_TOP_EXPAND(ps->set_cores_os, sizeof(YaepSetCore));
    ps->new_set->core = ps->new_core = (YaepSetCore*) OS_TOP_BEGIN(ps->set_cores_os);
    ps->new_core->n_start_sits = ps->new_n_start_sits;
    ps->new_core->sits = ps->new_sits;
    ps->new_set_ready_p = TRUE;
#ifdef USE_SET_HASH_TABLE
    /* Insert dists into table.*/
    setup_set_dists_hash(ps->new_set);
    entry = find_hash_table_entry(ps->set_dists_tab, ps->new_set, TRUE);
    if (*entry != NULL)
    {
        ps->new_dists = ps->new_set->dists =((YaepSet*)*entry)->dists;
        OS_TOP_NULLIFY(ps->set_dists_os);
    }
    else
    {
        OS_TOP_FINISH(ps->set_dists_os);
       *entry =(hash_table_entry_t)ps->new_set;
        ps->n_set_dists++;
        ps->n_set_dists_len += ps->new_n_start_sits;
    }
#else
    OS_TOP_FINISH(ps->set_dists_os);
    ps->n_set_dists++;
    ps->n_set_dists_len += ps->new_n_start_sits;
#endif
    /* Insert set core into table.*/
    setup_set_core_hash(ps->new_set);
    entry = find_hash_table_entry(ps->set_core_tab, ps->new_set, TRUE);
    if (*entry != NULL)
    {
        OS_TOP_NULLIFY(ps->set_cores_os);
        ps->new_set->core = ps->new_core = ((YaepSet*)*entry)->core;
        ps->new_sits = ps->new_core->sits;
        OS_TOP_NULLIFY(ps->set_sits_os);
        result = FALSE;
    }
    else
    {
        OS_TOP_FINISH(ps->set_cores_os);
        ps->new_core->num = ps->n_set_cores++;
        ps->new_core->n_sits = ps->new_n_start_sits;
        ps->new_core->n_all_dists = ps->new_n_start_sits;
        ps->new_core->parent_indexes = NULL;
       *entry =(hash_table_entry_t)ps->new_set;
        ps->n_set_core_start_sits += ps->new_n_start_sits;
        result = TRUE;
    }
#ifdef USE_SET_HASH_TABLE
    /* Insert set into table.*/
    entry = find_hash_table_entry(ps->set_tab, ps->new_set, TRUE);
    if (*entry == NULL)
    {
       *entry =(hash_table_entry_t)ps->new_set;
        ps->n_sets++;
        ps->n_sets_start_sits += ps->new_n_start_sits;
        OS_TOP_FINISH(ps->sets_os);
    }
    else
    {
        ps->new_set = (YaepSet*)*entry;
        OS_TOP_NULLIFY(ps->sets_os);
    }
#else
    OS_TOP_FINISH(ps->sets_os);
#endif
    return result;
}

/* The following function finishes work with set being formed.*/
static void set_new_core_stop(YaepParseState *ps)
{
    OS_TOP_FINISH(ps->set_sits_os);
    OS_TOP_FINISH(ps->set_parent_indexes_os);
}


/* Finalize work with sets.*/
static void set_fin(YaepParseState *ps)
{
    sit_dist_set_fin(ps);
    delete_hash_table(ps->set_term_lookahead_tab);
    delete_hash_table(ps->set_tab);
    delete_hash_table(ps->set_dists_tab);
    delete_hash_table(ps->set_core_tab);
    OS_DELETE(ps->set_term_lookahead_os);
    OS_DELETE(ps->sets_os);
    OS_DELETE(ps->set_parent_indexes_os);
    OS_DELETE(ps->set_sits_os);
    OS_DELETE(ps->set_dists_os);
    OS_DELETE(ps->set_cores_os);
}

/* Initialize work with the parser list.*/
static void pl_init(YaepParseState *ps)
{
    ps->pl = NULL;
}

/* The following function creates Earley's parser list.*/
static void pl_create(YaepParseState *ps)
{
    /* Because of error recovery we may have sets 2 times more than tokens.*/
    void *mem = yaep_malloc(ps->run.grammar->alloc, sizeof(YaepSet*)*(ps->toks_len + 1)* 2);
    ps->pl = (YaepSet**)mem;
    ps->pl_curr = -1;
}

/* Finalize work with the parser list.*/
static void pl_fin(YaepParseState *ps)
{
    if (ps->pl != NULL)
    {
        yaep_free(ps->run.grammar->alloc, ps->pl);
    }
}

/* Initialize work with array of vlos.*/
static void vlo_array_init(YaepParseState *ps)
{
    VLO_CREATE(ps->vlo_array, ps->run.grammar->alloc, 4096);
    ps->vlo_array_len = 0;
}

/* The function forms new empty vlo at the end of the array of vlos.*/
static int vlo_array_expand(YaepParseState *ps)
{
    vlo_t*vlo_ptr;

    if ((unsigned) ps->vlo_array_len >= VLO_LENGTH(ps->vlo_array) / sizeof(vlo_t))
    {
        VLO_EXPAND(ps->vlo_array, sizeof(vlo_t));
        vlo_ptr = &((vlo_t*) VLO_BEGIN(ps->vlo_array))[ps->vlo_array_len];
        VLO_CREATE(*vlo_ptr, ps->run.grammar->alloc, 64);
    }
    else
    {
        vlo_ptr = &((vlo_t*) VLO_BEGIN(ps->vlo_array))[ps->vlo_array_len];
        VLO_NULLIFY(*vlo_ptr);
    }
    return ps->vlo_array_len++;
}

/* The function purges the array of vlos.*/
static void vlo_array_nullify(YaepParseState *ps)
{
    ps->vlo_array_len = 0;
}

/* The following function returns pointer to vlo with INDEX.*/
static vlo_t *vlo_array_el(YaepParseState *ps, int index)
{
    assert(index >= 0 && ps->vlo_array_len > index);
    return &((vlo_t*) VLO_BEGIN(ps->vlo_array))[index];
}

/* Finalize work with array of vlos.*/
static void vlo_array_fin(YaepParseState *ps)
{
    vlo_t *vlo_ptr;

    for (vlo_ptr = (vlo_t*)VLO_BEGIN(ps->vlo_array); (char*) vlo_ptr < (char*) VLO_BOUND(ps->vlo_array); vlo_ptr++)
    {
        VLO_DELETE(*vlo_ptr);
    }
    VLO_DELETE(ps->vlo_array);
}

#ifdef USE_CORE_SYMB_HASH_TABLE
/* Hash of core_symb_vect.*/
static unsigned core_symb_vect_hash(hash_table_entry_t t)
{
    YaepCoreSymbVect*core_symb_vect =(YaepCoreSymbVect*) t;

    return((jauquet_prime_mod32* hash_shift
             +(unsigned) core_symb_vect->set_core)* hash_shift
            +(unsigned) core_symb_vect->symb);
}

/* Equality of core_symb_vects.*/
static int core_symb_vect_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    YaepCoreSymbVect*core_symb_vect1 =(YaepCoreSymbVect*) t1;
    YaepCoreSymbVect*core_symb_vect2 =(YaepCoreSymbVect*) t2;

    return(core_symb_vect1->set_core == core_symb_vect2->set_core
            && core_symb_vect1->symb == core_symb_vect2->symb);
}
#endif

/* Return hash of vector V. */
static unsigned vect_els_hash(YaepVect*v)
{
    unsigned result = jauquet_prime_mod32;
    int i;

    for(i = 0; i < v->len; i++)
        result = result* hash_shift + v->els[i];
    return result;
}

/* Return TRUE if V1 is equal to V2. */
static unsigned vect_els_eq(YaepVect*v1, YaepVect*v2)
{
    int i;
    if (v1->len != v2->len)
        return FALSE;

    for(i = 0; i < v1->len; i++)
        if (v1->els[i] != v2->els[i])
            return FALSE;
    return TRUE;
}

/* Hash of vector transition elements.*/
static unsigned transition_els_hash(hash_table_entry_t t)
{
    return vect_els_hash(&((YaepCoreSymbVect*) t)->transitions);
}

/* Equality of transition vector elements.*/
static int transition_els_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_els_eq(&((YaepCoreSymbVect*) t1)->transitions,
                        &((YaepCoreSymbVect*) t2)->transitions);
}

/* Hash of reduce vector elements.*/
static unsigned reduce_els_hash(hash_table_entry_t t)
{
    return vect_els_hash(&((YaepCoreSymbVect*) t)->reduces);
}

/* Equality of reduce vector elements.*/
static int reduce_els_eq(hash_table_entry_t t1, hash_table_entry_t t2)
{
    return vect_els_eq(&((YaepCoreSymbVect*) t1)->reduces,
                        &((YaepCoreSymbVect*) t2)->reduces);
}

/* Initialize work with the triples(set core, symbol, vector).*/
static void core_symb_vect_init(YaepParseState *ps)
{
    OS_CREATE(ps->core_symb_vect_os, ps->run.grammar->alloc, 0);
    VLO_CREATE(ps->new_core_symb_vect_vlo, ps->run.grammar->alloc, 0);
    OS_CREATE(ps->vect_els_os, ps->run.grammar->alloc, 0);

    vlo_array_init(ps);
#ifdef USE_CORE_SYMB_HASH_TABLE
    ps->core_symb_to_vect_tab = create_hash_table(ps->run.grammar->alloc, 3000, core_symb_vect_hash, core_symb_vect_eq);
#else
    VLO_CREATE(ps->core_symb_table_vlo, ps->run.grammar->alloc, 4096);
    ps->core_symb_table = (YaepCoreSymbVect***)VLO_BEGIN(ps->core_symb_table_vlo);
    OS_CREATE(ps->core_symb_tab_rows, ps->run.grammar->alloc, 8192);
#endif

    ps->transition_els_tab = create_hash_table(ps->run.grammar->alloc, 3000, transition_els_hash, transition_els_eq);
    ps->reduce_els_tab = create_hash_table(ps->run.grammar->alloc, 3000, reduce_els_hash, reduce_els_eq);

    ps->n_core_symb_pairs = ps->n_core_symb_vect_len = 0;
    ps->n_transition_vects = ps->n_transition_vect_len = 0;
    ps->n_reduce_vects = ps->n_reduce_vect_len = 0;
}

#ifdef USE_CORE_SYMB_HASH_TABLE

/* The following function returns entry in the table where pointer to
   corresponding triple with the same keys as TRIPLE ones is
   placed.*/
static YaepCoreSymbVect **core_symb_vect_addr_get(YaepParseState *ps, YaepCoreSymbVect *triple, int reserv_p)
{
    YaepCoreSymbVect**result;

    if (triple->symb->cached_core_symb_vect != NULL
        && triple->symb->cached_core_symb_vect->set_core == triple->set_core)
    {
        return &triple->symb->cached_core_symb_vect;
    }

    result = ((YaepCoreSymbVect**)find_hash_table_entry(ps->core_symb_to_vect_tab, triple, reserv_p));

    triple->symb->cached_core_symb_vect = *result;

    return result;
}

#else

/* The following function returns entry in the table where pointer to
   corresponding triple with SET_CORE and SYMB is placed.*/
static YaepCoreSymbVect **core_symb_vect_addr_get(YaepParseState *ps, YaepSetCore *set_core, YaepSymb *symb)
{
    YaepCoreSymbVect***core_symb_vect_ptr;

    core_symb_vect_ptr = ps->core_symb_table + set_core->num;

    if ((char*) core_symb_vect_ptr >=(char*) VLO_BOUND(ps->core_symb_table_vlo))
    {
        YaepCoreSymbVect***ptr,***bound;
        int diff, i;

        diff =((char*) core_symb_vect_ptr
                -(char*) VLO_BOUND(ps->core_symb_table_vlo));
        diff += sizeof(YaepCoreSymbVect**);
        if (diff == sizeof(YaepCoreSymbVect**))
            diff*= 10;

        VLO_EXPAND(ps->core_symb_table_vlo, diff);
        ps->core_symb_table
            =(YaepCoreSymbVect***) VLO_BEGIN(ps->core_symb_table_vlo);
        core_symb_vect_ptr = ps->core_symb_table + set_core->num;
        bound =(YaepCoreSymbVect***) VLO_BOUND(ps->core_symb_table_vlo);

        ptr = bound - diff / sizeof(YaepCoreSymbVect**);
        while(ptr < bound)
        {
            OS_TOP_EXPAND(ps->core_symb_tab_rows,
                          (ps->run.grammar->symbs_ptr->nn_terms + ps->run.grammar->symbs_ptr->n_nonterms)
                          * sizeof(YaepCoreSymbVect*));
           *ptr =(YaepCoreSymbVect**) OS_TOP_BEGIN(ps->core_symb_tab_rows);
            OS_TOP_FINISH(ps->core_symb_tab_rows);
            for(i = 0; i < ps->run.grammar->symbs_ptr->nn_terms + ps->run.grammar->symbs_ptr->n_nonterms; i++)
               (*ptr)[i] = NULL;
            ptr++;
        }
    }
    return &(*core_symb_vect_ptr)[symb->num];
}
#endif

/* The following function returns the triple(if any) for given SET_CORE and SYMB. */
static YaepCoreSymbVect *core_symb_vect_find(YaepParseState *ps, YaepSetCore *set_core, YaepSymb *symb)
{
    YaepCoreSymbVect *r;

#ifdef USE_CORE_SYMB_HASH_TABLE
    YaepCoreSymbVect core_symb_vect;

    core_symb_vect.set_core = set_core;
    core_symb_vect.symb = symb;
    r = *core_symb_vect_addr_get(ps, &core_symb_vect, FALSE);
#else
    r = *core_symb_vect_addr_get(ps, set_core, symb);
#endif

    TRACE_FA("%p %s -> %p", set_core, symb->repr, r);

    return r;
}

/* Add given triple(SET_CORE, TERM, ...) to the table and return
   pointer to it.*/
static YaepCoreSymbVect *core_symb_vect_new(YaepParseState *ps, YaepSetCore*set_core, YaepSymb*symb)
{
    YaepCoreSymbVect*triple;
    YaepCoreSymbVect**addr;
    vlo_t*vlo_ptr;

    /* Create table element.*/
    OS_TOP_EXPAND(ps->core_symb_vect_os, sizeof(YaepCoreSymbVect));
    triple =((YaepCoreSymbVect*) OS_TOP_BEGIN(ps->core_symb_vect_os));
    triple->set_core = set_core;
    triple->symb = symb;
    OS_TOP_FINISH(ps->core_symb_vect_os);

#ifdef USE_CORE_SYMB_HASH_TABLE
    addr = core_symb_vect_addr_get(ps, triple, TRUE);
#else
    addr = core_symb_vect_addr_get(ps, set_core, symb);
#endif
    assert(*addr == NULL);
   *addr = triple;

    triple->transitions.intern = vlo_array_expand(ps);
    vlo_ptr = vlo_array_el(ps, triple->transitions.intern);
    triple->transitions.len = 0;
    triple->transitions.els =(int*) VLO_BEGIN(*vlo_ptr);

    triple->reduces.intern = vlo_array_expand(ps);
    vlo_ptr = vlo_array_el(ps, triple->reduces.intern);
    triple->reduces.len = 0;
    triple->reduces.els =(int*) VLO_BEGIN(*vlo_ptr);
    VLO_ADD_MEMORY(ps->new_core_symb_vect_vlo, &triple,
                    sizeof(YaepCoreSymbVect*));
    ps->n_core_symb_pairs++;
    return triple;
}

/* Add EL to vector VEC. */
static void vect_new_add_el(YaepParseState *ps, YaepVect*vec, int el)
{
    vlo_t*vlo_ptr;

    vec->len++;
    vlo_ptr = vlo_array_el(ps, vec->intern);
    VLO_ADD_MEMORY(*vlo_ptr, &el, sizeof(int));
    vec->els =(int*) VLO_BEGIN(*vlo_ptr);
    ps->n_core_symb_vect_len++;
}

/* Add index EL to the transition vector of CORE_SYMB_VECT being formed.*/
static void core_symb_vect_new_add_transition_el(YaepParseState *ps,
                                                 YaepCoreSymbVect *core_symb_vect,
                                                 int el)
{
    vect_new_add_el(ps, &core_symb_vect->transitions, el);
}

/* Add index EL to the reduce vector of CORE_SYMB_VECT being formed.*/
static void core_symb_vect_new_add_reduce_el(YaepParseState *ps,
                                             YaepCoreSymbVect *core_symb_vect,
                                             int el)
{
    vect_new_add_el(ps, &core_symb_vect->reduces, el);
}

/* Insert vector VEC from CORE_SYMB_VECT into table TAB.  Update
   *N_VECTS and INT*N_VECT_LEN if it is a new vector in the table. */
static void process_core_symb_vect_el(YaepParseState *ps,
                                      YaepCoreSymbVect *core_symb_vect,
                                      YaepVect *vec,
                                      hash_table_t *tab,
                                      int *n_vects,
                                      int *n_vect_len)
{
    hash_table_entry_t*entry;

    if (vec->len == 0)
        vec->els = NULL;
    else
    {
        entry = find_hash_table_entry(*tab, core_symb_vect, TRUE);
        if (*entry != NULL)
            vec->els
                =(&core_symb_vect->transitions == vec
                   ?((YaepCoreSymbVect*)*entry)->transitions.els
                   :((YaepCoreSymbVect*)*entry)->reduces.els);
        else
	{
           *entry =(hash_table_entry_t) core_symb_vect;
            OS_TOP_ADD_MEMORY(ps->vect_els_os, vec->els, vec->len* sizeof(int));
            vec->els =(int*) OS_TOP_BEGIN(ps->vect_els_os);
            OS_TOP_FINISH(ps->vect_els_os);
           (*n_vects)++;
           *n_vect_len += vec->len;
	}
    }
    vec->intern = -1;
}

/* Finish forming all new triples core_symb_vect.*/
static void core_symb_vect_new_all_stop(YaepParseState *ps)
{
    YaepCoreSymbVect**triple_ptr;

    for(triple_ptr =(YaepCoreSymbVect**) VLO_BEGIN(ps->new_core_symb_vect_vlo);
        (char*) triple_ptr <(char*) VLO_BOUND(ps->new_core_symb_vect_vlo);
         triple_ptr++)
    {
        process_core_symb_vect_el(ps, *triple_ptr, &(*triple_ptr)->transitions,
                                  &ps->transition_els_tab, &ps->n_transition_vects,
                                  &ps->n_transition_vect_len);
        process_core_symb_vect_el(ps, *triple_ptr, &(*triple_ptr)->reduces,
                                  &ps->reduce_els_tab, &ps->n_reduce_vects,
                                  &ps->n_reduce_vect_len);
    }
    vlo_array_nullify(ps);
    VLO_NULLIFY(ps->new_core_symb_vect_vlo);
}

/* Finalize work with all triples(set core, symbol, vector).*/
static void core_symb_vect_fin(YaepParseState *ps)
{
    delete_hash_table(ps->transition_els_tab);
    delete_hash_table(ps->reduce_els_tab);

#ifdef USE_CORE_SYMB_HASH_TABLE
    delete_hash_table(ps->core_symb_to_vect_tab);
#else
    OS_DELETE(ps->core_symb_tab_rows);
    VLO_DELETE(ps->core_symb_table_vlo);
#endif
    vlo_array_fin(ps);
    OS_DELETE(ps->vect_els_os);
    VLO_DELETE(ps->new_core_symb_vect_vlo);
    OS_DELETE(ps->core_symb_vect_os);
}

/* The following function stores error CODE and MESSAGE.  The function
   makes long jump after that.  So the function is designed to process
   only one error.*/
static void yaep_error(YaepParseState *ps, int code, const char*format, ...)
{
    va_list arguments;

    ps->run.grammar->error_code = code;
    va_start(arguments, format);
    vsprintf(ps->run.grammar->error_message, format, arguments);
    va_end(arguments);
    assert(strlen(ps->run.grammar->error_message) < YAEP_MAX_ERROR_MESSAGE_LENGTH);
    longjmp(error_longjump_buff, code);
}

/* The following function processes allocation errors. */
static void error_func_for_allocate(void *ps)
{
   yaep_error((YaepParseState*)ps, YAEP_NO_MEMORY, "no memory");
}

YaepGrammar *yaepNewGrammar()
{
    YaepAllocator *allocator;

    allocator = yaep_alloc_new(NULL, NULL, NULL, NULL);
    if (allocator == NULL)
    {
        return NULL;
    }
    YaepGrammar *grammar = (YaepGrammar*)yaep_malloc(allocator, sizeof(*grammar));

    if (grammar == NULL)
    {
        yaep_alloc_del(allocator);
        return NULL;
    }
    grammar->alloc = allocator;
    yaep_alloc_seterr(allocator, error_func_for_allocate,
                      yaep_alloc_getuserptr(allocator));
    if (setjmp(error_longjump_buff) != 0)
    {
        //yaepFreeGrammar(pr, grammar);
        assert(0);
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
    grammar->symbs_ptr = symb_init(grammar);
    grammar->term_sets_ptr = term_set_init(grammar);
    grammar->rules_ptr = rule_init(grammar);
    return grammar;
}

YaepParseRun *yaepNewParseRun(YaepGrammar *g)
{
    YaepParseState *ps = (YaepParseState*)calloc(1, sizeof(YaepParseState));
    INSTALL_PARSE_STATE_MAGIC(ps);

    ps->run.grammar = g;

    return (YaepParseRun*)ps;
}

void yaepFreeParseRun(YaepParseRun *pr)
{
    YaepParseState *ps = (YaepParseState*)pr;
    assert(CHECK_PARSE_STATE_MAGIC(ps));
    free(ps);
}

void yaepSetUserData(YaepGrammar *g, void *data)
{
    g->user_data = data;
}

void *yaepGetUserData(YaepGrammar *g)
{
    return g->user_data;
}

/* The following function makes grammar empty.*/
static void yaep_empty_grammar(YaepParseState *ps,YaepGrammar *grammar)
{
    if (grammar != NULL)
    {
        rule_empty(grammar->rules_ptr);
        term_set_empty(grammar->term_sets_ptr);
        symb_empty(ps, grammar->symbs_ptr);
    }
}

/* The function returns the last occurred error code for given
   grammar.*/
int
yaep_error_code(YaepGrammar*g)
{
    assert(g != NULL);
    return g->error_code;
}

/* The function returns message are always contains error message
   corresponding to the last occurred error code.*/
const char*
yaep_error_message(YaepGrammar*g)
{
    assert(g != NULL);
    return g->error_message;
}

/* The following function creates sets FIRST and FOLLOW for all
   grammar nonterminals.*/
static void create_first_follow_sets(YaepParseState *ps)
{
    YaepSymb *symb, **rhs, *rhs_symb, *next_rhs_symb;
    YaepRule *rule;
    int changed_p, first_continue_p;
    int i, j, k, rhs_len;

    for (i = 0; (symb = nonterm_get(ps, i)) != NULL; i++)
    {
        symb->u.nonterm.first = term_set_create(ps, ps->run.grammar->symbs_ptr->nn_terms);
        term_set_clear(symb->u.nonterm.first, ps->run.grammar->symbs_ptr->nn_terms);
        symb->u.nonterm.follow = term_set_create(ps, ps->run.grammar->symbs_ptr->nn_terms);
        term_set_clear(symb->u.nonterm.follow, ps->run.grammar->symbs_ptr->nn_terms);
    }
    do
    {
        changed_p = 0;
        for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
            for(rule = symb->u.nonterm.rules;
                 rule != NULL; rule = rule->lhs_next)
            {
                first_continue_p = TRUE;
                rhs = rule->rhs;
                rhs_len = rule->rhs_len;
                for(j = 0; j < rhs_len; j++)
                {
                    rhs_symb = rhs[j];
                    if (rhs_symb->term_p)
                    {
                        if (first_continue_p)
                            changed_p |= term_set_up(symb->u.nonterm.first,
                                                     rhs_symb->u.term.term_num,
                                                     ps->run.grammar->symbs_ptr->nn_terms);
                    }
                    else
                    {
                        if (first_continue_p)
                            changed_p |= term_set_or(symb->u.nonterm.first,
                                                     rhs_symb->u.nonterm.first,
                                                     ps->run.grammar->symbs_ptr->nn_terms);
                        for(k = j + 1; k < rhs_len; k++)
                        {
                            next_rhs_symb = rhs[k];
                            if (next_rhs_symb->term_p)
                                changed_p
                                    |= term_set_up(rhs_symb->u.nonterm.follow,
                                                   next_rhs_symb->u.term.term_num,
                                                   ps->run.grammar->symbs_ptr->nn_terms);
                            else
                                changed_p
                                    |= term_set_or(rhs_symb->u.nonterm.follow,
                                                   next_rhs_symb->u.nonterm.first,
                                                   ps->run.grammar->symbs_ptr->nn_terms);
                            if (!next_rhs_symb->empty_p)
                                break;
                        }
                        if (k == rhs_len)
                            changed_p |= term_set_or(rhs_symb->u.nonterm.follow,
                                                     symb->u.nonterm.follow,
                                                     ps->run.grammar->symbs_ptr->nn_terms);
                    }
                    if (!rhs_symb->empty_p)
                        first_continue_p = FALSE;
                }
            }
    }
    while(changed_p);
}

/* The following function sets up flags empty_p, access_p and
   derivation_p for all grammar symbols.*/
static void set_empty_access_derives(YaepParseState *ps)
{
    YaepSymb*symb,*rhs_symb;
    YaepRule*rule;
    int empty_p, derivation_p;
    int empty_changed_p, derivation_changed_p, accessibility_change_p;
    int i, j;

    for(i = 0;(symb = symb_get(ps, i)) != NULL; i++)
    {
        symb->empty_p = 0;
        symb->derivation_p =(symb->term_p ? 1 : 0);
        symb->access_p = 0;
    }
    ps->run.grammar->axiom->access_p = 1;
    do
    {
        empty_changed_p = derivation_changed_p = accessibility_change_p = 0;
        for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
            for(rule = symb->u.nonterm.rules;
                 rule != NULL; rule = rule->lhs_next)
            {
                empty_p = derivation_p = 1;
                for(j = 0; j < rule->rhs_len; j++)
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
    while(empty_changed_p || derivation_changed_p || accessibility_change_p);

}

/* The following function sets up flags loop_p for nonterminals. */
static void set_loop_p(YaepParseState *ps)
{
    YaepSymb*symb,*lhs;
    YaepRule*rule;
    int i, j, k, loop_p, changed_p;

    /* Initialize accoding to minimal criteria: There is a rule in which
       the nonterminal stands and all the rest symbols can derive empty
       strings.*/
    for(rule = ps->run.grammar->rules_ptr->first_rule; rule != NULL; rule = rule->next)
        for(i = 0; i < rule->rhs_len; i++)
            if (!(symb = rule->rhs[i])->term_p)
            {
                for(j = 0; j < rule->rhs_len; j++)
                    if (i == j)
                        continue;
                    else if (!rule->rhs[j]->empty_p)
                        break;
                if (j >= rule->rhs_len)
                    symb->u.nonterm.loop_p = 1;
            }
    /* Major cycle: Check looped nonterminal that there is a rule with
       the nonterminal in lhs with a looped nonterminal in rhs and all
       the rest rhs symbols deriving empty string.*/
    do
    {
        changed_p = FALSE;
        for(i = 0;(lhs = nonterm_get(ps, i)) != NULL; i++)
            if (lhs->u.nonterm.loop_p)
            {
                loop_p = 0;
                for(rule = lhs->u.nonterm.rules;
                     rule != NULL; rule = rule->lhs_next)
                    for(j = 0; j < rule->rhs_len; j++)
                        if (!(symb = rule->rhs[j])->term_p && symb->u.nonterm.loop_p)
                        {
                            for(k = 0; k < rule->rhs_len; k++)
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
    while(changed_p);
}

/* The following function evaluates different sets and flags for
   grammar and checks the grammar on correctness.*/
static void check_grammar(YaepParseState *ps, int strict_p)
{
    YaepSymb*symb;
    int i;

    set_empty_access_derives(ps);
    set_loop_p(ps);
    if (strict_p)
    {
        for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
	{
            if (!symb->derivation_p)
            {
                yaep_error(ps, YAEP_NONTERM_DERIVATION,
                     "nonterm `%s' does not derive any term string", symb->repr);
            }
            else if (!symb->access_p)
            {
                yaep_error(ps,
                           YAEP_UNACCESSIBLE_NONTERM,
                           "nonterm `%s' is not accessible from axiom",
                           symb->repr);
            }
	}
    }
    else if (!ps->run.grammar->axiom->derivation_p)
    {
        yaep_error(ps, YAEP_NONTERM_DERIVATION,
                   "nonterm `%s' does not derive any term string",
                   ps->run.grammar->axiom->repr);
    }
    for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
    {
        if (symb->u.nonterm.loop_p)
        {
            yaep_error(ps, YAEP_LOOP_NONTERM,
                 "nonterm `%s' can derive only itself(grammar with loops)",
                 symb->repr);
        }
    }
    /* We should have correct flags empty_p here.*/
    create_first_follow_sets(ps);
}

/* The following are names of additional symbols.  Don't use them in
   grammars.*/
#define AXIOM_NAME "$S"
#define END_MARKER_NAME "$eof"
#define TERM_ERROR_NAME "error"

/* It should be negative.*/
#define END_MARKER_CODE -1
#define TERM_ERROR_CODE -2

/* The following function reads terminals/rules.  The function returns
   pointer to the grammar(or NULL if there were errors in
   grammar).*/
int yaep_read_grammar(YaepParseRun *pr,
                      YaepGrammar *g,
                      int strict_p,
                      const char*(*read_terminal)(int*code),
                      const char*(*read_rule)(const char***rhs,
                                              const char**abs_node,
                                              int*anode_cost, int**transl, char*mark, char**marks))
{
    const char*name,*lhs,**rhs,*anode;
    YaepSymb*symb,*start;
    YaepRule*rule;
    int anode_cost;
    int*transl;
    char mark;
    char*marks;
    int i, el, code;

    assert(g != NULL);
    YaepParseState *ps = (YaepParseState*)pr;
    assert(CHECK_PARSE_STATE_MAGIC(ps));

    if ((code = setjmp(error_longjump_buff)) != 0)
    {
        return code;
    }
    if (!ps->run.grammar->undefined_p)
    {
        yaep_empty_grammar(ps, ps->run.grammar);
    }
    while((name =(*read_terminal)(&code)) != NULL)
    {
        // fprintf(stderr, "TERM >%s< %d\n", name, code); // DEBUGGING TODO REMOVE
        if (code < 0)
            yaep_error(ps, YAEP_NEGATIVE_TERM_CODE,
                        "term `%s' has negative code", name);
        symb = symb_find_by_repr(ps, name);
        if (symb != NULL)
            yaep_error(ps, YAEP_REPEATED_TERM_DECL,
                        "repeated declaration of term `%s'", name);
        if (symb_find_by_code(ps, code) != NULL)
            yaep_error(ps, YAEP_REPEATED_TERM_CODE,
                        "repeated code %d in term `%s'", code, name);
        symb_add_term(ps, name, code);
    }

    /* Adding error symbol.*/
    if (symb_find_by_repr(ps, TERM_ERROR_NAME) != NULL)
        yaep_error(ps,
                   YAEP_FIXED_NAME_USAGE,
                   "do not use fixed name `%s'", TERM_ERROR_NAME);
    if (symb_find_by_code(ps, TERM_ERROR_CODE) != NULL)
        abort();
    ps->run.grammar->term_error = symb_add_term(ps, TERM_ERROR_NAME, TERM_ERROR_CODE);
    ps->run.grammar->term_error_num = ps->run.grammar->term_error->u.term.term_num;
    ps->run.grammar->axiom = ps->run.grammar->end_marker = NULL;
    while((lhs =(*read_rule)(&rhs, &anode, &anode_cost, &transl, &mark, &marks)) != NULL)
    {
        // fprintf(stderr, "LHS >%s<\n", lhs); // DEBUGGING TODO REMOVE
        symb = symb_find_by_repr(ps, lhs);
        if (symb == NULL)
            symb = symb_add_nonterm(ps, lhs);
        else if (symb->term_p)
            yaep_error(ps, YAEP_TERM_IN_RULE_LHS,
                        "term `%s' in the left hand side of rule", lhs);
        if (anode == NULL && transl != NULL &&*transl >= 0 && transl[1] >= 0)
            yaep_error(ps, YAEP_INCORRECT_TRANSLATION,
                        "rule for `%s' has incorrect translation", lhs);
        if (anode != NULL && anode_cost < 0)
            yaep_error(ps, YAEP_NEGATIVE_COST,
                        "translation for `%s' has negative cost", lhs);
        if (ps->run.grammar->axiom == NULL)
	{
            /* We made this here becuase we want that the start rule has
               number 0.*/
            /* Add axiom and end marker.*/
            start = symb;
            ps->run.grammar->axiom = symb_find_by_repr(ps, AXIOM_NAME);
            if (ps->run.grammar->axiom != NULL)
                yaep_error(ps, YAEP_FIXED_NAME_USAGE,
                            "do not use fixed name `%s'", AXIOM_NAME);
            ps->run.grammar->axiom = symb_add_nonterm(ps, AXIOM_NAME);
            ps->run.grammar->end_marker = symb_find_by_repr(ps, END_MARKER_NAME);
            if (ps->run.grammar->end_marker != NULL)
                yaep_error(ps, YAEP_FIXED_NAME_USAGE,
                            "do not use fixed name `%s'", END_MARKER_NAME);
            if (symb_find_by_code(ps, END_MARKER_CODE) != NULL)
                abort();
            ps->run.grammar->end_marker = symb_add_term(ps, END_MARKER_NAME, END_MARKER_CODE);
            /* Add rules for start*/
            rule = rule_new_start(ps, ps->run.grammar->axiom, NULL, 0);
            rule_new_symb_add(ps, symb);
            rule_new_symb_add(ps, ps->run.grammar->end_marker);
            rule_new_stop(ps);
            rule->order[0] = 0;
            rule->trans_len = 1;
	}
        rule = rule_new_start(ps, symb, anode,(anode != NULL ? anode_cost : 0));
        size_t rhs_len = 0;
        while(*rhs != NULL)
	{
            rhs_len++;
            symb = symb_find_by_repr(ps, *rhs);
            if (symb == NULL)
                symb = symb_add_nonterm(ps, *rhs);
            rule_new_symb_add(ps, symb);
            rhs++;
	}
        rule_new_stop(ps);
        // IXML
        rule->mark = mark;
        memcpy(rule->marks, marks, rhs_len);

        if (transl != NULL)
	{
            for(i = 0;(el = transl[i]) >= 0; i++)
            {
                if (el >= rule->rhs_len)
                {
                    if (el != YAEP_NIL_TRANSLATION_NUMBER)
                    {
                        yaep_error(ps, YAEP_INCORRECT_SYMBOL_NUMBER,
                                   "translation symbol number %d in rule for `%s' is out of range",
                                   el, lhs);
                    }
                    else
                    {
                        rule->trans_len++;
                    }
                }
                else if (rule->order[el] >= 0)
                {
                    yaep_error(ps, YAEP_REPEATED_SYMBOL_NUMBER,
                               "repeated translation symbol number %d in rule for `%s'",
                               el, lhs);
                }
                else
                {
                    rule->order[el] = i;
                    rule->trans_len++;
                }
                assert(i < rule->rhs_len || transl[i] < 0);
            }
	}
    }
    if (ps->run.grammar->axiom == NULL)
        yaep_error(ps, YAEP_NO_RULES, "grammar does not contains rules");
    assert(start != NULL);
    /* Adding axiom : error $eof if it is neccessary.*/
    for(rule = start->u.nonterm.rules; rule != NULL; rule = rule->lhs_next)
        if (rule->rhs[0] == ps->run.grammar->term_error)
            break;
    if (rule == NULL)
    {
        rule = rule_new_start(ps, ps->run.grammar->axiom, NULL, 0);
        rule_new_symb_add(ps, ps->run.grammar->term_error);
        rule_new_symb_add(ps, ps->run.grammar->end_marker);
        rule_new_stop(ps);
        rule->trans_len = 0;
    }
    check_grammar(ps, strict_p);

    symb_finish_adding_terms(ps);

    if (ps->run.grammar->debug_level > 2)
    {
        /* Print rules.*/
        fprintf(stderr, "Rules:\n");
        for(rule = ps->run.grammar->rules_ptr->first_rule; rule != NULL; rule = rule->next)
	{
            fprintf(stderr, "  ");
            rule_print(ps, stderr, rule, TRUE);
	}
        fprintf(stderr, "\n");
        /* Print symbol sets.*/
        for(i = 0;(symb = nonterm_get(ps, i)) != NULL; i++)
	{
            fprintf(stderr, "Nonterm %s:  Empty=%s , Access=%s, Derive=%s\n",
                     symb->repr,(symb->empty_p ? "Yes" : "No"),
                    (symb->access_p ? "Yes" : "No"),
                    (symb->derivation_p ? "Yes" : "No"));
            if (ps->run.grammar->debug_level > 3)
	    {
                fprintf(stderr, "  First: ");
                term_set_print(ps, stderr, symb->u.nonterm.first, ps->run.grammar->symbs_ptr->nn_terms);
                fprintf(stderr, "\n  Follow: ");
                term_set_print(ps, stderr, symb->u.nonterm.follow, ps->run.grammar->symbs_ptr->nn_terms);
                fprintf(stderr, "\n\n");
	    }
	}
    }

    ps->run.grammar->undefined_p = FALSE;
    return 0;
}

//include "sgramm.c"

/* The following functions set up parameter which affect parser work
   and return the previous parameter value.*/

int
yaep_set_lookahead_level(YaepGrammar *grammar, int level)
{
    int old;

    assert(grammar != NULL);
    old = grammar->lookahead_level;
    grammar->lookahead_level =(level < 0 ? 0 : level > 2 ? 2 : level);
    return old;
}

int
yaep_set_debug_level(YaepGrammar *grammar, int level)
{
    int old;

    assert(grammar != NULL);
    old = grammar->debug_level;
    grammar->debug_level = level;
    return old;
}

int
yaep_set_one_parse_flag(YaepGrammar *grammar, int flag)
{
    int old;

    assert(grammar != NULL);
    old = grammar->one_parse_p;
    grammar->one_parse_p = flag;
    return old;
}

int
yaep_set_cost_flag(YaepGrammar *grammar, int flag)
{
    int old;

    assert(grammar != NULL);
    old = grammar->cost_p;
    grammar->cost_p = flag;
    return old;
}

int
yaep_set_error_recovery_flag(YaepGrammar *grammar, int flag)
{
    int old;

    assert(grammar != NULL);
    old = grammar->error_recovery_p;
    grammar->error_recovery_p = flag;
    return old;
}

int
yaep_set_recovery_match(YaepGrammar *grammar, int n_toks)
{
    int old;

    assert(grammar != NULL);
    old = grammar->recovery_token_matches;
    grammar->recovery_token_matches = n_toks;
    return old;
}

/* The function initializes all internal data for parser for N_TOKS
   tokens.*/
static void yaep_parse_init(YaepParseState *ps, int n_toks)
{
    YaepRule*rule;

    sit_init(ps);
    set_init(ps, n_toks);
    core_symb_vect_init(ps);
#ifdef USE_CORE_SYMB_HASH_TABLE
    {
        int i;
        YaepSymb*symb;

        for(i = 0;(symb = symb_get(ps, i)) != NULL; i++)
            symb->cached_core_symb_vect = NULL;
    }
#endif
    for(rule = ps->run.grammar->rules_ptr->first_rule; rule != NULL; rule = rule->next)
        rule->caller_anode = NULL;
}

/* The function should be called the last(it frees all allocated
   data for parser).*/
static void yaep_parse_fin(YaepParseState *ps)
{
    core_symb_vect_fin(ps);
    set_fin(ps);
    sit_fin(ps);
}

/* The following function reads all input tokens.*/
static void read_toks(YaepParseState *ps)
{
    int code;
    void *attr;

    while((code = ps->run.read_token((YaepParseRun*)ps, &attr)) >= 0)
    {
        tok_add(ps, code, attr);
    }
    tok_add(ps, END_MARKER_CODE, NULL);
}

/* The following function add start situations which is formed from
   given start situation SIT with distance DIST by reducing symbol
   which can derivate empty string and which is placed after dot in
   given situation.  The function returns TRUE if the dot is placed on
   the last position in given situation or in the added situations.*/
static void add_derived_nonstart_sits(YaepParseState *ps, YaepSituation*sit, int parent)
{
    YaepSymb*symb;
    YaepRule*rule = sit->rule;
    int context = sit->context;
    int i;

    for(i = sit->pos;(symb = rule->rhs[i]) != NULL && symb->empty_p; i++)
        set_add_new_nonstart_sit(ps, sit_create(ps, rule, i + 1, context), parent);
}

/* The following function adds the rest(non-start) situations to the
   new set and and forms triples(set core, symbol, indexes) for
   further fast search of start situations from given core by
   transition on given symbol(see comment for abstract data
   `core_symb_vect').*/
static void expand_new_start_set(YaepParseState *ps)
{
    YaepSituation*sit;
    YaepSymb*symb;
    YaepCoreSymbVect*core_symb_vect;
    YaepRule*rule;
    int i;

    /* Add non start situations with nonzero distances.*/
    for(i = 0; i < ps->new_n_start_sits; i++)
        add_derived_nonstart_sits(ps, ps->new_sits[i], i);
    /* Add non start situations and form transitions vectors.*/
    for(i = 0; i < ps->new_core->n_sits; i++)
    {
        sit = ps->new_sits[i];
        if (sit->pos < sit->rule->rhs_len)
	{
            /* There is a symbol after dot in the situation.*/
            symb = sit->rule->rhs[sit->pos];
            core_symb_vect = core_symb_vect_find(ps, ps->new_core, symb);
            if (core_symb_vect == NULL)
	    {
                core_symb_vect = core_symb_vect_new(ps, ps->new_core, symb);
                if (!symb->term_p)
                    for(rule = symb->u.nonterm.rules;
                         rule != NULL; rule = rule->lhs_next)
                        set_new_add_initial_sit(ps, sit_create(ps, rule, 0, 0));
	    }
            core_symb_vect_new_add_transition_el(ps, core_symb_vect, i);
            if (symb->empty_p && i >= ps->new_core->n_all_dists)
                set_new_add_initial_sit(ps, sit_create(ps, sit->rule, sit->pos + 1, 0));
	}
    }
    /* Now forming reduce vectors.*/
    for(i = 0; i < ps->new_core->n_sits; i++)
    {
        sit = ps->new_sits[i];
        if (sit->pos == sit->rule->rhs_len)
	{
            symb = sit->rule->lhs;
            core_symb_vect = core_symb_vect_find(ps, ps->new_core, symb);
            if (core_symb_vect == NULL)
                core_symb_vect = core_symb_vect_new(ps, ps->new_core, symb);
            core_symb_vect_new_add_reduce_el(ps, core_symb_vect, i);
	}
    }
    if (ps->run.grammar->lookahead_level > 1)
    {
        YaepSituation*new_sit,*shifted_sit;
        term_set_el_t*context_set;
        int changed_p, sit_ind, context, j;

        /* Now we have incorrect initial situations because their
           context is not correct.*/
        context_set = term_set_create(ps, ps->run.grammar->symbs_ptr->nn_terms);
        do
	{
            changed_p = FALSE;
            for(i = ps->new_core->n_all_dists; i < ps->new_core->n_sits; i++)
	    {
                term_set_clear(context_set, ps->run.grammar->symbs_ptr->nn_terms);
                new_sit = ps->new_sits[i];
                core_symb_vect = core_symb_vect_find(ps, ps->new_core, new_sit->rule->lhs);
                for(j = 0; j < core_symb_vect->transitions.len; j++)
		{
                    sit_ind = core_symb_vect->transitions.els[j];
                    sit = ps->new_sits[sit_ind];
                    shifted_sit = sit_create(ps, sit->rule, sit->pos + 1,
                                              sit->context);
                    term_set_or(context_set, shifted_sit->lookahead, ps->run.grammar->symbs_ptr->nn_terms);
		}
                context = term_set_insert(ps, context_set);
                if (context >= 0)
                    context_set = term_set_create(ps, ps->run.grammar->symbs_ptr->nn_terms);
                else
                    context = -context - 1;
                sit = sit_create(ps, new_sit->rule, new_sit->pos, context);
                if (sit != new_sit)
		{
                    ps->new_sits[i] = sit;
                    changed_p = TRUE;
		}
	    }
	}
        while(changed_p);
    }
    set_new_core_stop(ps);
    core_symb_vect_new_all_stop(ps);
}

/* The following function forms the 1st set.*/
static void build_start_set(YaepParseState *ps)
{
    YaepRule*rule;
    YaepSituation*sit;
    term_set_el_t*context_set;
    int context;

    set_new_start(ps);
    if (ps->run.grammar->lookahead_level <= 1)
        context = 0;
    else
    {
        context_set = term_set_create(ps, ps->run.grammar->symbs_ptr->nn_terms);
        term_set_clear(context_set, ps->run.grammar->symbs_ptr->nn_terms);
        context = term_set_insert(ps, context_set);
        /* Empty context in the table has always number zero.*/
        assert(context == 0);
    }
    for(rule = ps->run.grammar->axiom->u.nonterm.rules;
         rule != NULL; rule = rule->lhs_next)
    {
        sit = sit_create(ps, rule, 0, context);
        set_new_add_start_sit(ps, sit, 0);
    }
    if (!set_insert(ps)) assert(FALSE);
    expand_new_start_set(ps);
    ps->pl[0] = ps->new_set;

    if (ps->run.grammar->debug_level > 2)
    {
        fprintf(stderr, "\nParsing start...\n");
        if (ps->run.grammar->debug_level > 3)
        {
            set_print(ps, stderr, ps->new_set, 0, ps->run.grammar->debug_level > 4, ps->run.grammar->debug_level > 5);
        }
    }
}

/* The following function builds new set by shifting situations of SET
   given in CORE_SYMB_VECT with given lookahead terminal number.  If
   the number is negative, we ignore lookahead at all.*/
static void build_new_set(YaepParseState *ps,
                          YaepSet *set,
                          YaepCoreSymbVect *core_symb_vect,
                          int lookahead_term_num)
{
    YaepSet*prev_set;
    YaepSetCore*set_core,*prev_set_core;
    YaepSituation*sit,*new_sit,**prev_sits;
    YaepCoreSymbVect*prev_core_symb_vect;
    int local_lookahead_level, dist, sit_ind, new_dist;
    int i, place;
    YaepVect*transitions;

    local_lookahead_level =(lookahead_term_num < 0
                             ? 0 : ps->run.grammar->lookahead_level);
    set_core = set->core;
    set_new_start(ps);
    transitions = &core_symb_vect->transitions;

    empty_sit_dist_set(ps);
    for(i = 0; i < transitions->len; i++)
    {
        sit_ind = transitions->els[i];
        sit = set_core->sits[sit_ind];
        new_sit = sit_create(ps, sit->rule, sit->pos + 1, sit->context);
        if (local_lookahead_level != 0
            && !term_set_test(new_sit->lookahead, lookahead_term_num, ps->run.grammar->symbs_ptr->nn_terms)
            && !term_set_test(new_sit->lookahead, ps->run.grammar->term_error_num, ps->run.grammar->symbs_ptr->nn_terms))
            continue;
        dist = 0;
        if (sit_ind >= set_core->n_all_dists)
            ;
        else if (sit_ind < set_core->n_start_sits)
            dist = set->dists[sit_ind];
        else
            dist = set->dists[set_core->parent_indexes[sit_ind]];
        dist++;
        if (sit_dist_insert(ps, new_sit, dist))
            set_new_add_start_sit(ps, new_sit, dist);
    }
    for(i = 0; i < ps->new_n_start_sits; i++)
    {
        new_sit = ps->new_sits[i];
        if (new_sit->empty_tail_p)
	{
            int*curr_el,*bound;

            /* All tail in new sitiation may derivate empty string so
               make reduce and add new situations.*/
            new_dist = ps->new_dists[i];
            place = ps->pl_curr + 1 - new_dist;
            prev_set = ps->pl[place];
            prev_set_core = prev_set->core;
            prev_core_symb_vect = core_symb_vect_find(ps, prev_set_core, new_sit->rule->lhs);
            if (prev_core_symb_vect == NULL)
	    {
                assert(new_sit->rule->lhs == ps->run.grammar->axiom);
                continue;
	    }
            curr_el = prev_core_symb_vect->transitions.els;
            bound = curr_el + prev_core_symb_vect->transitions.len;

            assert(curr_el != NULL);
            prev_sits = prev_set_core->sits;
            do
	    {
                sit_ind =*curr_el++;
                sit = prev_sits[sit_ind];
                new_sit = sit_create(ps, sit->rule, sit->pos + 1, sit->context);
                if (local_lookahead_level != 0
                    && !term_set_test(new_sit->lookahead, lookahead_term_num, ps->run.grammar->symbs_ptr->nn_terms)
                    && !term_set_test(new_sit->lookahead,
                                      ps->run.grammar->term_error_num,
                                      ps->run.grammar->symbs_ptr->nn_terms))
                    continue;
                dist = 0;
                if (sit_ind >= prev_set_core->n_all_dists)
                    ;
                else if (sit_ind < prev_set_core->n_start_sits)
                    dist = prev_set->dists[sit_ind];
                else
                    dist =
                        prev_set->dists[prev_set_core->parent_indexes[sit_ind]];
                dist += new_dist;
                if (sit_dist_insert(ps, new_sit, dist))
                    set_new_add_start_sit(ps, new_sit, dist);
	    }
            while(curr_el < bound);
	}
    }
    if (set_insert(ps))
    {
        expand_new_start_set(ps);
        ps->new_core->term = core_symb_vect->symb;
    }
}



/* This page contains error recovery code.  This code finds minimal
   cost error recovery.  The cost of error recovery is number of
   tokens ignored by error recovery.  The error recovery is successful
   when we match at least RECOVERY_TOKEN_MATCHES tokens.*/

/* The following strucrture describes an error recovery state(an
   error recovery alternative.*/
struct recovery_state
{
    /* The following three members define start pl used to given error
       recovery state(alternative).*/
    /* The following members define what part of original(at the error
       recovery start) pl will be head of error recovery state.  The
       head will be all states from original pl with indexes in range
       [0, last_original_pl_el].*/
    int last_original_pl_el;
    /* The following two members define tail of pl for this error
       recovery state.*/
    int pl_tail_length;
    YaepSet**pl_tail;
    /* The following member is index of start token for given error
       recovery state.*/
    int start_tok;
    /* The following member value is number of tokens already ignored in
       order to achieved given error recovery state.*/
    int backward_move_cost;
};
/* The following function may be called if you know that pl has
   original sets upto LAST element(including it).  Such call can
   decrease number of restored sets.*/
static void set_original_set_bound(YaepParseState *ps, int last)
{
    assert(last >= 0 && last <= ps->start_pl_curr
            && ps->original_last_pl_el <= ps->start_pl_curr);
    ps->original_last_pl_el = last;
}

/* The following function guarantees that original pl tail sets
   starting with pl_curr(including the state) is saved.  The function
   should be called after any decreasing pl_curr with subsequent
   writing to pl [pl_curr].*/
static void save_original_sets(YaepParseState *ps)
{
    int length, curr_pl;

    assert(ps->pl_curr >= 0 && ps->original_last_pl_el <= ps->start_pl_curr);
    length = VLO_LENGTH(ps->original_pl_tail_stack) / sizeof(YaepSet*);
    for(curr_pl = ps->start_pl_curr - length; curr_pl >= ps->pl_curr; curr_pl--)
    {
        VLO_ADD_MEMORY(ps->original_pl_tail_stack, &ps->pl[curr_pl],
                        sizeof(YaepSet*));
#ifndef NO_YAEP_DEBUG_PRINT
        if (ps->run.grammar->debug_level > 2)
	{
            fprintf(stderr, "++++Save original set=%d\n", curr_pl);
            if (ps->run.grammar->debug_level > 3)
	    {
                set_print(ps, stderr, ps->pl[curr_pl], curr_pl,
                          ps->run.grammar->debug_level > 4, ps->run.grammar->debug_level > 5);
                fprintf(stderr, "\n");
	    }
	}
#endif
    }
    ps->original_last_pl_el = ps->pl_curr - 1;
}

/* If it is necessary, the following function restores original pl
   part with states in range [0, last_pl_el].*/
static void restore_original_sets(YaepParseState *ps, int last_pl_el)
{
    assert(last_pl_el <= ps->start_pl_curr
            && ps->original_last_pl_el <= ps->start_pl_curr);
    if (ps->original_last_pl_el >= last_pl_el)
    {
        ps->original_last_pl_el = last_pl_el;
        return;
    }
    for(;;)
    {
        ps->original_last_pl_el++;
        ps->pl[ps->original_last_pl_el]
            =((YaepSet**) VLO_BEGIN(ps->original_pl_tail_stack))
            [ps->start_pl_curr - ps->original_last_pl_el];
#ifndef NO_YAEP_DEBUG_PRINT
        if (ps->run.grammar->debug_level > 2)
	{
            fprintf(stderr, "++++++Restore original set=%d\n",
                     ps->original_last_pl_el);
            if (ps->run.grammar->debug_level > 3)
	    {
                set_print(ps, stderr, ps->pl[ps->original_last_pl_el], ps->original_last_pl_el,
                          ps->run.grammar->debug_level > 4, ps->run.grammar->debug_level > 5);
                fprintf(stderr, "\n");
	    }
	}
#endif
        if (ps->original_last_pl_el >= last_pl_el)
            break;
    }
}

/* The following function looking backward in pl starting with element
   START_PL_EL and returns pl element which refers set with situation
   containing `. error'.  START_PL_EL should be non negative.
   Remember that zero pl set contains `.error' because we added such
   rule if it is necessary.  The function returns number of terminals
  (not taking error into account) on path(result, start_pl_set].*/
static int find_error_pl_set(YaepParseState *ps, int start_pl_set, int*cost)
{
    int curr_pl;

    assert(start_pl_set >= 0);
   *cost = 0;
    for(curr_pl = start_pl_set; curr_pl >= 0; curr_pl--)
        if (core_symb_vect_find(ps, ps->pl[curr_pl]->core, ps->run.grammar->term_error) != NULL)
            break;
        else if (ps->pl[curr_pl]->core->term != ps->run.grammar->term_error)
           (*cost)++;
    assert(curr_pl >= 0);
    return curr_pl;
}

/* The following function creates and returns new error recovery state
   with charcteristics(LAST_ORIGINAL_PL_EL, BACKWARD_MOVE_COST,
   pl_curr, tok_curr).*/
static struct recovery_state new_recovery_state(YaepParseState *ps, int last_original_pl_el, int backward_move_cost)
{
    struct recovery_state state;
    int i;

    assert(backward_move_cost >= 0);
#ifndef NO_YAEP_DEBUG_PRINT
    if (ps->run.grammar->debug_level > 2)
    {
        fprintf(stderr,
                 "++++Creating recovery state: original set=%d, tok=%d, ",
                 last_original_pl_el, ps->tok_curr);
        symb_print(stderr, ps->toks[ps->tok_curr].symb, TRUE);
        fprintf(stderr, "\n");
    }
#endif
    state.last_original_pl_el = last_original_pl_el;
    state.pl_tail_length = ps->pl_curr - last_original_pl_el;
    assert(state.pl_tail_length >= 0);
    for(i = last_original_pl_el + 1; i <= ps->pl_curr; i++)
    {
        OS_TOP_ADD_MEMORY(ps->recovery_state_tail_sets, &ps->pl[i], sizeof(ps->pl[i]));
#ifndef NO_YAEP_DEBUG_PRINT
        if (ps->run.grammar->debug_level > 3)
	{
            fprintf(stderr, "++++++Saving set=%d\n", i);
            set_print(ps, stderr, ps->pl[i], i, ps->run.grammar->debug_level > 4,
                      ps->run.grammar->debug_level > 5);
            fprintf(stderr, "\n");
	}
#endif
    }
    state.pl_tail =(YaepSet**) OS_TOP_BEGIN(ps->recovery_state_tail_sets);
    OS_TOP_FINISH(ps->recovery_state_tail_sets);
    state.start_tok = ps->tok_curr;
    state.backward_move_cost = backward_move_cost;
    return state;
}

/* The following function creates new error recovery state and pushes
   it on the states stack top. */
static void push_recovery_state(YaepParseState *ps, int last_original_pl_el, int backward_move_cost)
{
    struct recovery_state state;

    state = new_recovery_state(ps, last_original_pl_el, backward_move_cost);
#ifndef NO_YAEP_DEBUG_PRINT
    if (ps->run.grammar->debug_level > 2)
    {
        fprintf(stderr, "++++Push recovery state: original set=%d, tok=%d, ",
                 last_original_pl_el, ps->tok_curr);
        symb_print(stderr, ps->toks[ps->tok_curr].symb, TRUE);
        fprintf(stderr, "\n");
    }
#endif
    VLO_ADD_MEMORY(ps->recovery_state_stack, &state, sizeof(state));
}

/* The following function sets up parser state(pl, pl_curr, ps->tok_curr)
   according to error recovery STATE. */
static void set_recovery_state(YaepParseState *ps, struct recovery_state*state)
{
    int i;

    ps->tok_curr = state->start_tok;
    restore_original_sets(ps, state->last_original_pl_el);
    ps->pl_curr = state->last_original_pl_el;
#ifndef NO_YAEP_DEBUG_PRINT
    if (ps->run.grammar->debug_level > 2)
    {
        fprintf(stderr, "++++Set recovery state: set=%d, tok=%d, ",
                 ps->pl_curr, ps->tok_curr);
        symb_print(stderr, ps->toks[ps->tok_curr].symb, TRUE);
        fprintf(stderr, "\n");
    }
#endif
    for(i = 0; i < state->pl_tail_length; i++)
    {
        ps->pl[++ps->pl_curr] = state->pl_tail[i];
#ifndef NO_YAEP_DEBUG_PRINT
        if (ps->run.grammar->debug_level > 3)
	{
            fprintf(stderr, "++++++Add saved set=%d\n", ps->pl_curr);
            set_print(ps, stderr, ps->pl[ps->pl_curr], ps->pl_curr, ps->run.grammar->debug_level > 4,
                      ps->run.grammar->debug_level > 5);
            fprintf(stderr, "\n");
	}
#endif
    }
}

/* The following function pops the top error recovery state from
   states stack.  The current parser state will be setup according to
   the state. */
static struct recovery_state pop_recovery_state(YaepParseState *ps)
{
    struct recovery_state *state;

    state = &((struct recovery_state*) VLO_BOUND(ps->recovery_state_stack))[-1];
    VLO_SHORTEN(ps->recovery_state_stack, sizeof(struct recovery_state));
#ifndef NO_YAEP_DEBUG_PRINT
    if (ps->run.grammar->debug_level > 2)
        fprintf(stderr, "++++Pop error recovery state\n");
#endif
    set_recovery_state(ps, state);
    return*state;
}

/* The following function is major function of syntax error recovery.
   It searches for minimal cost error recovery.  The function returns
   in the parameter number of start token which is ignored and number
   of the first token which is not ignored.  If the number of ignored
   tokens is zero,*START will be equal to*STOP and number of token
   on which the error occurred.*/
static void error_recovery(YaepParseState *ps, int *start, int *stop)
{
    YaepSet*set;
    YaepCoreSymbVect*core_symb_vect;
    struct recovery_state best_state, state;
    int best_cost, cost, lookahead_term_num, n_matched_toks;
    int back_to_frontier_move_cost, backward_move_cost;

#ifndef NO_YAEP_DEBUG_PRINT
    if (ps->run.grammar->debug_level > 2)
        fprintf(stderr, "\n++Error recovery start\n");
#endif
   *stop =*start = -1;
    OS_CREATE(ps->recovery_state_tail_sets, ps->run.grammar->alloc, 0);
    VLO_NULLIFY(ps->original_pl_tail_stack);
    VLO_NULLIFY(ps->recovery_state_stack);
    ps->start_pl_curr = ps->pl_curr;
    ps->start_tok_curr = ps->tok_curr;
    /* Initialize error recovery state stack.*/
    ps->pl_curr
        = ps->back_pl_frontier = find_error_pl_set(ps, ps->pl_curr, &backward_move_cost);
    back_to_frontier_move_cost = backward_move_cost;
    save_original_sets(ps);
    push_recovery_state(ps, ps->back_pl_frontier, backward_move_cost);
    best_cost = 2* ps->toks_len;
    while(VLO_LENGTH(ps->recovery_state_stack) > 0)
    {
        state = pop_recovery_state(ps);
        cost = state.backward_move_cost;
        assert(cost >= 0);
        /* Advance back frontier.*/
        if (ps->back_pl_frontier > 0)
	{
            int saved_pl_curr = ps->pl_curr, saved_tok_curr = ps->tok_curr;

            /* Advance back frontier.*/
            ps->pl_curr = find_error_pl_set(ps, ps->back_pl_frontier - 1,
                                         &backward_move_cost);
#ifndef NO_YAEP_DEBUG_PRINT
            if (ps->run.grammar->debug_level > 2)
                fprintf(stderr, "++++Advance back frontier: old=%d, new=%d\n",
                         ps->back_pl_frontier, ps->pl_curr);
#endif
            if (best_cost >= back_to_frontier_move_cost + backward_move_cost)
	    {
                ps->back_pl_frontier = ps->pl_curr;
                ps->tok_curr = ps->start_tok_curr;
                save_original_sets(ps);
                back_to_frontier_move_cost += backward_move_cost;
                push_recovery_state(ps, ps->back_pl_frontier,
                                    back_to_frontier_move_cost);
                set_original_set_bound(ps, state.last_original_pl_el);
                ps->tok_curr = saved_tok_curr;
	    }
            ps->pl_curr = saved_pl_curr;
	}
        /* Advance head frontier.*/
        if (best_cost >= cost + 1)
	{
            ps->tok_curr++;
            if (ps->tok_curr < ps->toks_len)
	    {
#ifndef NO_YAEP_DEBUG_PRINT
                if (ps->run.grammar->debug_level > 2)
		{
                    fprintf(stderr,
                             "++++Advance head frontier(one pos): tok=%d, ",
                             ps->tok_curr);
                    symb_print(stderr, ps->toks[ps->tok_curr].symb, TRUE);
                    fprintf(stderr, "\n");
#endif
		}
                push_recovery_state(ps, state.last_original_pl_el, cost + 1);
	    }
            ps->tok_curr--;
	}
        set = ps->pl[ps->pl_curr];
#ifndef NO_YAEP_DEBUG_PRINT
        if (ps->run.grammar->debug_level > 2)
	{
            fprintf(stderr, "++++Trying set=%d, tok=%d, ", ps->pl_curr, ps->tok_curr);
            symb_print(stderr, ps->toks[ps->tok_curr].symb, TRUE);
            fprintf(stderr, "\n");
	}
#endif
        /* Shift error:*/
        core_symb_vect = core_symb_vect_find(ps, set->core, ps->run.grammar->term_error);
        assert(core_symb_vect != NULL);
#ifndef NO_YAEP_DEBUG_PRINT
        if (ps->run.grammar->debug_level > 2)
            fprintf(stderr, "++++Making error shift in set=%d\n", ps->pl_curr);
#endif
        build_new_set(ps, set, core_symb_vect, -1);
        ps->pl[++ps->pl_curr] = ps->new_set;
#ifndef NO_YAEP_DEBUG_PRINT
        if (ps->run.grammar->debug_level > 2)
	{
            fprintf(stderr, "++Trying new set=%d\n", ps->pl_curr);
            if (ps->run.grammar->debug_level > 3)
	    {
                set_print(ps, stderr, ps->new_set, ps->pl_curr, ps->run.grammar->debug_level > 4,
                          ps->run.grammar->debug_level > 5);
                fprintf(stderr, "\n");
	    }
	}
#endif
        /* Search the first right token.*/
        while(ps->tok_curr < ps->toks_len)
	{
            core_symb_vect = core_symb_vect_find(ps, ps->new_core, ps->toks[ps->tok_curr].symb);
            if (core_symb_vect != NULL)
                break;
#ifndef NO_YAEP_DEBUG_PRINT
            if (ps->run.grammar->debug_level > 2)
	    {
                fprintf(stderr, "++++++Skipping=%d ", ps->tok_curr);
                symb_print(stderr, ps->toks[ps->tok_curr].symb, TRUE);
                fprintf(stderr, "\n");
	    }
#endif
            cost++;
            ps->tok_curr++;
            if (cost >= best_cost)
                /* This state is worse.  Reject it.*/
                break;
	}
        if (cost >= best_cost)
	{
#ifndef NO_YAEP_DEBUG_PRINT
            if (ps->run.grammar->debug_level > 2)
                fprintf
                   (stderr,
                     "++++Too many ignored tokens %d(already worse recovery)\n",
                     cost);
#endif
            /* This state is worse.  Reject it.*/
            continue;
	}
        if (ps->tok_curr >= ps->toks_len)
	{
#ifndef NO_YAEP_DEBUG_PRINT
            if (ps->run.grammar->debug_level > 2)
                fprintf
                   (stderr,
                     "++++We achieved EOF without matching -- reject this state\n");
#endif
            /* Go to the next recovery state.  To guarantee that pl does
               not grows to much we don't push secondary error recovery
               states without matching in primary error recovery state.
               So we can say that pl length at most twice length of
               tokens array.*/
            continue;
	}
        /* Shift the found token.*/
        lookahead_term_num =(ps->tok_curr + 1 < ps->toks_len
                              ? ps->toks[ps->tok_curr + 1].symb->u.term.term_num : -1);
        build_new_set(ps, ps->new_set, core_symb_vect, lookahead_term_num);
        ps->pl[++ps->pl_curr] = ps->new_set;
#ifndef NO_YAEP_DEBUG_PRINT
        if (ps->run.grammar->debug_level > 3)
	{
            fprintf(stderr, "++++++++Building new set=%d\n", ps->pl_curr);
            if (ps->run.grammar->debug_level > 3)
                set_print(ps, stderr, ps->new_set, ps->pl_curr, ps->run.grammar->debug_level > 4,
                          ps->run.grammar->debug_level > 5);
	}
#endif
        n_matched_toks = 0;
        for(;;)
	{
#ifndef NO_YAEP_DEBUG_PRINT
            if (ps->run.grammar->debug_level > 2)
	    {
                fprintf(stderr, "++++++Matching=%d ", ps->tok_curr);
                symb_print(stderr, ps->toks[ps->tok_curr].symb, TRUE);
                fprintf(stderr, "\n");
	    }
#endif
            n_matched_toks++;
            if (n_matched_toks >= ps->run.grammar->recovery_token_matches)
                break;
            ps->tok_curr++;
            if (ps->tok_curr >= ps->toks_len)
                break;
            /* Push secondary recovery state(with error in set).*/
            if (core_symb_vect_find(ps, ps->new_core, ps->run.grammar->term_error) != NULL)
	    {
#ifndef NO_YAEP_DEBUG_PRINT
                if (ps->run.grammar->debug_level > 2)
		{
                    fprintf
                       (stderr,
                         "++++Found secondary state: original set=%d, tok=%d, ",
                         state.last_original_pl_el, ps->tok_curr);
                    symb_print(stderr, ps->toks[ps->tok_curr].symb, TRUE);
                    fprintf(stderr, "\n");
		}
#endif
                push_recovery_state(ps, state.last_original_pl_el, cost);
	    }
            core_symb_vect
                = core_symb_vect_find(ps, ps->new_core, ps->toks[ps->tok_curr].symb);
            if (core_symb_vect == NULL)
                break;
            lookahead_term_num =(ps->tok_curr + 1 < ps->toks_len
                                  ? ps->toks[ps->tok_curr + 1].symb->u.term.term_num
                                  : -1);
            build_new_set(ps, ps->new_set, core_symb_vect, lookahead_term_num);
            ps->pl[++ps->pl_curr] = ps->new_set;
	}
        if (n_matched_toks >= ps->run.grammar->recovery_token_matches
            || ps->tok_curr >= ps->toks_len)
	{
            /* We found an error recovery.  Compare costs.*/
            if (best_cost > cost)
	    {
#ifndef NO_YAEP_DEBUG_PRINT
                if (ps->run.grammar->debug_level > 2)
                    fprintf
                       (stderr,
                         "++++Ignore %d tokens(the best recovery now): Save it:\n",
                         cost);
#endif
                best_cost = cost;
                if (ps->tok_curr == ps->toks_len)
                    ps->tok_curr--;
                best_state = new_recovery_state(ps, state.last_original_pl_el,
                                                 /* It may be any constant here
                                                    because it is not used.*/
                                                 0);
               *start = ps->start_tok_curr - state.backward_move_cost;
               *stop =*start + cost;
	    }
#ifndef NO_YAEP_DEBUG_PRINT
            else if (ps->run.grammar->debug_level > 2)
                fprintf(stderr, "++++Ignore %d tokens(worse recovery)\n", cost);
#endif
	}
#ifndef NO_YAEP_DEBUG_PRINT
        else if (cost < best_cost && ps->run.grammar->debug_level > 2)
            fprintf(stderr, "++++No %d matched tokens  -- reject this state\n",
                     ps->run.grammar->recovery_token_matches);
#endif
    }
#ifndef NO_YAEP_DEBUG_PRINT
    if (ps->run.grammar->debug_level > 2)
        fprintf(stderr, "\n++Finishing error recovery: Restore best state\n");
#endif
    set_recovery_state(ps, &best_state);
#ifndef NO_YAEP_DEBUG_PRINT
    if (ps->run.grammar->debug_level > 2)
    {
        fprintf(stderr, "\n++Error recovery end: curr token %d=", ps->tok_curr);
        symb_print(stderr, ps->toks[ps->tok_curr].symb, TRUE);
        fprintf(stderr, ", Current set=%d:\n", ps->pl_curr);
        if (ps->run.grammar->debug_level > 3)
            set_print(ps, stderr, ps->pl[ps->pl_curr], ps->pl_curr, ps->run.grammar->debug_level > 4,
                      ps->run.grammar->debug_level > 5);
    }
#endif
    OS_DELETE(ps->recovery_state_tail_sets);
}

/* Initialize work with error recovery.*/
static void error_recovery_init(YaepParseState *ps)
{
    VLO_CREATE(ps->original_pl_tail_stack, ps->run.grammar->alloc, 4096);
    VLO_CREATE(ps->recovery_state_stack, ps->run.grammar->alloc, 4096);
}

/* Finalize work with error recovery.*/
static void error_recovery_fin(YaepParseState *ps)
{
    VLO_DELETE(ps->recovery_state_stack);
    VLO_DELETE(ps->original_pl_tail_stack);
}

/* Return TRUE if goto set SET from parsing list PLACE can be used as
   the next set.  The criterium is that all origin sets of start
   situations are the same as from PLACE. */
static int check_cached_transition_set(YaepParseState *ps, YaepSet*set, int place)
{
    int i, dist;
    int*dists = set->dists;

    for(i = set->core->n_start_sits - 1; i >= 0; i--)
    {
        if ((dist = dists[i]) <= 1)
            continue;
        /* Sets at origins of situations with distance one are supposed
           to be the same. */
        if (ps->pl[ps->pl_curr + 1 - dist] != ps->pl[place + 1 - dist])
            return FALSE;
    }
    return TRUE;
}

/* The following function is major function forming parsing list in
   Earley's algorithm.*/
static void build_pl(YaepParseState *ps)
{
    int i;
    YaepSymb*term;
    YaepSet*set;
    YaepCoreSymbVect*core_symb_vect;
    int lookahead_term_num;
#ifdef USE_SET_HASH_TABLE
    hash_table_entry_t*entry;
    YaepSetTermLookAhead*new_set_term_lookahead;
#endif

    error_recovery_init(ps);
    build_start_set(ps);
    lookahead_term_num = -1;
    for(ps->tok_curr = ps->pl_curr = 0; ps->tok_curr < ps->toks_len; ps->tok_curr++)
    {
        term = ps->toks[ps->tok_curr].symb;
        if (ps->run.grammar->lookahead_level != 0)
        {
            if (ps->tok_curr < ps->toks_len-1)
            {
                lookahead_term_num = ps->toks[ps->tok_curr+1].symb->u.term.term_num;
            }
            else
            {
                lookahead_term_num = -1;
            }
        }

#ifndef NO_YAEP_DEBUG_PRINT
        if (ps->run.grammar->debug_level > 2)
	{
            fprintf(stderr, "\nReading %d=", ps->tok_curr);
            symb_print(stderr, term, TRUE);
            fprintf(stderr, ", Current set=%d\n", ps->pl_curr);
	}
#endif
        set = ps->pl[ps->pl_curr];
        ps->new_set = NULL;
#ifdef USE_SET_HASH_TABLE
        OS_TOP_EXPAND(ps->set_term_lookahead_os, sizeof(YaepSetTermLookAhead));
        new_set_term_lookahead = (YaepSetTermLookAhead*) OS_TOP_BEGIN(ps->set_term_lookahead_os);
        new_set_term_lookahead->set = set;
        new_set_term_lookahead->term = term;
        new_set_term_lookahead->lookahead = lookahead_term_num;
        for(i = 0; i < MAX_CACHED_GOTO_RESULTS; i++)
        {
            new_set_term_lookahead->result[i] = NULL;
        }
        new_set_term_lookahead->curr = 0;
        entry = find_hash_table_entry(ps->set_term_lookahead_tab, new_set_term_lookahead, TRUE);
        if (*entry != NULL)
	{
            YaepSet*tab_set;

            OS_TOP_NULLIFY(ps->set_term_lookahead_os);
            for(i = 0; i < MAX_CACHED_GOTO_RESULTS; i++)
                if ((tab_set =
                    ((YaepSetTermLookAhead*)*entry)->result[i]) == NULL)
                    break;
                else if (check_cached_transition_set(ps,
                                                     tab_set,
                                                     ((YaepSetTermLookAhead*)*entry)->place[i]))
                {
                    ps->new_set = tab_set;
                    ps->n_goto_successes++;
                    break;
                }
	}
        else
	{
            OS_TOP_FINISH(ps->set_term_lookahead_os);
            *entry =(hash_table_entry_t) new_set_term_lookahead;
            ps->n_set_term_lookaheads++;
	}

#endif
        if (ps->new_set == NULL)
	{
            core_symb_vect = core_symb_vect_find(ps, set->core, term);
            if (core_symb_vect == NULL)
	    {
                int saved_tok_curr, start, stop;

                /* Error recovery.  We do not check transition vector
                   because for terminal transition vector is never NULL
                   and reduce is always NULL.*/
                saved_tok_curr = ps->tok_curr;
                if (ps->run.grammar->error_recovery_p)
		{
                    error_recovery(ps, &start, &stop);
                    ps->run.syntax_error(saved_tok_curr, ps->toks[saved_tok_curr].attr,
                                     start, ps->toks[start].attr, stop,
                                     ps->toks[stop].attr);
                    continue;
		}
                else
		{
                    ps->run.syntax_error(saved_tok_curr, ps->toks[saved_tok_curr].attr,
                                     -1, NULL, -1, NULL);
                    break;
		}
	    }
            build_new_set(ps, set, core_symb_vect, lookahead_term_num);
#ifdef USE_SET_HASH_TABLE
            /* Save(set, term, lookahead) -> new_set in the table. */
            i =((YaepSetTermLookAhead*)*entry)->curr;
           ((YaepSetTermLookAhead*)*entry)->result[i] = ps->new_set;
           ((YaepSetTermLookAhead*)*entry)->place[i] = ps->pl_curr;
           ((YaepSetTermLookAhead*)*entry)->lookahead = lookahead_term_num;
           ((YaepSetTermLookAhead*)*entry)->curr = (i + 1) % MAX_CACHED_GOTO_RESULTS;
#endif
	}
        ps->pl[++ps->pl_curr] = ps->new_set;
#ifndef NO_YAEP_DEBUG_PRINT
        if (ps->run.grammar->debug_level > 2)
	{
            fprintf(stderr, "New set=%d\n", ps->pl_curr);
            if (ps->run.grammar->debug_level > 3)
                set_print(ps, stderr, ps->new_set, ps->pl_curr, ps->run.grammar->debug_level > 4,
                          ps->run.grammar->debug_level > 5);
	}
#endif
    }
    error_recovery_fin(ps);
}

/* Hash of parse state.*/
static unsigned parse_state_hash(hash_table_entry_t s)
{
    YaepInternalParseState*state =((YaepInternalParseState*) s);

    /* The table contains only states with dot at the end of rule.*/
    assert(state->pos == state->rule->rhs_len);
    return(((jauquet_prime_mod32* hash_shift +
             (unsigned)(size_t) state->rule)* hash_shift +
             state->orig)* hash_shift + state->pl_ind);
}

/* Equality of parse states.*/
static int parse_state_eq(hash_table_entry_t s1, hash_table_entry_t s2)
{
    YaepInternalParseState*state1 =((YaepInternalParseState*) s1);
    YaepInternalParseState*state2 =((YaepInternalParseState*) s2);

    /* The table contains only states with dot at the end of rule.*/
    assert(state1->pos == state1->rule->rhs_len
            && state2->pos == state2->rule->rhs_len);
    return(state1->rule == state2->rule && state1->orig == state2->orig
            && state1->pl_ind == state2->pl_ind);
}

/* The following function initializes work with parser states.*/
static void parse_state_init(YaepParseState *ps)
{
    ps->free_parse_state = NULL;
    OS_CREATE(ps->parse_state_os, ps->run.grammar->alloc, 0);
    if (!ps->run.grammar->one_parse_p)
        ps->parse_state_tab =
            create_hash_table(ps->run.grammar->alloc, ps->toks_len* 2, parse_state_hash,
                               parse_state_eq);
}

/* The following function returns new parser state.*/
static YaepInternalParseState *parse_state_alloc(YaepParseState *ps)
{
    YaepInternalParseState*result;

    if (ps->free_parse_state == NULL)
    {
        OS_TOP_EXPAND(ps->parse_state_os, sizeof(YaepInternalParseState));
        result =(YaepInternalParseState*) OS_TOP_BEGIN(ps->parse_state_os);
        OS_TOP_FINISH(ps->parse_state_os);
    }
    else
    {
        result = ps->free_parse_state;
        ps->free_parse_state =(YaepInternalParseState*) ps->free_parse_state->rule;
    }
    return result;
}

/* The following function frees STATE.*/
static void parse_state_free(YaepParseState *ps, YaepInternalParseState*state)
{
    state->rule = (YaepRule*)ps->free_parse_state;
    ps->free_parse_state = state;
}

/* The following function searches for state in the table with the
   same characteristics as*STATE.  If it found it, it returns pointer
   to the state in the table.  Otherwise the function makes copy of
  *STATE, inserts into the table and returns pointer to copied state.
   In the last case, the function also sets up*NEW_P.*/
static YaepInternalParseState *parse_state_insert(YaepParseState *ps, YaepInternalParseState *state, int *new_p)
{
    hash_table_entry_t*entry;

    entry = find_hash_table_entry(ps->parse_state_tab, state, TRUE);

   *new_p = FALSE;
    if (*entry != NULL)
        return(YaepInternalParseState*)*entry;
   *new_p = TRUE;
    /* We make copy because pl_ind can be changed in further processing
       state.*/
   *entry = parse_state_alloc(ps);
   *(YaepInternalParseState*)*entry =*state;
    return(YaepInternalParseState*)*entry;
}

/* The following function finalizes work with parser states.*/
static void parse_state_fin(YaepParseState *ps)
{
    if (!ps->run.grammar->one_parse_p)
        delete_hash_table(ps->parse_state_tab);
    OS_DELETE(ps->parse_state_os);
}



#ifndef NO_YAEP_DEBUG_PRINT

/* This page conatins code to traverse translation.*/

/* Hash of translation visit node.*/
static unsigned trans_visit_node_hash(hash_table_entry_t n)
{
    return(size_t)((YaepTransVisitNode*) n)->node;
}

/* Equality of translation visit nodes.*/
static int trans_visit_node_eq(hash_table_entry_t n1, hash_table_entry_t n2)
{
    return(((YaepTransVisitNode*) n1)->node == ((YaepTransVisitNode*) n2)->node);
}

/* The following function checks presence translation visit node with
   given NODE in the table and if it is not present in the table, the
   function creates the translation visit node and inserts it into
   the table.*/
static YaepTransVisitNode *visit_node(YaepParseState *ps, YaepTreeNode*node)
{
    YaepTransVisitNode trans_visit_node;
    hash_table_entry_t*entry;

    trans_visit_node.node = node;
    entry = find_hash_table_entry(ps->trans_visit_nodes_tab,
                                   &trans_visit_node, TRUE);

    if (*entry == NULL)
    {
        /* If it is the new node, we did not visit it yet.*/
        trans_visit_node.num = -1 - ps->n_trans_visit_nodes;
        ps->n_trans_visit_nodes++;
        OS_TOP_ADD_MEMORY(ps->trans_visit_nodes_os,
                           &trans_visit_node, sizeof(trans_visit_node));
       *entry =(hash_table_entry_t) OS_TOP_BEGIN(ps->trans_visit_nodes_os);
        OS_TOP_FINISH(ps->trans_visit_nodes_os);
    }
    return(YaepTransVisitNode*)*entry;
}

/* The following function returns the positive order number of node with number NUM.*/
static int canon_node_num(int num)
{
    return (num < 0 ? -num - 1 : num);
}

/* The following recursive function prints NODE into file F and prints
   all its children(if debug_level < 0 output format is for
   graphviz).*/
static void print_yaep_node(YaepParseState *ps, FILE *f, YaepTreeNode *node)
{
    YaepTransVisitNode*trans_visit_node;
    YaepTreeNode*child;
    int i;

    assert(node != NULL);
    trans_visit_node = visit_node(ps, node);
    if (trans_visit_node->num >= 0)
        return;
    trans_visit_node->num = -trans_visit_node->num - 1;
    if (ps->run.grammar->debug_level > 0)
        fprintf(f, "%7d: ", trans_visit_node->num);
    switch(node->type)
    {
    case YAEP_NIL:
        if (ps->run.grammar->debug_level > 0)
            fprintf(f, "EMPTY\n");
        break;
    case YAEP_ERROR:
        if (ps->run.grammar->debug_level > 0)
            fprintf(f, "ERROR\n");
        break;
    case YAEP_TERM:
        if (ps->run.grammar->debug_level > 0)
            fprintf(f, "TERMINAL: code=%d, repr=%s, mark=%d %c\n", node->val.term.code,
                    symb_find_by_code(ps, node->val.term.code)->repr, node->val.term.mark, node->val.term.mark>32?node->val.term.mark:' ');
        break;
    case YAEP_ANODE:
        if (ps->run.grammar->debug_level > 0)
	{
            fprintf(f, "ABSTRACT: %c%s(", node->val.anode.mark?node->val.anode.mark:' ', node->val.anode.name);
            for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
                fprintf(f, " %d", canon_node_num(visit_node(ps, child)->num));
	}
        else
	{
            for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
	    {
                fprintf(f, "  \"%d: %s\" -> \"%d: ", trans_visit_node->num,
                         node->val.anode.name,
                        canon_node_num(visit_node(ps, child)->num));
                switch(child->type)
		{
		case YAEP_NIL:
                    fprintf(f, "EMPTY");
                    break;
		case YAEP_ERROR:
                    fprintf(f, "ERROR");
                    break;
		case YAEP_TERM:
                    fprintf(f, "%s",
                            symb_find_by_code(ps, child->val.term.code)->repr);
                    break;
		case YAEP_ANODE:
                    fprintf(f, "%s", child->val.anode.name);
                    break;
		case YAEP_ALT:
                    fprintf(f, "ALT");
                    break;
		default:
                    assert(FALSE);
		}
                fprintf(f, "\";\n");
	    }
	}
        for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
            print_yaep_node(ps, f, child);
        break;
    case YAEP_ALT:
        if (ps->run.grammar->debug_level > 0)
	{
            fprintf(f, "ALTERNATIVE: node=%d, next=",
                    canon_node_num(visit_node(ps, node->val.alt.node)->num));
            if (node->val.alt.next != NULL)
                fprintf(f, "%d\n",
                        canon_node_num(visit_node(ps, node->val.alt.next)->num));
            else
                fprintf(f, "nil\n");
	}
        else
	{
            fprintf(f, "  \"%d: ALT\" -> \"%d: ", trans_visit_node->num,
                    canon_node_num(visit_node(ps, node->val.alt.node)->num));
            switch(node->val.alt.node->type)
	    {
	    case YAEP_NIL:
                fprintf(f, "EMPTY");
                break;
	    case YAEP_ERROR:
                fprintf(f, "ERROR");
                break;
	    case YAEP_TERM:
                fprintf(f, "%s",
                        symb_find_by_code(ps, node->val.alt.node->val.term.code)->
                         repr);
                break;
	    case YAEP_ANODE:
                fprintf(f, "%s", node->val.alt.node->val.anode.name);
                break;
	    case YAEP_ALT:
                fprintf(f, "ALT");
                break;
	    default:
                assert(FALSE);
	    }
            fprintf(f, "\";\n");
            if (node->val.alt.next != NULL)
                fprintf(f, "  \"%d: ALT\" -> \"%d: ALT\";\n",
                         trans_visit_node->num,
                        canon_node_num(visit_node(ps, node->val.alt.next)->num));
	}
        print_yaep_node(ps, f, node->val.alt.node);
        if (node->val.alt.next != NULL)
            print_yaep_node(ps, f, node->val.alt.next);
        break;
    default:
        assert(FALSE);
    }
}

/* The following function prints parse tree with ROOT.*/
static void print_parse(YaepParseState *ps, FILE* f, YaepTreeNode*root)
{
    ps->trans_visit_nodes_tab =
        create_hash_table(ps->run.grammar->alloc, ps->toks_len* 2, trans_visit_node_hash,
                           trans_visit_node_eq);

    ps->n_trans_visit_nodes = 0;
    OS_CREATE(ps->trans_visit_nodes_os, ps->run.grammar->alloc, 0);
    print_yaep_node(ps, f, root);
    OS_DELETE(ps->trans_visit_nodes_os);
    delete_hash_table(ps->trans_visit_nodes_tab);
}

#endif

/* The following function places translation NODE into *PLACE and
   creates alternative nodes if it is necessary. */
static void place_translation(YaepParseState *ps, YaepTreeNode **place, YaepTreeNode *node)
{
    YaepTreeNode *alt, *next_alt;

    assert(place != NULL);
    if (*place == NULL)
    {
        TRACE_FA("immediate %p %p", place, node);
        *place = node;
        return;
    }
    /* We need an alternative.*/
#ifndef NO_YAEP_DEBUG_PRINT
    ps->n_parse_alt_nodes++;
#endif
    alt =(YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode));
    alt->type = YAEP_ALT;
    alt->val.alt.node = node;
    if ((*place)->type == YAEP_ALT)
    {
        alt->val.alt.next =*place;
    }
    else
    {
        /* We need alternative node for the 1st
           alternative too.*/
        ps->n_parse_alt_nodes++;
        next_alt = alt->val.alt.next
            =((YaepTreeNode*)
              (*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
        next_alt->type = YAEP_ALT;
        next_alt->val.alt.node =*place;
        next_alt->val.alt.next = NULL;
    }
   *place = alt;

   TRACE_FA("ind %p %p", place, node);
}

static YaepTreeNode *copy_anode(YaepParseState *ps,
                                YaepTreeNode **place,
                                YaepTreeNode *anode,
                                YaepRule *rule,
                                int disp)
{
    YaepTreeNode*node;
    int i;

    TRACE_F;

    node = ((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)
                                              + sizeof(YaepTreeNode*)
                                              *(rule->trans_len + 1)));
   *node =*anode;
    node->val.anode.children = ((YaepTreeNode**)((char*) node + sizeof(YaepTreeNode)));
    for(i = 0; i <= rule->trans_len; i++)
    {
        node->val.anode.children[i] = anode->val.anode.children[i];
    }
    node->val.anode.children[disp] = NULL;
    place_translation(ps, place, node);

    return node;
}

/* The hash of the memory reference. */
static unsigned reserv_mem_hash(hash_table_entry_t m)
{
    return (size_t)m;
}

/* The equity of the memory reference. */
static int reserv_mem_eq(hash_table_entry_t m1, hash_table_entry_t m2)
{
    return m1 == m2;
}

/* The following function sets up minimal cost for each abstract node.
   The function returns minimal translation corresponding to NODE.
   The function also collects references to memory which can be
   freed. Remeber that the translation is DAG, altenatives form lists
  (alt node may not refer for another alternative).*/
static YaepTreeNode *prune_to_minimal(YaepParseState *ps, YaepTreeNode *node, int *cost)
{
    YaepTreeNode*child,*alt,*next_alt,*result = NULL;
    int i, min_cost = INT_MAX;

    TRACE_F;

    assert(node != NULL);
    switch(node->type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
    case YAEP_TERM:
        if (ps->run.parse_free != NULL)
        {
            VLO_ADD_MEMORY(ps->tnodes_vlo, &node, sizeof(node));
        }
       *cost = 0;
        return node;
    case YAEP_ANODE:
        if (node->val.anode.cost >= 0)
	{
            if (ps->run.parse_free != NULL)
            {
                VLO_ADD_MEMORY(ps->tnodes_vlo, &node, sizeof(node));
            }
            for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
	    {
                node->val.anode.children[i] = prune_to_minimal(ps, child, cost);
                node->val.anode.cost +=*cost;
	    }
           *cost = node->val.anode.cost;
            node->val.anode.cost = -node->val.anode.cost - 1;	/* flag of visit*/
	}
        return node;
    case YAEP_ALT:
        for(alt = node; alt != NULL; alt = next_alt)
	{
            if (ps->run.parse_free != NULL)
            {
                VLO_ADD_MEMORY(ps->tnodes_vlo, &alt, sizeof(alt));
            }
            next_alt = alt->val.alt.next;
            alt->val.alt.node = prune_to_minimal(ps, alt->val.alt.node, cost);
            if (alt == node || min_cost >*cost)
	    {
                min_cost =*cost;
                alt->val.alt.next = NULL;
                result = alt;
	    }
            else if (min_cost ==*cost && !ps->run.grammar->one_parse_p)
	    {
                alt->val.alt.next = result;
                result = alt;
	    }
	}
       *cost = min_cost;
        return(result->val.alt.next == NULL ? result->val.alt.node : result);
    default:
        assert(FALSE);
    }
   *cost = 0;
    return NULL;
}

/* The following function traverses the translation collecting
   reference to memory which may not be freed.*/
static void traverse_pruned_translation(YaepParseState *ps, YaepTreeNode *node)
{
    YaepTreeNode*child;
    hash_table_entry_t*entry;
    int i;

next:
    assert(node != NULL);
    if (ps->run.parse_free != NULL && *(entry = find_hash_table_entry(ps->reserv_mem_tab, node, TRUE)) == NULL)
    {
       *entry = (hash_table_entry_t)node;
    }
    switch(node->type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
    case YAEP_TERM:
        break;
    case YAEP_ANODE:
        if (ps->run.parse_free != NULL && *(entry = find_hash_table_entry(ps->reserv_mem_tab,
                                                                      node->val.anode.name,
                                                                      TRUE)) == NULL)
        {
            *entry =(hash_table_entry_t) node->val.anode.name;
        }
        for(i = 0;(child = node->val.anode.children[i]) != NULL; i++)
        {
            traverse_pruned_translation(ps, child);
        }
        assert(node->val.anode.cost < 0);
        node->val.anode.cost = -node->val.anode.cost - 1;
        break;
    case YAEP_ALT:
        traverse_pruned_translation(ps, node->val.alt.node);
        if ((node = node->val.alt.next) != NULL)
            goto next;
        break;
    default:
        assert(FALSE);
    }

    TRACE_F;

    return;
}

/* The function finds and returns a minimal cost parse(s). */
static YaepTreeNode *find_minimal_translation(YaepParseState *ps, YaepTreeNode *root)
{
    YaepTreeNode**node_ptr;
    int cost;

    if (ps->run.parse_free != NULL)
    {
        ps->reserv_mem_tab = create_hash_table(ps->run.grammar->alloc, ps->toks_len* 4, reserv_mem_hash,
                                           reserv_mem_eq);

        VLO_CREATE(ps->tnodes_vlo, ps->run.grammar->alloc, ps->toks_len* 4* sizeof(void*));
    }
    root = prune_to_minimal(ps, root, &cost);
    traverse_pruned_translation(ps, root);
    if (ps->run.parse_free != NULL)
    {
        for(node_ptr =(YaepTreeNode**) VLO_BEGIN(ps->tnodes_vlo);
             node_ptr <(YaepTreeNode**) VLO_BOUND(ps->tnodes_vlo);
             node_ptr++)
            if (*find_hash_table_entry(ps->reserv_mem_tab,*node_ptr, TRUE) == NULL)
            {
                if ((*node_ptr)->type == YAEP_ANODE
                    &&*find_hash_table_entry(ps->reserv_mem_tab,
					      (*node_ptr)->val.anode.name,
					       TRUE) == NULL)
                {
                   (*ps->run.parse_free)((void*)(*node_ptr)->val.anode.name);
                }
               (*ps->run.parse_free)(*node_ptr);
            }
        VLO_DELETE(ps->tnodes_vlo);
        delete_hash_table(ps->reserv_mem_tab);
    }

    TRACE_F;

    return root;
}

/* The following function finds parse tree of parsed input.  The
   function sets up*AMBIGUOUS_P if we found that the grammer is
   ambigous(it works even we asked only one parse tree without
   alternatives).*/
static YaepTreeNode *make_parse(YaepParseState *ps, int *ambiguous_p)
{
    YaepSet *set, *check_set;
    YaepSetCore *set_core, *check_set_core;
    YaepSituation *sit, *check_sit;
    YaepRule *rule, *sit_rule;
    YaepSymb *symb;
    YaepCoreSymbVect *core_symb_vect, *check_core_symb_vect;
    int i, j, k, found, pos, orig, pl_ind, n_candidates, disp;
    int sit_ind, check_sit_ind, sit_orig, check_sit_orig, new_p;
    YaepInternalParseState *state, *orig_state, *curr_state;
    YaepInternalParseState *table_state, *parent_anode_state;
    YaepInternalParseState root_state;
    YaepTreeNode *result, *empty_node, *node, *error_node;
    YaepTreeNode *parent_anode, *anode, root_anode;
    int parent_disp;
    int saved_one_parse_p;
    YaepTreeNode **term_node_array = NULL;
    vlo_t stack, orig_states;

    ps->n_parse_term_nodes = ps->n_parse_abstract_nodes = ps->n_parse_alt_nodes = 0;
    set = ps->pl[ps->pl_curr];
    assert(ps->run.grammar->axiom != NULL);
    /* We have only one start situation: "$S : <start symb> $eof .". */
    sit =(set->core->sits != NULL ? set->core->sits[0] : NULL);
    if (sit == NULL
        || set->dists[0] != ps->pl_curr
        || sit->rule->lhs != ps->run.grammar->axiom || sit->pos != sit->rule->rhs_len)
    {
        /* It is possible only if error recovery is switched off.
           Because we always adds rule `axiom: error $eof'.*/
        assert(!ps->run.grammar->error_recovery_p);
        return NULL;
    }
    saved_one_parse_p = ps->run.grammar->one_parse_p;
    if (ps->run.grammar->cost_p)
        /* We need all parses to choose the minimal one*/
        ps->run.grammar->one_parse_p = FALSE;
    sit = set->core->sits[0];
    parse_state_init(ps);
    if (!ps->run.grammar->one_parse_p)
    {
        void*mem;

        /* We need this array to reuse terminal nodes only for
           generation of several parses.*/
        mem = yaep_malloc(ps->run.grammar->alloc,
                          sizeof(YaepTreeNode*)* ps->toks_len);
        term_node_array =(YaepTreeNode**) mem;
        for(i = 0; i < ps->toks_len; i++)
        {
            term_node_array[i] = NULL;
        }
        /* The following is used to check necessity to create current
           state with different pl_ind.*/
        VLO_CREATE(orig_states, ps->run.grammar->alloc, 0);
    }
    VLO_CREATE(stack, ps->run.grammar->alloc, 10000);
    VLO_EXPAND(stack, sizeof(YaepInternalParseState*));
    state = parse_state_alloc(ps);
   ((YaepInternalParseState**) VLO_BOUND(stack))[-1] = state;
    rule = state->rule = sit->rule;
    state->pos = sit->pos;
    state->orig = 0;
    state->pl_ind = ps->pl_curr;
    result = NULL;
    root_state.anode = &root_anode;
    root_anode.val.anode.children = &result;
    state->parent_anode_state = &root_state;
    state->parent_disp = 0;
    state->anode = NULL;
    /* Create empty and error node:*/
    empty_node =((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
    empty_node->type = YAEP_NIL;
    empty_node->val.nil.used = 0;
    error_node =((YaepTreeNode*)(*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
    error_node->type = YAEP_ERROR;
    error_node->val.error.used = 0;
    while(VLO_LENGTH(stack) != 0)
    {
#if !defined(NDEBUG) && !defined(NO_YAEP_DEBUG_PRINT)
        if ((ps->run.grammar->debug_level > 2 && state->pos == state->rule->rhs_len)
            || ps->run.grammar->debug_level > 3)
	{
            fprintf(stderr, "Processing top %ld, set place = %d, sit = ",
                    (long) VLO_LENGTH(stack) / sizeof(YaepInternalParseState*) - 1,
                     state->pl_ind);
            rule_dot_print(ps, stderr, state->rule, state->pos);
            fprintf(stderr, ", %d\n", state->orig);
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
            /* We've processed all rhs of the rule.*/
#if !defined(NDEBUG) && !defined(NO_YAEP_DEBUG_PRINT)
            if ((ps->run.grammar->debug_level > 2 && state->pos == state->rule->rhs_len)
                || ps->run.grammar->debug_level > 3)
	    {
                fprintf(stderr, "Poping top %ld, set place = %d, sit = ",
                        (long) VLO_LENGTH(stack) / sizeof(YaepInternalParseState*) - 1,
                         state->pl_ind);
                rule_dot_print(ps, stderr, state->rule, 0);
                fprintf(stderr, ", %d\n", state->orig);
	    }
#endif
            parse_state_free(ps, state);
            VLO_SHORTEN(stack, sizeof(YaepInternalParseState*));
            if (VLO_LENGTH(stack) != 0)
                state =((YaepInternalParseState**) VLO_BOUND(stack))[-1];
            if (parent_anode != NULL && rule->trans_len == 0 && anode == NULL)
	    {
                /* We do produce nothing but we should.  So write empty
                   node.*/
                place_translation(ps, parent_anode->val.anode.children + parent_disp, empty_node);
                empty_node->val.nil.used = 1;
	    }
            else if (anode != NULL)
	    {
                /* Change NULLs into empty nodes.  We can not make it
                   the first time because when building several parses
                   the NULL means flag of absence of translations(see
                   function `place_translation').*/
                for(i = 0; i < rule->trans_len; i++)
                    if (anode->val.anode.children[i] == NULL)
                    {
                        anode->val.anode.children[i] = empty_node;
                        empty_node->val.nil.used = 1;
                    }
	    }
            continue;
	}
        assert(pos >= 0);
        if ((symb = rule->rhs[pos])->term_p)
	{
            /* Terminal before dot:*/
            pl_ind--;		/* l*/
            /* Because of error recovery toks [pl_ind].symb may be not
               equal to symb.*/
            assert(ps->toks[pl_ind].symb == symb);
            if (parent_anode != NULL && disp >= 0)
	    {
                /* We should generate and use the translation of the
                   terminal.  Add reference to the current node.*/
                if (symb == ps->run.grammar->term_error)
		{
                    node = error_node;
                    error_node->val.error.used = 1;
		}
                else if (!ps->run.grammar->one_parse_p
                         &&(node = term_node_array[pl_ind]) != NULL)
                    ;
                else
		{
                    ps->n_parse_term_nodes++;
                    node =((YaepTreeNode*)
                           (*ps->run.parse_alloc)(sizeof(YaepTreeNode)));
                    node->type = YAEP_TERM;
                    node->val.term.code = symb->u.term.code;
                    // IXML
                    if (rule->marks && rule->marks[pos])
                    {
                        // Copy the mark from the rhs position on to the terminal.
                        node->val.term.mark = rule->marks[pos];
                    }
                    node->val.term.attr = ps->toks[pl_ind].attr;
                    if (!ps->run.grammar->one_parse_p)
                        term_node_array[pl_ind] = node;
		}
                place_translation(ps,
                                  anode != NULL ?
                                  anode->val.anode.children + disp
                                  : parent_anode->val.anode.children + parent_disp, node);
	    }
            if (pos != 0)
                state->pl_ind = pl_ind;
            continue;
	}
        /* Nonterminal before dot:*/
        set = ps->pl[pl_ind];
        set_core = set->core;
        core_symb_vect = core_symb_vect_find(ps, set_core, symb);
        assert(core_symb_vect->reduces.len != 0);
        n_candidates = 0;
        orig_state = state;
        if (!ps->run.grammar->one_parse_p)
            VLO_NULLIFY(orig_states);
        for(i = 0; i < core_symb_vect->reduces.len; i++)
	{
            sit_ind = core_symb_vect->reduces.els[i];
            sit = set_core->sits[sit_ind];
            if (sit_ind < set_core->n_start_sits)
                sit_orig = pl_ind - set->dists[sit_ind];
            else if (sit_ind < set_core->n_all_dists)
                sit_orig = pl_ind - set->dists[set_core->parent_indexes[sit_ind]];
            else
                sit_orig = pl_ind;
#if !defined(NDEBUG) && !defined(NO_YAEP_DEBUG_PRINT)
            if (ps->run.grammar->debug_level > 3)
	    {
                fprintf(stderr, "    Trying set place = %d, sit = ", pl_ind);
                sit_print(ps, stderr, sit, ps->run.grammar->debug_level > 5);
                fprintf(stderr, ", %d\n", sit_orig);
	    }
#endif
            check_set = ps->pl[sit_orig];
            check_set_core = check_set->core;
            check_core_symb_vect = core_symb_vect_find(ps, check_set_core, symb);
            assert(check_core_symb_vect != NULL);
            found = FALSE;
            for(j = 0; j < check_core_symb_vect->transitions.len; j++)
	    {
                check_sit_ind = check_core_symb_vect->transitions.els[j];
                check_sit = check_set->core->sits[check_sit_ind];
                if (check_sit->rule != rule || check_sit->pos != pos)
                    continue;
                check_sit_orig = sit_orig;
                if (check_sit_ind < check_set_core->n_all_dists)
		{
                    if (check_sit_ind < check_set_core->n_start_sits)
                        check_sit_orig
                            = sit_orig - check_set->dists[check_sit_ind];
                    else
                        check_sit_orig
                            =(sit_orig
                               - check_set->dists[check_set_core->parent_indexes
                                                  [check_sit_ind]]);
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
                if (ps->run.grammar->one_parse_p)
                    break;
	    }
            sit_rule = sit->rule;
            if (n_candidates == 0)
                orig_state->pl_ind = sit_orig;
            if (parent_anode != NULL && disp >= 0)
	    {
                /* We should generate and use the translation of the
                   nonterminal.*/
                curr_state = orig_state;
                anode = orig_state->anode;
                /* We need translation of the rule.*/
                if (n_candidates != 0)
		{
                    assert(!ps->run.grammar->one_parse_p);
                    if (n_candidates == 1)
		    {
                        VLO_EXPAND(orig_states, sizeof(YaepInternalParseState*));
                       ((YaepInternalParseState**) VLO_BOUND(orig_states))[-1]
                            = orig_state;
		    }
                    for(j =(VLO_LENGTH(orig_states)
                              / sizeof(YaepInternalParseState*) - 1); j >= 0; j--)
                        if (((YaepInternalParseState**)
                             VLO_BEGIN(orig_states))[j]->pl_ind == sit_orig)
                            break;
                    if (j >= 0)
		    {
                        /* [A -> x., n] & [A -> y., n]*/
                        curr_state =((YaepInternalParseState**)
                                      VLO_BEGIN(orig_states))[j];
                        anode = curr_state->anode;
		    }
                    else
		    {
                        /* [A -> x., n] & [A -> y., m] where n != m.*/
                        /* It is different from the previous ones so add
                           it to process.*/
                        state = parse_state_alloc(ps);
                        VLO_EXPAND(stack, sizeof(YaepInternalParseState*));
                       ((YaepInternalParseState**) VLO_BOUND(stack))[-1] = state;
                       *state =*orig_state;
                        state->pl_ind = sit_orig;
                        if (anode != NULL)
                            state->anode
                                = copy_anode(ps, parent_anode->val.anode.children
                                              + parent_disp, anode, rule, disp);
                        VLO_EXPAND(orig_states, sizeof(YaepInternalParseState*));
                       ((YaepInternalParseState**) VLO_BOUND(orig_states))[-1]
                            = state;
#if !defined(NDEBUG) && !defined(NO_YAEP_DEBUG_PRINT)
                        if (ps->run.grammar->debug_level > 3)
			{
                            fprintf(stderr,
                                     "  Adding top %ld, set place = %d, modified sit = ",
                                    (long) VLO_LENGTH(stack) /
                                     sizeof(YaepInternalParseState*) - 1,
                                     sit_orig);
                            rule_dot_print(ps, stderr, state->rule, state->pos);
                            fprintf(stderr, ", %d\n", state->orig);
			}
#endif
                        curr_state = state;
                        anode = state->anode;
		    }
		}		/* if (n_candidates != 0)*/
                if (sit_rule->anode != NULL)
		{
                    /* This rule creates abstract node.*/
                    state = parse_state_alloc(ps);
                    state->rule = sit_rule;
                    state->pos = sit->pos;
                    state->orig = sit_orig;
                    state->pl_ind = pl_ind;
                    table_state = NULL;
                    if (!ps->run.grammar->one_parse_p)
                        table_state = parse_state_insert(ps, state, &new_p);
                    if (table_state == NULL || new_p)
		    {
                        /* We need new abtract node.*/
                        ps->n_parse_abstract_nodes++;
                        node
                            =((YaepTreeNode*)
                              (*ps->run.parse_alloc)(sizeof(YaepTreeNode)
                                               + sizeof(YaepTreeNode*)
                                              *(sit_rule->trans_len + 1)));
                        state->anode = node;
                        if (table_state != NULL)
                            table_state->anode = node;
                        node->type = YAEP_ANODE;
                        if (sit_rule->caller_anode == NULL)
			{
                            sit_rule->caller_anode
                                =((char*)
                                  (*ps->run.parse_alloc)(strlen(sit_rule->anode) + 1));
                            strcpy(sit_rule->caller_anode, sit_rule->anode);
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
                            =((YaepTreeNode**)
                              ((char*) node + sizeof(YaepTreeNode)));
                        for(k = 0; k <= sit_rule->trans_len; k++)
                            node->val.anode.children[k] = NULL;
                        VLO_EXPAND(stack, sizeof(YaepInternalParseState*));
                       ((YaepInternalParseState**) VLO_BOUND(stack))[-1] = state;
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
#if !defined(NDEBUG) && !defined(NO_YAEP_DEBUG_PRINT)
                        if (ps->run.grammar->debug_level > 3)
			{
                            fprintf(stderr,
                                     "  Adding top %ld, set place = %d, sit = ",
                                    (long) VLO_LENGTH(stack) /
                                     sizeof(YaepInternalParseState*) - 1, pl_ind);
                            sit_print(ps, stderr, sit, ps->run.grammar->debug_level > 5);
                            fprintf(stderr, ", %d\n", sit_orig);
			}
#endif
		    }
                    else
		    {
                        /* We allready have the translation.*/
                        assert(!ps->run.grammar->one_parse_p);
                        parse_state_free(ps, state);
                        state =((YaepInternalParseState**) VLO_BOUND(stack))[-1];
                        node = table_state->anode;
                        assert(node != NULL);
#if !defined(NDEBUG) && !defined(NO_YAEP_DEBUG_PRINT)
                        if (ps->run.grammar->debug_level > 3)
			{
                            fprintf(stderr,
                                     "  Found prev. translation: set place = %d, sit = ",
                                     pl_ind);
                            sit_print(ps, stderr, sit, ps->run.grammar->debug_level > 5);
                            fprintf(stderr, ", %d\n", sit_orig);
			}
#endif
		    }
                    place_translation(ps, anode == NULL
                                       ? parent_anode->val.anode.children
                                       + parent_disp
                                       : anode->val.anode.children + disp,
                                       node);
		}		/* if (sit_rule->anode != NULL)*/
                else if (sit->pos != 0)
		{
                    /* We should generate and use the translation of the
                       nonterminal.  Add state to get a translation.*/
                    state = parse_state_alloc(ps);
                    VLO_EXPAND(stack, sizeof(YaepInternalParseState*));
                   ((YaepInternalParseState**) VLO_BOUND(stack))[-1] = state;
                    state->rule = sit_rule;
                    state->pos = sit->pos;
                    state->orig = sit_orig;
                    state->pl_ind = pl_ind;
                    state->parent_anode_state =(anode == NULL
                                                 ? curr_state->
                                                 parent_anode_state :
                                                 curr_state);
                    state->parent_disp = anode == NULL ? parent_disp : disp;
                    state->anode = NULL;
#if !defined(NDEBUG) && !defined(NO_YAEP_DEBUG_PRINT)
                    if (ps->run.grammar->debug_level > 3)
		    {
                        fprintf(stderr,
                                 "  Adding top %ld, set place = %d, sit = ",
                                (long) VLO_LENGTH(stack) /
                                 sizeof(YaepInternalParseState*) - 1, pl_ind);
                        sit_print(ps, stderr, sit, ps->run.grammar->debug_level > 5);
                        fprintf(stderr, ", %d\n", sit_orig);
		    }
#endif
		}
                else
		{
                    /* Empty rule should produce something not abtract
                       node.  So place empty node.*/
                    place_translation(ps, anode == NULL
                                       ? parent_anode->val.anode.children
                                       + parent_disp
                                       : anode->val.anode.children + disp,
                                       empty_node);
                    empty_node->val.nil.used = 1;
		}
	    }			/* if (parent_anode != NULL && disp >= 0)*/
            n_candidates++;
	}			/* For all reduces of the nonterminal.*/
        /* We should have a parse.*/
        assert(n_candidates != 0 && (!ps->run.grammar->one_parse_p || n_candidates == 1));
    } /* For all parser states.*/
    VLO_DELETE(stack);
    if (!ps->run.grammar->one_parse_p)
    {
        VLO_DELETE(orig_states);
        yaep_free(ps->run.grammar->alloc, term_node_array);
    }
    parse_state_fin(ps);
    ps->run.grammar->one_parse_p = saved_one_parse_p;
    if (ps->run.grammar->cost_p &&*ambiguous_p)
        /* We can not build minimal tree during building parsing list
           because we have not the translation yet.  We can not make it
           during parsing because the abstract nodes are created before
           their children.*/
        result = find_minimal_translation(ps, result);
#ifndef NO_YAEP_DEBUG_PRINT
    if (ps->run.grammar->debug_level > 1)
    {
        fprintf(stderr, "Translation:\n");
        print_parse(ps, stderr, result);
        fprintf(stderr, "\n");
    }
    else if (ps->run.grammar->debug_level < 0)
    {
        fprintf(stderr, "digraph CFG {\n");
        fprintf(stderr, "  node [shape=ellipse, fontsize=200];\n");
        fprintf(stderr, "  ratio=fill;\n");
        fprintf(stderr, "  ordering=out;\n");
        fprintf(stderr, "  page = \"8.5, 11\"; // inches\n");
        fprintf(stderr, "  size = \"7.5, 10\"; // inches\n\n");
        print_parse(ps, stderr, result);
        fprintf(stderr, "}\n");
    }
#endif

    /* Free empty and error node if they have not been used*/
    if (ps->run.parse_free != NULL)
    {
        if (!empty_node->val.nil.used)
	{
            ps->run.parse_free(empty_node);
	}
        if (!error_node->val.error.used)
	{
            ps->run.parse_free(error_node);
	}
    }

    assert(result != NULL && (!ps->run.grammar->one_parse_p || ps->n_parse_alt_nodes == 0));

    return result;
}

static void *parse_alloc_default(int nmemb)
{
    void *result;

    assert(nmemb > 0);

    result = malloc(nmemb);
    if (result == NULL)
    {
        exit(1);
    }

    return result;
}

static void parse_free_default(void *mem)
{
    free(mem);
}

/* The following function parses input according read grammar.
   ONE_PARSE_FLAG means build only one parse tree.  For unambiguous
   grammar the flag does not affect the result.  LA_LEVEL means usage
   of statik(if 1) or dynamic(2) lookahead to decrease size of sets.
   Static lookaheads gives the best results with the point of space
   and speed, dynamic ones does sligthly worse, and no usage of
   lookaheds does the worst.  D_LEVEL says what debugging information
   to output(it works only if we compiled without defined macro
   NO_YAEP_DEBUG_PRINT).  The function returns the error code(which
   will be also in error_code).  The function sets up
  *AMBIGUOUS_P if we found that the grammer is ambigous(it works even
   we asked only one parse tree without alternatives).*/
int yaepParse(YaepParseRun *pr, YaepGrammar *g)
{
    YaepParseState *ps = (YaepParseState*)pr;

    assert(CHECK_PARSE_STATE_MAGIC(ps));

    ps->run.grammar = g;
    YaepTreeNode **root = &ps->run.root;
    int *ambiguous_p = &ps->run.ambiguous_p;

    int code, tok_init_p, parse_init_p;
    int tab_collisions, tab_searches;

    /* Set up parse allocation*/
    if (ps->run.parse_alloc == NULL)
    {
        if (ps->run.parse_free != NULL)
	{
            /* Cannot allocate memory with a null function*/
            return YAEP_NO_MEMORY;
	}
        /* Set up defaults*/
        ps->run.parse_alloc = parse_alloc_default;
        ps->run.parse_free = parse_free_default;
    }

    assert(ps->run.grammar != NULL);
   *root = NULL;
   *ambiguous_p = FALSE;
    pl_init(ps);
    tok_init_p = parse_init_p = FALSE;
    if ((code = setjmp(error_longjump_buff)) != 0)
    {
        pl_fin(ps);
        if (parse_init_p)
            yaep_parse_fin(ps);
        if (tok_init_p)
            tok_fin(ps);
        return code;
    }
    if (g->undefined_p)
    {
        yaep_error(ps, YAEP_UNDEFINED_OR_BAD_GRAMMAR, "undefined or bad grammar");
    }
    ps->n_goto_successes = 0;
    tok_init(ps);
    tok_init_p = TRUE;
    read_toks(ps);
    yaep_parse_init(ps, ps->toks_len);
    parse_init_p = TRUE;
    pl_create(ps);
    tab_collisions = get_all_collisions();
    tab_searches = get_all_searches();

    build_pl(ps);
    *root = make_parse(ps, ambiguous_p);

    tab_collisions = get_all_collisions() - tab_collisions;
    tab_searches = get_all_searches() - tab_searches;

#ifndef NO_YAEP_DEBUG_PRINT
    if (g->debug_level > 0)
    {
        fprintf(stderr, "%sGrammar: #terms = %d, #nonterms = %d, ",
                *ambiguous_p ? "AMBIGUOUS " : "",
                 ps->run.grammar->symbs_ptr->nn_terms, ps->run.grammar->symbs_ptr->n_nonterms);
        fprintf(stderr, "#rules = %d, rules size = %d\n",
                 ps->run.grammar->rules_ptr->n_rules,
                 ps->run.grammar->rules_ptr->n_rhs_lens + ps->run.grammar->rules_ptr->n_rules);
        fprintf(stderr, "Input: #tokens = %d, #unique situations = %d\n",
                 ps->toks_len, ps->n_all_sits);
        fprintf(stderr, "       #terminal sets = %d, their size = %d\n",
                 ps->run.grammar->term_sets_ptr->n_term_sets, ps->run.grammar->term_sets_ptr->n_term_sets_size);
        fprintf(stderr,
                 "       #unique set cores = %d, #their start situations = %d\n",
                 ps->n_set_cores, ps->n_set_core_start_sits);
        fprintf(stderr,
                 "       #parent indexes for some non start situations = %d\n",
                 ps->n_parent_indexes);
        fprintf(stderr,
                 "       #unique set dist. vects = %d, their length = %d\n",
                 ps->n_set_dists, ps->n_set_dists_len);
        fprintf(stderr,
                 "       #unique sets = %d, #their start situations = %d\n",
                 ps->n_sets, ps->n_sets_start_sits);
        fprintf(stderr,
                 "       #unique triples(set, term, lookahead) = %d, goto successes=%d\n",
                ps->n_set_term_lookaheads, ps->n_goto_successes);
        fprintf(stderr,
                 "       #pairs(set core, symb) = %d, their trans+reduce vects length = %d\n",
                 ps->n_core_symb_pairs, ps->n_core_symb_vect_len);
        fprintf(stderr,
                 "       #unique transition vectors = %d, their length = %d\n",
                ps->n_transition_vects, ps->n_transition_vect_len);
        fprintf(stderr,
                 "       #unique reduce vectors = %d, their length = %d\n",
                 ps->n_reduce_vects, ps->n_reduce_vect_len);
        fprintf(stderr,
                 "       #term nodes = %d, #abstract nodes = %d\n",
                 ps->n_parse_term_nodes, ps->n_parse_abstract_nodes);
        fprintf(stderr,
                 "       #alternative nodes = %d, #all nodes = %d\n",
                 ps->n_parse_alt_nodes,
                 ps->n_parse_term_nodes + ps->n_parse_abstract_nodes
                 + ps->n_parse_alt_nodes);
        if (tab_searches == 0)
            tab_searches++;
        fprintf(stderr,
                 "       #table collisions = %.2g%%(%d out of %d)\n",
                 tab_collisions* 100.0 / tab_searches,
                 tab_collisions, tab_searches);
    }
#endif
    yaep_parse_fin(ps);
    tok_fin(ps);
    return 0;
}

/* The following function frees memory allocated for the grammar.*/
void yaepFreeGrammar(YaepParseRun *pr, YaepGrammar *g)
{
    YaepParseState *ps = (YaepParseState*)pr;
    assert(CHECK_PARSE_STATE_MAGIC(ps));

    YaepAllocator *allocator;

    if (g != NULL)
    {
        allocator = g->alloc;
        pl_fin(ps);
        rule_fin(g, g->rules_ptr);
        term_set_fin(g, g->term_sets_ptr);
        symb_fin(ps, g->symbs_ptr);
        yaep_free(allocator, g);
        yaep_alloc_del(allocator);
    }

    TRACE_F;
}

static void free_tree_reduce(YaepTreeNode *node)
{
    YaepTreeNodeType type;
    YaepTreeNode **childp;
    size_t numChildren, pos, freePos;

    assert(node != NULL);
    assert((node->type & _yaep_VISITED) == 0);

    TRACE_FA("%p", node);

    type = node->type;
    node->type =(YaepTreeNodeType)(node->type | _yaep_VISITED);

    switch(type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
    case YAEP_TERM:
        break;

    case YAEP_ANODE:
        if (node->val.anode.name[0] == '\0')
	{
            /* We have already seen the node name*/
            node->val.anode.name = NULL;
	}
        else
	{
            /* Mark the node name as seen*/
            node->val._anode_name.name[0] = '\0';
	}
        for(numChildren = 0, childp = node->val.anode.children;
            *childp != NULL; ++numChildren, ++childp)
	{
            if ((*childp)->type & _yaep_VISITED)
	    {
               *childp = NULL;
	    }
            else
	    {
                free_tree_reduce(*childp);
	    }
	}
        /* Compactify children array*/
        for(freePos = 0, pos = 0; pos != numChildren; ++pos)
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
            free_tree_reduce(node->val.alt.node);
	}
        while((node->val.alt.next != NULL)
               &&(node->val.alt.next->type & _yaep_VISITED))
	{
            assert(node->val.alt.next->type ==(YAEP_ALT | _yaep_VISITED));
            node->val.alt.next = node->val.alt.next->val.alt.next;
	}
        if (node->val.alt.next != NULL)
	{
            assert((node->val.alt.next->type & _yaep_VISITED) == 0);
            free_tree_reduce(node->val.alt.next);
	}
        break;

    default:
        assert("This should not happen" == NULL);
    }
}

static void free_tree_sweep(YaepTreeNode *node,
                            void(*parse_free)(void*),
                            void(*termcb)(YaepTermNode*))
{
    YaepTreeNodeType type;
    YaepTreeNode *next;
    YaepTreeNode **childp;

    if (node == NULL)
    {
        return;
    }

    TRACE_FA("%p", node);

    assert(node->type & _yaep_VISITED);
    type =(YaepTreeNodeType)(node->type & ~_yaep_VISITED);

    switch(type)
    {
    case YAEP_NIL:
    case YAEP_ERROR:
        break;

    case YAEP_TERM:
        if (termcb != NULL)
	{
            termcb(&node->val.term);
	}
        break;

    case YAEP_ANODE:
        parse_free(node->val._anode_name.name);
        for(childp = node->val.anode.children;*childp != NULL; ++childp)
	{
            free_tree_sweep(*childp, parse_free, termcb);
	}
        break;

    case YAEP_ALT:
        free_tree_sweep(node->val.alt.node, parse_free, termcb);
        next = node->val.alt.next;
        parse_free(node);
        free_tree_sweep(next, parse_free, termcb);
        return;			/* Tail recursion*/

    default:
        assert("This should not happen" == NULL);
    }

    parse_free(node);
}

void yaepFreeTree(YaepTreeNode *root,
                  void(*parse_free)(void*),
                  void(*termcb)(YaepTermNode*))
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
       double free errors. Therefore, we walk the parse tree twice.
       On the first walk, we reduce the DAG to an actual tree.
       On the second walk, we recursively free the tree nodes. */
    free_tree_reduce(root);
    free_tree_sweep(root, parse_free, termcb);
    TRACE_F;
}

#ifndef NO_YAEP_DEBUG_PRINT

/* The following function prints symbol SYMB to file F.  Terminal is
   printed with its code if CODE_P.*/
static void symb_print(FILE* f, YaepSymb*symb, int code_p)
{
    fprintf(f, "%s", symb->repr);
    if (code_p && symb->term_p)
    {
        fprintf(f, "(%d)", symb->u.term.code);
    }
}

/* The following function prints RULE with its translation(if TRANS_P) to file F.*/
static void rule_print(YaepParseState *ps, FILE *f, YaepRule *rule, int trans_p)
{
    int i, j;

    assert(rule->mark >= 0 && rule->mark < 128);
    fprintf(f, "%c", rule->mark?rule->mark:' ');
    symb_print(f, rule->lhs, FALSE);
    fprintf(f, " :");
    for(i = 0; i < rule->rhs_len; i++)
    {
        assert(rule->marks[i] >= 0 && rule->marks[i] < 128);
        fprintf(f, " %c", rule->marks[i]?rule->marks[i]:' ');
        symb_print(f, rule->rhs[i], FALSE);
    }
    if (trans_p)
    {
        fprintf(f, " ---- ");
        if (rule->anode != NULL)
            fprintf(f, "%s(", rule->anode);
        for(i = 0; i < rule->trans_len; i++)
	{
            for(j = 0; j < rule->rhs_len; j++)
                if (rule->order[j] == i)
                {
                    fprintf(f, " %d:", j);
                    symb_print(f, rule->rhs[j], FALSE);
                    break;
                }
            if (j >= rule->rhs_len)
                fprintf(f, " nil");
	}
        if (rule->anode != NULL)
            fprintf(f, " )");
    }
    fprintf(f, "\n");
}

/* The following function prints RULE to file F with dot in position POS.*/
static void rule_dot_print(YaepParseState *ps, FILE *f, YaepRule *rule, int pos)
{
    int i;

    assert(pos >= 0 && pos <= rule->rhs_len);

    symb_print(f, rule->lhs, FALSE);
    fprintf(f, " :");
    for(i = 0; i < rule->rhs_len; i++)
    {
        fprintf(f, i == pos ? " ." : " ");
        symb_print(f, rule->rhs[i], FALSE);
    }
    if (rule->rhs_len == pos)
    {
        fprintf(f, ".");
    }
}

/* The following function prints situation SIT to file F.  The
   situation is printed with the lookahead set if LOOKAHEAD_P.*/
static void sit_print(YaepParseState *ps, FILE *f, YaepSituation *sit, int lookahead_p)
{
    fprintf(f, "%3d ", sit->sit_number);
    rule_dot_print(ps, f, sit->rule, sit->pos);
    if (ps->run.grammar->lookahead_level != 0 && lookahead_p)
    {
        fprintf(f, ",");
        term_set_print(ps, f, sit->lookahead, ps->run.grammar->symbs_ptr->nn_terms);
    }
}

/* The following function prints SET to file F.  If NONSTART_P is TRUE
   then print all situations.  The situations are printed with the
   lookahead set if LOOKAHEAD_P.  SET_DIST is used to print absolute
   distances of non-start situations. */
static void set_print(YaepParseState *ps, FILE* f, YaepSet*set, int set_dist, int nonstart_p, int lookahead_p)
{
    int i;
    int num, n_start_sits, n_sits, n_all_dists;
    YaepSituation**sits;
    int*dists,*parent_indexes;

    if (set == NULL && !ps->new_set_ready_p)
    {
        /* The following is necessary if we call the function from a
           debugger.  In this case new_set, new_core and their members
           may be not set up yet.*/
        num = -1;
        n_start_sits = n_sits = n_all_dists = ps->new_n_start_sits;
        sits = ps->new_sits;
        dists = ps->new_dists;
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
    fprintf(f, "  Set core = %d\n", num);
    for(i = 0; i < n_sits; i++)
    {
        fprintf(f, "    ");
        sit_print(ps, f, sits[i], lookahead_p);
        fprintf(f, ", %d\n",
                (i < n_start_sits
                  ? dists[i] : i < n_all_dists ? parent_indexes[i]
                  : 0));
        if (i == n_start_sits - 1)
        {
            if (!nonstart_p)
                break;
            fprintf(f, "    -----------\n");
        }
    }
}

#endif
/****************** YAEP parser single source file end **********************/
#endif // YAEP_MODULE
