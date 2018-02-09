/* Diagnostics <assert.h>

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include "_PDCLIB_aux.h"
#include "_PDCLIB_config.h"

/*
   Defines a macro assert() that, depending on the value of the preprocessor
   symbol NDEBUG, does
   * evaluate to a void expression if NDEBUG is set OR the parameter expression
     evaluates to true;
   * print an error message and terminates the program if NDEBUG is not set AND
     the parameter expression evaluates to false.
   The error message contains the parameter expression, name of the source file
  (__FILE__), line number (__LINE__), and (from C99 onward) name of the function
  (__func__).
    The header can be included MULTIPLE times, and redefines the macro depending
   on the current setting of NDEBUG.
*/

#ifndef _PDCLIB_ASSERT_H
#define _PDCLIB_ASSERT_H _PDCLIB_ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Functions _NOT_ tagged noreturn as this hampers debugging */
void _PDCLIB_assert99( char const * const, char const * const, char const * const ) _PDCLIB_nothrow;
void _PDCLIB_assert89( char const * const ) _PDCLIB_nothrow;

#ifdef __cplusplus
}
#endif

#if !defined( _PDCLIB_CXX_VERSION ) || _PDCLIB_CXX_VERSION < 2011
#if _PDCLIB_C_VERSION >= 2011
#define static_assert _Static_assert
#else
#define static_assert( e, m )
#endif
#endif

#endif

/* If NDEBUG is set, assert() is a null operation. */
#undef assert

#ifdef __divine__
#include <sys/divm.h>

#ifdef __cplusplus
extern "C" {
#endif
void __dios_fault( int f, const char *msg, ... ) _PDCLIB_nothrow __attribute__(( __noinline__ ));
#ifdef __cplusplus
}
#endif

#define assert(expression) \
    do { if(!(expression)) { \
        __dios_fault( _VM_F_Assert, "Assertion failed: " _PDCLIB_symbol2string(expression) \
                      ", file " __FILE__ \
                      ", line " _PDCLIB_symbol2string( __LINE__ ) \
                      "." ); \
      } \
    } while(0)

#elif defined NDEBUG
#define assert( ignore ) ( (void) 0 )
#elif _PDCLIB_C_MIN(99)
#define assert(expression) \
    do { if(!(expression)) { \
        _PDCLIB_assert99("Assertion failed: " _PDCLIB_symbol2string(expression)\
                         ", function ", __func__, \
                         ", file " __FILE__ \
                         ", line " _PDCLIB_symbol2string( __LINE__ ) \
                         "." _PDCLIB_endl ); \
        _PDCLIB_UNREACHABLE; \
      } \
    } while(0)

#else
#define assert(expression) \
    do { if(!(expression)) { \
        _PDCLIB_assert89("Assertion failed: " _PDCLIB_symbol2string(expression)\
                         ", file " __FILE__ \
                         ", line " _PDCLIB_symbol2string( __LINE__ ) \
                         "." _PDCLIB_endl ); \
        _PDCLIB_UNREACHABLE; \
      } \
    } while(0)
#endif