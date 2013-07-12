#ifndef __DIVINE_USR_H
#define __DIVINE_USR_H

#ifndef DIVINE
#define DIVINE
#endif

/* ---- TEMPORARY ---- */
#define DEBUG
#define NO_MALLOC_FAILURE
#define NO_JMP
#define NEW_INTERP_BUGS
/* ------------------- */

#undef assert
#define assert( x ) __divine_assert( x )
#define ap( x ) __divine_ap( x )
#define LTL( name, x ) extern const char * const __divine_LTL_ ## name = #x

#ifdef TRACE
#define trace __divine_trace
#else
#define trace( x )
#endif

#ifdef DEBUG
#define DBG_ASSERT( x ) __divine_assert( x );
#else
#define DBG_ASSERT( x )
#endif

// Inlining have potential to break desired logic of interruption masking.
// Use NOINLINE for every function in which interruption is (un)masked.
#define NOINLINE __attribute__(( noinline ))

#if __has_attribute( warning )
#define WARNING( message ) __attribute__(( warning( message ) ))
#else
#if __has_attribute( deprecated )
 // silly workaround for unsupported warning attribute
#define WARNING( message ) __attribute__(( deprecated( message ) ))
#else
#define WARNING( message )
#endif
#endif

#if __has_attribute( error )
#define ERROR( message ) __attribute__(( error( message ) ))
#else
#if __has_attribute( unavailable )
 // workaround for unsupported error attribute
#define ERROR( message ) __attribute__(( unavailable( message ) ))
#else
#define ERROR( message )
#endif
#endif

#define UNSUPPORTED_USER     ERROR( "the function is not yet implemented in the user-space." )
#define UNSUPPORTED_SYSTEM   ERROR( "the function is currently unsupported by system-space." )
#define NO_EFFECT            WARNING( "the function is currently ignored during state space generation and" \
                                      " hence doesn't affect overall process of verification." )

#ifdef __cplusplus
#define NOTHROW throw()
extern "C" {
#else
#define NOTHROW
#endif

/* Prototypes for DiVinE-provided builtins. */
int __divine_new_thread( void (*)(void *), void* ) NOTHROW;
void __divine_interrupt_mask( void ) NOTHROW;
void __divine_interrupt_unmask( void ) NOTHROW;
void __divine_interrupt( void ) NOTHROW;
int __divine_get_tid( void ) NOTHROW;
int __divine_choice( int ) NOTHROW;
void __divine_assert( int ) NOTHROW; // + some informative string ?
void __divine_ap( int ) NOTHROW;
void * __divine_malloc( unsigned long ) NOTHROW;
void __divine_free( void * ) NOTHROW;

#ifdef TRACE
void __divine_trace( const char *, ... ) NOTHROW;
#endif

#ifdef __cplusplus
}
#endif

#undef NOTHROW

#endif
