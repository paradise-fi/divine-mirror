// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/taint.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Argument.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/DerivedTypes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/metadata.h>
#include <lart/abstract/intrinsics.h>

#include <lart/support/util.h>

#include <algorithm>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

std::string taint_suffix( const Types &args ) {
    std::string res;
    for ( auto a : args )
        res += "." + llvm_name( a );
    return res;
}

void use_tainted_value( Instruction *i, Instruction *orig, Instruction *tainted ) {
    auto op = i->getOperandList();
    while ( op ) {
        if ( op->get() == orig ) {
            op->set( tainted );
            return;
        }
        op = op->getNext();
    }
    UNREACHABLE( "Instruction does not use tainted value." );
}

Function* intrinsic( Instruction *i ) {
    auto d = MDValue( i ).domain();
    return get_intrinsic( i, d );
}

Instruction* branch_intrinsic( Instruction *i, std::string name ) {
    auto i8 = Type::getInt8Ty( i->getContext() );
    assert( i->getType() == i8 );
    auto fn = get_intrinsic( getModule( i ), name, i8, { i8 } );
    return create_taint( i, { fn, i, i } );
}

Instruction* to_tristate( Instruction *i, Domain dom ) {
    auto &ctx = i->getContext();
    MDBuilder mdb( ctx );
    auto trs_dom = mdb.domain_node( Domain::Tristate );
    auto b2t = branch_intrinsic( i, "lart.gen." + DomainTable[ dom ] + ".bool_to_tristate" );
    b2t->setMetadata( "lart.domains", MDTuple::get( ctx, trs_dom ) );
    return b2t;
}

Instruction* lower_tristate( Instruction *i ) {
    auto lt = branch_intrinsic( i, "lart.gen.tristate.lower" );
    lt->setMetadata( "lart.domains", nullptr );
    return lt;
}

Type* abstract_type( Type *t, Domain dom ) {
    std::string name;
    if ( dom == Domain::Tristate )
        name = "lart." + DomainTable[ dom ];
    else
		name = "lart." + DomainTable[ dom ] + "." + llvm_name( t );

    if ( auto aty = t->getContext().pImpl->NamedStructTypes.lookup( name ) )
        return aty;
    return StructType::create( { t }, name );

}

std::string lift_name( Type *t, Domain dom ) {
    if ( dom == Domain::Tristate )
        return "lart." + DomainTable[ dom ] + ".lift" ;
    else
		return "lart." + DomainTable[ dom ] + ".lift." + llvm_name( t );
}

Function* lift( Value *val, Domain dom ) {
    auto m = getModule( val );
    auto ty = val->getType();
    auto aty = abstract_type( ty, dom );
    auto name = lift_name( ty, dom );
    auto fty = FunctionType::get( aty, { ty }, false );
    return cast< Function >( m->getOrInsertFunction( name, fty ) );
}

Function* rep( Value *val, Domain dom ) {
    auto m = getModule( val );
	auto ty = val->getType();
	auto aty = abstract_type( ty, dom );
	auto fty = FunctionType::get( aty, { aty }, false );
	auto name = "lart." + DomainTable[ dom ] + ".rep." + llvm_name( ty );
    return cast< Function >( m->getOrInsertFunction( name, fty ) );
}

Function* unrep( Value *val, Domain dom, Type *to ) {
    auto m = getModule( val );
	auto ty = val->getType();
	auto fty = FunctionType::get( ty, { ty }, false );
	auto name = "lart." + DomainTable[ dom ] + ".unrep." + llvm_name( to );
    return cast< Function >( m->getOrInsertFunction( name, fty ) );
}

BasicBlock* make_bb( Function *fn, std::string name ) {
	auto &ctx = fn->getContext();
	return BasicBlock::Create( ctx, name, fn );
}

