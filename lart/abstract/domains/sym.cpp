// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/sym.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

Type* formula_t( Module *m ) {
    return m->getFunction( "__abstract_sym_lift" )->getReturnType();
}

ConstantInt* bitwidth( Value *v ) {
    auto &ctx = v->getContext();
    auto bw = cast< IntegerType >( v->getType() )->getBitWidth();
    return ConstantInt::get( IntegerType::get( ctx, 32 ), bw );
}

std::string intr_name( CallInst *call ) {
    auto intr = call->getCalledFunction()->getName();
    size_t pref = std::string( "lart.sym." ).length();
    size_t suff = intr.drop_front( pref ).find_last_of( '.' );
    auto tag = intr.substr( pref, suff );
    return "__abstract_sym_" + tag.str();
}

Value* impl_lift( CallInst *call ) {
    IRBuilder<> irb( call );
    auto &ctx = call->getContext();
    auto op = call->getOperand( 0 );
    auto fn = get_module( call )->getFunction( "__abstract_sym_lift" );
    auto val = irb.CreateSExt( op, IntegerType::get( ctx, 64 ) );
    auto argc = ConstantInt::get( IntegerType::get( ctx, 32 ), 1 );
    return irb.CreateCall( fn, { bitwidth( op ), argc, val } );
}

Value* impl_assume( CallInst *call, Values &args ) {
    IRBuilder<> irb( call );
    auto fn = get_module( call )->getFunction( "__abstract_sym_assume" );
    return irb.CreateCall( fn, { args[ 0 ], args [ 0 ], args[ 1 ] } );
}

Value* impl_rep( CallInst *call ) {
    IRBuilder<> irb( call );
    auto val = call->getOperand( 0 );
    auto aty = formula_t( get_module( call ) );
    auto ev = irb.CreateExtractValue( val, 0 );
    return irb.CreateIntToPtr( ev, aty );
}

Value* impl_unrep( CallInst *call, Values &args ) {
    IRBuilder<> irb( call );
    auto ty = cast< StructType >( call->getType() )->getElementType( 0 );
    auto iv = irb.CreatePtrToInt( args[ 0 ], ty );
	auto cs = Constant::getNullValue( call->getType() );
    auto unrep = irb.CreateInsertValue( cs, iv, 0 );
    call->replaceAllUsesWith( unrep );
    return unrep;
}

Value* impl_op( CallInst *call, Values &args ) {
    IRBuilder<> irb( call );
    auto name = intr_name( call );
    auto fn = get_module( call )->getFunction( name );
    return irb.CreateCall( fn, args );
}

} // anonymous namespace

Value* Symbolic::process( Instruction *intr, Values &args ) {
    auto call = cast< CallInst >( intr );

    if ( is_lift( call ) )
        return impl_lift( call );
    if ( is_rep( call ) )
        return impl_rep( call );
    if ( is_unrep( call ) )
        return impl_unrep( call, args );
    if ( is_assume( call ) )
        return impl_assume( call, args );

    return impl_op( call, args );
}

} // namespace abstract
} // namespace lart
