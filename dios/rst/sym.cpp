#include <rst/sym.h>
#include <rst/domains.h>
#include <rst/common.h>
#include <dios.h>
#include <cstdarg>

using namespace lart::sym;
using abstract::Tristate;
using abstract::__new;
using abstract::mark;
using abstract::weaken;

template< typename T, typename ... Args >
static Formula *__newf( Args &&...args ) {
    void *ptr = __vm_obj_make( sizeof( T ) );
    new ( ptr ) T( std::forward< Args >( args )... );
    return static_cast< Formula * >( ptr );
}

struct State
{
    int counter = 0;
    Formula *constraints = nullptr;
};

State __sym_state;

extern "C" void __sym_formula_dump()
{
    Formula *pcf = __sym_state.constraints;
    while ( pcf != NULL )
    {
        __vm_trace( _VM_T_Text, lart::sym::toString( pcf->binary.left ).c_str() );
        pcf = pcf->binary.right;
    }
}

extern "C" uint64_t __vm_taint_i64();

template< typename T, typename = std::enable_if_t< sizeof( T ) == sizeof( T* ) > >
_SYM T __sym_val_impl() {
    auto val = reinterpret_cast< T >( __sym_lift( sizeof( T ), 0 ) );
    return val + __vm_taint_i64();
}

extern "C" {
// uint8_t __sym_val_i8() { return __sym_val_impl< uint8_t >(); }
// uint16_t __sym_val_i16() { return __sym_val_impl< uint16_t >(); }
// uint32_t __sym_val_i32() { return __sym_val_impl< uint32_t >(); }
uint64_t __sym_val_i64() { return __sym_val_impl< uint64_t >(); }
}

Formula *__sym_lift( int bitwidth, int argc, ... ) {
    if ( bitwidth > 64 )
        _UNREACHABLE_F( "Integer too long: %d bits", bitwidth );
    if ( !argc ) {
        return mark( __newf< Variable >( Type{ Type::Int, bitwidth }, __sym_state.counter++ ) );
    }
    if ( argc > 1 )
        _UNREACHABLE_F( "Lifting of more values is not yet supported." );

    va_list args;
    va_start( args, argc );
    return mark( __newf< Constant >( Type{ Type::Int, bitwidth }, va_arg( args, int64_t ) ) );
}

#define BINARY( suff, op ) Formula *__sym_ ## suff( Formula *a, Formula *b ) \
{                                                                                     \
    return mark( __newf< Binary >( Op::op, a->type(), weaken( a ), weaken( b ) ) );   \
}

BINARY( add, Add );
BINARY( sub, Sub );
BINARY( mul, Mul );
BINARY( sdiv, SDiv );
BINARY( udiv, UDiv );
BINARY( urem, URem );
BINARY( srem, SRem );
BINARY( and, And );
BINARY( or, Or );
BINARY( xor, Xor );
BINARY( shl, Shl );
BINARY( lshr, LShr );
BINARY( ashr, AShr );

#define CAST( suff, op ) Formula *__sym_ ## suff( Formula *a, int bitwidth ) \
{                                                                                     \
    Type t = a->type();                                                               \
    t.bitwidth( bitwidth );                                                           \
    return mark( __newf< Unary >( Op::op, t, weaken( a ) ) );                         \
}

CAST( trunc, Trunc );
CAST( zext, ZExt );
CAST( sext, SExt );
CAST( bitcast, BitCast );

#define ICMP( suff, op ) Formula *__sym_icmp_ ## suff( Formula *a, Formula *b ) { \
    Type i1( Type::Int, 1 ); \
    return mark( __newf< Binary >( Op::op, i1, weaken( a ), weaken( b ) ) ); \
}

ICMP( eq, EQ );
ICMP( ne, NE );
ICMP( ugt, UGT );
ICMP( uge, UGE );
ICMP( ule, ULE );
ICMP( ult, ULT );
ICMP( sgt, SGT );
ICMP( sge, SGE );
ICMP( sle, SLE );
ICMP( slt, SLT );

Tristate __sym_bool_to_tristate( Formula * )
{
    // TODO: pattern matching for trivial cases of True/False
    return Tristate::Value::Unknown;
}

Formula *__sym_assume( Formula *value, Formula *constraint, bool assume )
{
    Formula *wconstraint = weaken( constraint );
    if ( !assume )
        wconstraint = weaken( __newf< Unary >( Op::BoolNot, wconstraint->type(), wconstraint ) );
    __sym_state.constraints = mark( __newf< Binary >( Op::Constraint, wconstraint->type(), wconstraint,
                                                      weaken( __sym_state.constraints ) ) );
    __vm_trace( _VM_T_Assume, __sym_state.constraints );
    return value;
}