BasicBlock* entry_bb( Value *tflag, size_t idx ) {
	auto &ctx = tflag->getContext();

    std::string name = "arg." + std::to_string( idx ) + ".entry";
	auto bb = make_bb( getFunction( tflag ), name );

    IRBuilder<> irb( bb );
    irb.CreateICmpEQ( tflag, ConstantInt::getTrue( ctx ) );
	return bb;
}

BasicBlock* tainted_bb( Value *arg, size_t idx, Domain dom ) {
    std::string name = "arg." + std::to_string( idx ) + ".tainted";
	auto bb = make_bb( getFunction( arg ), name );

    auto aty = abstract_type( arg->getType(), dom );
	auto cs = Constant::getNullValue( aty );

	IRBuilder<> irb( bb );
	auto iv = irb.CreateInsertValue( cs, arg, { 0 } );
	irb.CreateCall( rep( arg, dom ), { iv } );
	return bb;
}

BasicBlock* untainted_bb( Value *arg, size_t idx, Domain dom ) {
    std::string name = "arg." + std::to_string( idx ) + ".untainted";
	auto bb = make_bb( getFunction( arg ), name );
    IRBuilder<>( bb ).CreateCall( lift( arg, dom ), { arg } );
	return bb;
}

BasicBlock* merge_bb( Value *arg, size_t idx ) {
    std::string name = "arg." + std::to_string( idx ) + ".merge";
	auto bb = make_bb( getFunction( arg ), name );
	return bb;
}

void join_bbs( BasicBlock *ebb, BasicBlock *tbb, BasicBlock *ubb,
			   BasicBlock *mbb, BasicBlock *exbb )
{
	IRBuilder<> irb( ebb );
    irb.CreateCondBr( &ebb->back(), tbb, ubb );

	irb.SetInsertPoint( mbb );
	auto tv = &tbb->back();
	auto uv = &ubb->back();
	assert( tv->getType() == uv->getType() );
	auto phi = irb.CreatePHI( tv->getType(), 2 );
	phi->addIncoming( tv, tbb );
	phi->addIncoming( uv, ubb );
    exbb->moveAfter( mbb );
	irb.CreateBr( exbb );

    irb.SetInsertPoint( tbb );
    irb.CreateBr( mbb );

	irb.SetInsertPoint( ubb );
    irb.CreateBr( mbb );
}

Function* make_abstract_op( CallInst *taint, Types args ) {
    auto fn = cast< Function >( taint->getOperand( 0 ) );
    auto m = getModule( taint );

	std::string prefix = "lart.gen.";
	auto name = "lart." + fn->getName().drop_front( prefix.size() );

	auto rty = ( taint->getMetadata( "lart.domains" ) )
		     ? abstract_type( taint->getType(), MDValue( taint ).domain() )
		     : taint->getType();

    auto fty = FunctionType::get( rty, args, false );
	return cast< Function >( m->getOrInsertFunction( name.str(), fty ) );
}

void exit_lifter( BasicBlock *exbb, CallInst *taint, Values &args ) {
    auto fn = cast< Function >( taint->getOperand( 0 ) );

    Domain dom;
    if ( taint->getMetadata( "lart.domains" ) )
        dom = MDValue( taint ).domain();
    else
        dom = MDValue( taint->getOperand( 1 ) ).domain();

    IRBuilder<> irb( exbb );
	auto aop = make_abstract_op( taint, types_of( args ) );
	auto call = irb.CreateCall( aop, args );
	if ( call->getType() != fn->getReturnType() ) {
        auto to = fn->getReturnType();
		auto ur = create_call( irb, unrep( call, dom, to ), { call }, dom );
        auto ev = irb.CreateExtractValue( ur, 0 );
		irb.CreateRet( ev );
	} else {
		irb.CreateRet( call );
	}
}

} // anonymous namespace

Function* get_taint_fn( Module *m, Type *ret, Types args ) {
    auto taint_fn = args.front();
    args.erase( args.begin() );
    auto name = "__vm_test_taint" + taint_suffix( args );
    args.insert( args.begin(), taint_fn );

    auto fty = FunctionType::get( ret, args, false );
    auto fn = m->getOrInsertFunction( name, fty );
    return cast< Function >( fn );
}

Instruction* create_taint( Instruction *i, const Values &args ) {
    IRBuilder<> irb( i );

    auto rty = i->getType();

    auto fn = get_taint_fn( getModule( i ), rty, types_of( args ) );

    auto call = irb.CreateCall( fn, args );
    call->removeFromParent();
    call->insertAfter( i );
    call->setMetadata( "lart.domains", i->getMetadata( "lart.domains" ) );
    return call;
}

bool is_taintable( Value *i ) {
    return is_one_of< BinaryOperator, CmpInst, TruncInst, SExtInst, ZExtInst >( i );
}

void Tainting::run( Module &m ) {
    for ( const auto & mdv : abstract_metadata( m ) )
    	taint( cast< Instruction >( mdv.value() ) );
}

void Tainting::taint( Instruction *i ) {
    if ( !is_taintable( i ) )
        return;

    Values args;
    args.push_back( intrinsic( i ) );
    args.push_back( i ); // fallback value
    for ( auto & op : i->operands() )
        args.emplace_back( op.get() );

    auto call = create_taint( i, args );

    for ( const auto & u : i->users() )
        if ( u != call )
            use_tainted_value( cast< Instruction >( u ), i, call );

    tainted.insert( i );
}

void TaintBranching::run( Module &m ) {
    for ( auto t : taints( m ) )
        for ( auto u : t->users() )
            if ( auto br = dyn_cast< BranchInst >( u ) )
                expand( t, br );
}

void TaintBranching::expand( Value *t, BranchInst *br ) {
    IRBuilder<> irb( br );
    auto &ctx = br->getContext();

    auto orig = cast< User >( t )->getOperand( 1 ); // fallback value
    auto dom = MDValue( orig ).domain();

    auto ti = cast< Instruction >( t );
    auto i8 = cast< Instruction >( irb.CreateZExt( ti, Type::getInt8Ty( ctx ) ) );
    i8->setMetadata( "lart.domains", ti->getMetadata( "lart.domains" ) );

    auto trs = to_tristate( cast< Instruction >( i8 ), dom );
    auto low = lower_tristate( trs );
    auto i1 = irb.CreateTrunc( low, Type::getInt1Ty( ctx ) );

    br->setCondition( i1 );
}

void LifterSyntetize::run( Module &m ) {
    for ( auto t : taints( m ) )
        process( cast< CallInst >( t ) );
}

void LifterSyntetize::process( CallInst *taint ) {
    auto fn = cast< Function >( taint->getOperand( 0 ) );
    if ( fn->empty() ) {
		using LifterArg = std::pair< Value*, Value* >; // tainted flag + value

		std::vector< LifterArg > args;
		for ( auto it = fn->arg_begin(); it != fn->arg_end(); std::advance( it, 2 ) )
			args.emplace_back( it, std::next( it ) );

		auto dom = MDValue( taint->getOperand( 1 ) ).domain();

		auto exbb = make_bb( fn, "exit" );

		size_t idx = 0;
		BasicBlock *prev = nullptr;

		Values lifted;
		for ( const auto &arg : args ) {
			auto ebb = entry_bb( arg.first, idx );
			auto tbb = tainted_bb( arg.second, idx, dom );
			auto ubb = untainted_bb( arg.second, idx, dom );
			auto mbb = merge_bb( arg.second, idx );

			join_bbs( ebb, tbb, ubb, mbb, exbb );
			lifted.push_back( mbb->begin() );

			if ( prev ) {
				prev->getTerminator()->setSuccessor( 0, ebb );
			}

			prev = mbb;
			idx++;
		}

		exit_lifter( exbb, taint, lifted );
    }
}

} // namespace abstract
} // namespace lart