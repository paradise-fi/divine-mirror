// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/ValueMap.h>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Analysis/CallGraph.h>

#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>

#include <lart/analysis/postorder.h>

#include <lart/abstract/types.h>
#include <lart/abstract/annotation.h>

#include <string>

namespace lart {
namespace abstract {

struct Abstraction : lart::Pass {

    using V = llvm::Value;
    using T = llvm::Type;
    using I = llvm::Instruction;
    using F = llvm::Function;
	using Dependencies = std::vector< V * >;

	virtual ~Abstraction() {}

	static PassMeta meta() {
    	return passMeta< Abstraction >(
        	"Abstraction", "Substitutes annotated values and by abstract values." );
    }

	llvm::PreservedAnalyses run( llvm::Module &m ) override {
        //init tristate
        auto & ctx = m.getContext();

        auto tristate = Tristate::get( ctx );

        type_store.insert( { llvm::IntegerType::getInt1Ty( ctx ), tristate } );
        abstract_types.insert( tristate );

        auto annotations = getAbstractAnnotations( m );

        lart::util::Store< std::map< llvm::Function *, std::vector< Annotation > > > functions;
        for ( const auto &a : annotations ) {
            auto parent = a.allocaInst->getParent()->getParent();
            if ( functions.contains( parent ) )
                functions[ parent ].push_back( a );
            else
                functions.insert( { parent, { a } } );
        }

        std::vector< llvm::Function * > unordered;
        for ( auto it : functions )
            unordered.push_back( it.first );
        auto order = analysis::callPostorder< F * >( m, unordered );

        for ( auto &current : order ) {
            preprocessFunction( current );
            for ( auto annot : functions[ current ] )
                processFunction( current, annot.allocaInst );
        }
        //TODO solve main

        for (auto & f : functionsToRemove ) {
            f->eraseFromParent();
        }

        return llvm::PreservedAnalyses::none();
	}

    void preprocessFunction( F * f ) {
        // change switches to branching
        auto lspass = llvm::createLowerSwitchPass();
        lspass->runOnFunction( *f );
        // unify exit nodes
        llvm::UnifyFunctionExitNodes ufen;
        ufen.runOnFunction( *f );
    }

    void processFunction( F * f, I * entry ) {
        toAnnotate.clear();

        storeType( entry->getType() );

        propagateValue( entry );

        bool changeReturn = query::query( *f ).flatten()
                            .map( query::refToPtr )
                            .map( query::llvmdyncast< llvm::ReturnInst > )
                            .filter( query::notnull )
                            .any( [&]( llvm::ReturnInst * ret ) {
                                return isAbstractType( ret->getReturnValue()->getType() );
                            } );

        T * rty = ( changeReturn )
                ? type_store[ f->getReturnType() ]
                : f->getReturnType();

        storeFunction( f, changeReturn, rty );

        for ( auto &a : toAnnotate ) {
            annotateAnonymous( a.first, a.second );
            a.first->eraseFromParent();
        }

        if ( changeReturn ) {
            std::vector< llvm::User * > users = { f->user_begin(), f->user_end() } ;
            for ( auto user : users ) {
                if( auto inst = llvm::dyn_cast< I >( user ) ) {
                    auto userFunction = inst->getParent()->getParent();
                    preprocessFunction( userFunction );
                    processFunction( userFunction, inst );
                }
            }
            functionsToRemove.insert( f );
        }
    }

    void propagateValue( V * v ) {
        std::set< I * > unusedLifts;
        auto deps = analysis::postorder< V * >( v );
        for ( auto dep : lart::util::reverse( deps ) )
            if ( auto inst = llvm::dyn_cast< I >( dep ) ) {
                process( inst );
                if ( value_store.contains( inst ) ) {
                    auto lifts = liftsOf( inst );
                    for ( auto & lift : lifts ) {
                        lift->replaceAllUsesWith( value_store[ inst ] );
                        unusedLifts.insert( lift );
                    }
                }
            }
        // obscure removing because of cyclic dependencies in phinodes
        for ( auto dep : deps )
        	if ( auto inst = llvm::dyn_cast< I >( dep ) ) {
                inst->removeFromParent();
                auto find = unusedLifts.find( inst );
                if ( find != unusedLifts.end() )
                    unusedLifts.erase( find );
            }
        for ( auto dep : deps )
            if ( auto inst = llvm::dyn_cast< I >( dep ) )
                inst->replaceAllUsesWith( llvm::UndefValue::get( inst->getType() ) );
        for ( auto dep : deps )
        	if ( auto inst = llvm::dyn_cast< I >( dep ) ) {
                auto val = value_store.find( inst );
                if ( val != value_store.end() )
                    value_store.erase( val );
                delete inst;
            }
        for ( auto & lift : unusedLifts )
            lift->eraseFromParent();
    }

    void process( I * inst ) {
        if ( ignore( inst ) ) return;

        llvmcase( inst,
     		[&]( llvm::AllocaInst * i ) {
                doAlloca( i );
            },
            [&]( llvm::LoadInst * i ) {
                doLoad( i );
            },
            [&]( llvm::StoreInst * i ) {
                doStore( i );
            },
            [&]( llvm::ICmpInst * i ) {
                doICmp( i );
            },
            [&]( llvm::SelectInst * i ) {
                doSelect( i );
            },
            [&]( llvm::BranchInst * i ) {
                doBranch( i );
   	        },
			[&]( llvm::BinaryOperator * i ) {
				doBinary( i );
			},
			[&]( llvm::CastInst * i ) {
                doCast( i );
            },
			[&]( llvm::PHINode * i ) {
                doPhi( i );
            },
			[&]( llvm::CallInst * i ) {
				doCall( i );
            },
 			[&]( llvm::ReturnInst * i ) {
                doReturn( i );
            },
			[&]( I *inst ) {
				std::cerr << "ERR: unknown instruction: ";
                inst->dump();
                std::exit( EXIT_FAILURE );
			} );
    }

    bool ignore( llvm::Instruction * inst ) {
        if ( auto call = llvm::dyn_cast< llvm::CallInst >( inst ) )
            return isLift( call );
        return false;
    }

    llvm::CallInst * createNamedCall( I * inst, T * rty, const std::string &tag,
                               std::vector< V * > args = {} )
    {
        auto acall = createCall( inst, rty, tag, args );
        value_store[ inst ] = acall;
        return acall;
    }

    llvm::Instruction * doAlloca( llvm::AllocaInst * i ) {
        assert( !i->isArrayAllocation() );
        auto rty = type_store[ i->getType() ];
        auto tag = "lart.abstract.alloca." + getTypeName( i->getAllocatedType() );
        return createNamedCall( i, rty, tag );
    }

    llvm::Instruction * doLoad( llvm::LoadInst * i ) {
        auto args = { value_store[ i->getOperand( 0 ) ] };
        auto rty = type_store[ i->getType() ];
        auto tag = "lart.abstract.load." + getTypeName( i->getType() );
        return createAnonymousCall( i, rty, tag, args);
    }

    llvm::Instruction * doStore( llvm::StoreInst * i ) {
        auto val = value_store.contains( i->getOperand( 0 ) )
                 ? value_store[ i->getOperand( 0 ) ]
                 : i->getOperand( 0 );
        auto type = val->getType();

        if ( llvm::isa< llvm::Argument >( val ) || llvm::isa< llvm::Constant >( val ) )
            if ( !isAbstractType( val->getType() ) )
                val = lift( val, i );
        auto ptr = i->getOperand( 1 );
        if ( !isAbstractType( ptr->getType() ) )
            ptr = value_store.contains( ptr ) ? value_store[ ptr ] : lift( ptr, i );
        auto args = { val, ptr };
        auto rty = llvm::Type::getVoidTy( i->getContext() );

        std::string name = getTypeName( type );
        if ( isAbstractType( type ) )
            for ( auto it = type_store.begin(); it != type_store.end(); ++it )
                if ( it->second == type )
                    name = getTypeName( it->first );
        auto tag = "lart.abstract.store." + name;
        return createAnonymousCall( i, rty, tag, args );
    }

    llvm::Instruction * doICmp( llvm::ICmpInst * i ) {
	    auto args = getBinaryArgs( i );
        auto argT = i->getOperand( 0 )->getType();
        std::string lowerType = isAbstractType( argT )
                              ? typeQualifier( argT )
                              : getTypeName( argT );
        auto tag = "lart.abstract.icmp."
                 + predicate.at( i->getPredicate() )
                 + "." + lowerType;
		return createAnonymousCall( i, Tristate::get( i->getContext() ), tag, args );
    }

    llvm::Instruction * doSelect( llvm::SelectInst * i ) {
        auto cond = value_store.contains( i->getCondition() )
                    ? lower( i, i->getCondition() )
                    : i->getCondition();

        auto tv = i->getTrueValue();
		auto fv = i->getFalseValue();

        if ( !isAbstractType( tv->getType() ) )
            tv = value_store.contains( tv ) ? value_store[ tv ] : lift( tv, i );
        if ( !isAbstractType( fv->getType() ) )
            fv = value_store.contains( fv ) ? value_store[ fv ] : lift( fv, i );

        llvm::IRBuilder<> irb( i );
        auto sub = irb.CreateSelect( cond, tv, fv );
        value_store.insert( { i, sub } );
        return llvm::cast< llvm::Instruction >( sub );
    }

    llvm::Instruction * doBranch( llvm::BranchInst * i ) {
        llvm::IRBuilder<> irb( i );
        if ( i->isUnconditional() ) {
            auto dest = i->getSuccessor( 0 );
            return irb.CreateBr( dest );
        } else {
            auto cond = i->getCondition();
            if ( isAbstractType( i->getCondition()->getType() )
                 || value_store.contains( i->getCondition() ) )
                cond = lower( i, i->getCondition() );
            auto tbb = i->getSuccessor( 0 );
            auto fbb = i->getSuccessor( 1 );
            return irb.CreateCondBr( cond, tbb, fbb );
        }
    }

    llvm::Instruction * doBinary( llvm::BinaryOperator * i ) {
        auto args = getBinaryArgs( i );
        auto tag = "lart.abstract."
                 + std::string( i->getOpcodeName() )
                 + "." + getTypeName( i->getType() );
        auto rty = type_store[ i->getType() ];
        return createAnonymousCall( i, rty, tag, args );
    }

    llvm::Instruction * doCast( llvm::CastInst * i ) {
        auto args = getUnaryArgs( i );
        auto tag = "lart.abstract." + std::string( i->getOpcodeName() ) + "."
                   + getTypeName( i->getSrcTy() ) + "." + getTypeName( i->getDestTy() );
        auto type = i->getDestTy();
        storeType( type );
        auto rty = type_store[ type ];
        return createAnonymousCall( i, rty, tag, args );
    }

    llvm::Instruction * doPhi( llvm::PHINode * n ) {
        auto at = type_store[ n->getType() ];

        unsigned int niv = n->getNumIncomingValues();
        llvm::IRBuilder<> irb( n );
        auto sub = irb.CreatePHI( at, niv );
        value_store.insert( { n , sub } );
        if ( n->getType() == at )
            n->replaceAllUsesWith( sub );

        for ( unsigned int i = 0; i < niv; ++i ) {
            auto val = llvm::cast< V >( n->getIncomingValue( i ) );
            auto parent = n->getIncomingBlock( i );
            if ( value_store.contains( val ) ) {
                sub->addIncoming( value_store[ val ], parent );
            } else {
                if ( isAbstractType( val->getType() ) )
                    sub->addIncoming( val, parent );
                else {
                    auto nbb =  parent->splitBasicBlock( parent->getTerminator() );
                    auto nval = lift( val, nbb->getTerminator() );
                    sub->addIncoming( nval, nbb );
                }
            }
        }
        return sub;
    }

    llvm::Instruction * doCall( llvm::CallInst * i ) {
        if ( isLift( i ) )
            return handleLiftCall( i );
        else if ( isLower( i ) )
            return handleLowerCall( i );
        else if ( i->getCalledFunction()->isIntrinsic() )
            return handleIntrinsicCall( llvm::cast< llvm::IntrinsicInst >( i ) );
        else
            return handleGenericCall( i );
    }

    llvm::Instruction * handleIntrinsicCall( llvm::IntrinsicInst * i ) {
        auto name = llvm::Intrinsic::getName( i->getIntrinsicID() );
        if ( ( name == "llvm.lifetime.start" ) || ( name == "llvm.lifetime.end" ) ) { /*skip */ }
        else if ( name == "llvm.var.annotation" ) { /* skip */ }
        else {
	        std::cerr << "ERR: unknown intrinsic: ";
            i->dump();
            std::exit( EXIT_FAILURE );
        }

        return i;
    }

    llvm::Instruction * handleLiftCall( llvm::CallInst * i ) {
        i->replaceAllUsesWith( value_store[ i->getArgOperand( 0 ) ] );
        return i;
    }

    llvm::Instruction * handleLowerCall( llvm::CallInst * i ) {
        auto clone = i->clone();
        clone->insertBefore( i );
        i->replaceAllUsesWith( clone );
        return clone;
    }

    llvm::Instruction * handleGenericCall( llvm::CallInst * i ) {
        std::vector < V * > args;
		std::vector < T * > arg_types;

		auto at = isAbstractType( i->getType() )
                ? i->getType()
                : type_store[ i->getType() ];

        for ( auto &arg : i->arg_operands() )
			if ( value_store.contains( arg ) ) {
            	auto type = isAbstractType( arg->getType() )
                     ? arg->getType()
                     : type_store[ arg->getType() ];
                arg_types.push_back( type );
                args.push_back( value_store[ arg ] );
            } else {
            	arg_types.push_back( arg->getType() );
                args.push_back( arg );
            }

        auto fn = i->getCalledFunction();

        if ( toAnnotate.contains( fn ) ) {
            //lart anonymous call
            auto rty = fn->getReturnType();
            auto tag = fn->getName();
            auto call = createNamedCall( i, rty, tag, args );
            auto find = toAnnotate.find( fn );
            i->replaceAllUsesWith( call );
            toAnnotate.insert( { call->getCalledFunction(), find->second } );
            return call;
        } else {
            llvm::ArrayRef< T * > params = arg_types;

            llvm::Function * stored = nullptr;
            if ( fn->getFunctionType()->params() == params && fn->getReturnType() == at )
                stored = fn;
            else
                stored = storedFunction( fn, params );

            if ( !stored && function_store.contains( fn ) ) {
                params = fn->getFunctionType()->params();
                fn = storedFunction( fn, params );
            }

            if ( fn->empty() ) {
                assert( fn->getName().startswith( "lart" ) );
                stored = fn;
            } else if ( !stored ) {
                auto fty = llvm::FunctionType::get( fn->getFunctionType()->getReturnType(),
                                                    arg_types,
                                                    fn->getFunctionType()->isVarArg() );
                stored = cloneFunction( fn, fty );
                preprocessFunction( stored );

                for ( auto &arg : stored->args() )
                    if ( arg.getType() == at )
                        value_store.insert( { &arg, &arg } );

                for ( auto &arg : stored->args() )
                    if ( arg.getType() == at )
                        propagateValue( &arg );

                //FIXME refactor with processFunction
                bool changeReturn = query::query( *stored ).flatten()
                            .map( query::refToPtr )
                            .map( query::llvmdyncast< llvm::ReturnInst > )
                            .filter( query::notnull )
                            .any( [&]( llvm::ReturnInst * ret ) {
                                return isAbstractType( ret->getReturnValue()->getType() );
                            } );

                if ( changeReturn && stored->getReturnType() != at ) {
                    functionsToRemove.insert( stored );
                    stored = changeReturnValue( stored, at );
                }

                storeFunction( fn, stored );
            }

            llvm::IRBuilder<> irb( i );
            auto abs = irb.CreateCall( stored, args );
            value_store.insert( { i, abs } );
            return abs;
        }
    }

    llvm::Instruction * doReturn( llvm::ReturnInst *i ) {
        llvm::IRBuilder<> irb( i );
        auto ret = value_store.contains( i->getReturnValue() )
                   ? value_store[ i->getReturnValue() ]
                   : i->getReturnValue();
        return irb.CreateRet( ret );
    }

    std::vector< V * > getBinaryArgs( I * i ) {
        std::vector< V * > args;
        auto a = i->getOperand( 0 );
		auto b = i->getOperand( 1 );

		if ( value_store.contains( a ) )
			args.push_back( value_store[ a ] );
		else
			args.push_back( a );

		if ( value_store.contains( b ) )
			args.push_back( value_store[ b ] );
		else
			args.push_back( b );

        return args;
    }

    std::vector< V * > getUnaryArgs( I * i ) {
        auto a = i->getOperand( 0 );
		auto val = value_store.contains( a ) ? value_store[ a ] : a ;
        return { val };
    }

    llvm::CallInst * lower( I * i, V * v ) {
        auto cond = value_store[ v ];
        auto tag = "lart.tristate.lower";
        auto ret = llvm::IntegerType::getInt1Ty( i->getContext() );
        return createCall( i, ret, tag, { cond } );
    }

    I * lift( V * v, I * to ) {
        auto at = type_store[ v->getType() ];

        llvm::IRBuilder<> irb( to );
        auto fty = llvm::FunctionType::get( at, { v->getType() }, false );
        auto tag = v->getType()->isIntegerTy( 1 )
                 ? "lart.tristate.lift"
                 : "lart.abstract.lift." + getTypeName( v->getType() );
        auto ncall = to->getModule()->getOrInsertFunction( tag, fty );
        auto sub = irb.CreateCall( ncall, v );
        //value_store.insert( { v, sub } ); //FIXME minimeze number of lifts

        return sub;
    }

    //Helper functions
    bool isAbstractType( T * t ) {
        if ( t->isPointerTy() )
            t = t->getPointerElementType();
        return abstract_types.find( t ) != abstract_types.end();
    }

    bool dependentReturn( Dependencies deps ) {
        return query::query( deps ).any( [&] ( V *v ) {
                    return llvm::dyn_cast< llvm::ReturnInst >( v );
                } );
    }

    llvm::CallInst * createCall( I * inst, T * rty, const std::string &tag,
                                 std::vector< V * > args = {} )
    {
		std::vector< T * > arg_types = {};
		for ( auto &arg : args )
			arg_types.push_back( arg->getType() );

		llvm::ArrayRef < T * > params = arg_types;

		auto fty = llvm::FunctionType::get( rty, params, false );
        auto call = inst->getModule()->getOrInsertFunction( tag, fty );
        llvm::IRBuilder<> irb( inst );
        return irb.CreateCall( call, args );
    }

    F * storeFunction( F * fn, bool changeReturn = false, T * rty = nullptr ) {
        F * newfn = changeReturn
                  ? changeReturnValue( fn, rty )
                  : fn;
        storeFunction( fn, newfn );
        return newfn;
    }

    void storeFunction( F * fn, F * tostore ) {
        if ( function_store.contains( fn ) )
            function_store[ fn ].push_back( tostore );
        else
            function_store[ fn ] = { tostore };
    }

    void storeType( llvm::CallInst * call ) {
        auto type = call->getCalledFunction()->getFunctionType()->getReturnType();
        storeType( type );
    }

    void storeType( T * t ) {
        T * ptr = t->isPointerTy() ? t : t->getPointerTo();
        T * type = t->isPointerTy() ? t->getPointerElementType() : t;

        if ( !type_store.contains( type ) ) {
            auto at = IntegerType::get( type );
            type_store.insert( { type, at } );
            type_store.insert( { ptr, at->getPointerTo() } );
            abstract_types.insert( at );
            abstract_types.insert( at->getPointerTo() );
        }
    }

    auto liftsOf( llvm::Instruction * inst ) ->  std::vector< llvm::CallInst * > {
        return query::query( inst->users() )
                     .map( query::llvmdyncast< llvm::CallInst > )
                     .filter( query::notnull )
                     .filter( [&]( llvm::CallInst * call ) {
                         return isLift( call );
                     } ).freeze();
    }

    bool isLift( llvm::CallInst * call ) {
        return call->getCalledFunction()->getName().startswith( "lart.abstract.lift" );
    }

    bool isLifted( llvm::Value * val ) {
        auto deps = analysis::postorder< llvm::Value * >( val );
        for ( auto & dep : deps )
            if ( auto call = llvm::dyn_cast< llvm::CallInst >( dep ) )
                if ( isLift( call ) ) return true;
        return false;
    }

    bool isLower( llvm::CallInst * call ) {
        return call->getCalledFunction()->getName().startswith( "lart.tristate.lower" );
    }

    bool isArgument( llvm::Value * v ) {
        return llvm::dyn_cast< llvm::Argument>( v ) != nullptr;
    }
    // Anonymous calls
    llvm::Instruction * createAnonymousCall( I * i, T * rty, const std::string &tag,
                           std::vector< V * > args = {} )
    {
        std::vector< T * > arg_types = {};
		for ( auto &arg : args )
			arg_types.push_back( arg->getType() );

        llvm::ArrayRef < T * > params = arg_types;

		auto fty = llvm::FunctionType::get( rty, params, false );
        using Linkage = llvm::GlobalValue::LinkageTypes;
        auto call = F::Create( fty, Linkage::ExternalLinkage, "", i->getModule() );

        llvm::IRBuilder<> irb( i );
        auto acall = irb.CreateCall( call, args );

        toAnnotate.insert( { acall->getCalledFunction(), tag } );
        value_store.insert( { i, acall } );
        return acall;
    }

    void annotateAnonymous( F * f, std::string name ) {
        auto rty = f->getReturnType();

        std::vector< std::pair< llvm::CallInst *, llvm::CallInst *> > toReplace;
        for ( auto user : f->users() ) {
            auto call = llvm::cast< llvm::CallInst >( user );

            std::vector < V * > args;
            for ( auto &arg : call->arg_operands() )
                if ( isAbstractType( arg->getType() ) )
                    args.push_back( arg );
                else {
                    auto lifted = lift( arg, call );
                    args.push_back( lifted );
                }

            std::vector< T * > arg_types;
		    for ( auto &arg : args )
			    arg_types.push_back( arg->getType() );

		    llvm::ArrayRef < T * > params = arg_types;

		    auto fty = llvm::FunctionType::get( rty, params, false );
            auto nf = call->getModule()->getOrInsertFunction( name, fty );
            auto ncall = llvm::CallInst::Create( nf, args );
            toReplace.push_back( { call, ncall } );
        }

        for ( auto &replace : toReplace )
            llvm::ReplaceInstWithInst( replace.first, replace.second );
    }

private:
	using Predicate = llvm::CmpInst::Predicate;
    const std::map< Predicate, std::string > predicate = {
    	{ Predicate::ICMP_EQ, "eq" },
    	{ Predicate::ICMP_NE, "ne" },
    	{ Predicate::ICMP_UGT, "ugt" },
    	{ Predicate::ICMP_UGE, "uge" },
    	{ Predicate::ICMP_ULT, "ult" },
    	{ Predicate::ICMP_ULE, "ule" },
    	{ Predicate::ICMP_SGT, "sgt" },
    	{ Predicate::ICMP_SGE, "sge" },
    	{ Predicate::ICMP_SLT, "slt" },
		{ Predicate::ICMP_SLE, "sle" }
    };

    template < typename V >
    using AbstractStore = lart::util::Store< std::map < V, V > >;

    template < typename K, typename V >
    using FunctionStore = lart::util::Store< std::map < K, std::vector< V > > >;

	F * storedFunction( F * fn, llvm::ArrayRef< T * > &params ) {
        if ( function_store.contains( fn ) )
        	for ( auto store_fn : function_store[ fn ] )
            	if ( store_fn->getFunctionType()->params() == params )
                	return store_fn;
 		return nullptr;
  	}

    // substituted instructions
    AbstractStore < V * > value_store;

    // substituted types
    AbstractStore< T * > type_store;
    std::set< T * > abstract_types;

	// substituted functions
    FunctionStore < F *, F * > function_store;

    // functions to annotate
    lart::util::Store< std::map < F *, std::string > > toAnnotate;

    // changed functions
    std::set< F * > functionsToRemove;
};

struct Substitution : lart::Pass {
    Substitution( std::string type ) {
        if ( type == "zero" )
            abstractionName = "__abstract_zero_";
        if ( type == "test" )
            abstractionName = "__abstract_test_";
    }

    virtual ~Substitution() {}

    static PassMeta meta() {
    	return passMetaC( "Substitution",
                "Substitutes abstract values by concrete abstraction.",
                []( llvm::ModulePassManager &mgr, std::string opt ) {
                    return mgr.addPass( Substitution( opt ) );
                } );
    }

	llvm::PreservedAnalyses run( llvm::Module &m ) override {
        // TODO generalize
        abstractionType = m.getFunction( abstractionName + "alloca" )
                          ->getReturnType()->getPointerElementType();

        processAbstractArguments( m );

        auto nameFilter = []( std::string name ) {
            return [&]( llvm::CallInst * call ) -> bool {
                auto fn = call->getCalledFunction();
                return fn != nullptr && fn->hasName() && fn->getName().startswith( name );
            };
        };

        auto allocaFilter = nameFilter( "lart.abstract.alloca" );
        auto allocas = identify< llvm::CallInst, decltype(allocaFilter) >( m, allocaFilter );

        auto liftFilter = nameFilter( "lart.abstract.lift" );
        auto lifts = identify< llvm::CallInst, decltype(liftFilter) >( m, liftFilter );

        std::vector< llvm::Value * > abstract;
        abstract.reserve( allocas.size() + lifts.size() );
        abstract.insert( abstract.end(), allocas.begin(), allocas.end() );
        abstract.insert( abstract.end(), lifts.begin(), lifts.end() );

        TToVStoreMap< llvm::Function *, llvm::Value * > funToValMap;
        for ( const auto &a : abstract ) {
            auto inst = llvm::dyn_cast< llvm::Instruction >( a );
            assert( inst != nullptr );

            abstractedTypes.insert( inst->getType() );

            llvm::Function * fn = inst->getParent()->getParent();
            if ( function_store.contains( fn ) )
                continue;
            if ( funToValMap.contains( fn ) )
                funToValMap[ fn ].push_back( a );
            else
                funToValMap.insert( { fn, { a } } );
        }

        for ( auto & fn : funToValMap ) {
            if ( fn.first->hasName() && fn.first->getName().startswith( "lart.abstract" ) )
                continue;
            //substitute all dependencies of abstract values
            for ( auto & value : fn.second )
                propagateAndProcess( m, value );
        }

        // FIXME can be done from start in postorder of all functions
        // in cleaner way
        auto retAbsVal = query::query( m )
                        .map( query::refToPtr )
                        .filter( []( llvm::Function * fn ) {
                            return isAbstractType( fn->getReturnType() )
                                && ! isAbstractDeclaration( fn );
                        } )
                        .filter( [&]( llvm::Function * fn ) {
                            return ! function_store.contains( fn )
                                 || funToValMap.contains( fn );
                        } ).freeze();

        auto order = analysis::callPostorder< llvm::Function * >( m, retAbsVal );

        for ( auto fn : order ) {
            processAbstractReturn( fn );
        }

        removeInstructions< decltype( abstractedValues ) >( abstractedValues );
        abstractedValues.clear();

        //remove abstracted functions
        std::vector< llvm::Function * > functionsToRemove;
        for ( auto it : function_store )
            functionsToRemove.push_back( it.first );
        for ( auto & fn : functionsToRemove ) {
            fn->replaceAllUsesWith( llvm::UndefValue::get( fn->getType() ) );
            fn->eraseFromParent();
        }

        removeAbstractDeclarations( m );
        return llvm::PreservedAnalyses::none();
    }

    void processAbstractReturn( llvm::Function * fn ){
        auto insts = query::query( abstractedValues )
                     .map( query::llvmdyncast< llvm::Instruction > )
                     .filter( query::notnull )
                     .filter( [&]( llvm::Instruction * i ) {
                         return i->getParent()->getParent() == fn;
                     } )
                     .freeze();
        for ( auto inst : insts )
            abstractedValues.erase( inst );
        removeInstructions< decltype( insts ) >( insts );

        if ( ! function_store.contains( fn ) ) {
            auto newfn = changeReturnValue( fn, abstractionType );
            function_store.insert( { fn, newfn } );
        }

        for ( auto user : fn->users() ) {
            auto call = llvm::cast< llvm::CallInst >( user );

            llvm::IRBuilder<> irb( call );
            auto newfn = function_store[ fn ];
            std::vector< llvm::Value * > args;
            for ( auto & arg : call->arg_operands() )
                args.push_back( arg );
            auto ncall = irb.CreateCall( newfn, args );
            store( call, ncall );
            for ( auto uuser : user->users() )
                propagateAndProcess( *fn->getParent(), uuser );
        }
    }

    void processAbstractArguments( llvm::Module & m ){
        auto args = query::query( m )
                    .map( []( llvm::Function & f ) {
                        return f.args();
                    } ).flatten()
                    .map( query::refToPtr )
                    .filter( [] ( llvm::Value * v ) {
                        return isAbstractType( v->getType() );
                    } )
                    .freeze();

        TToVStoreMap< llvm::Function *, llvm::Argument * > funToArgMap;

        for ( const auto &arg : args ) {
            llvm::Function * fn = arg->getParent();
            assert( fn != nullptr );
            if ( fn->hasName() && fn->getName().startswith( "lart." ) )
                continue;
            if ( funToArgMap.contains( fn ) )
                funToArgMap[ fn ].push_back( arg );
            else
                funToArgMap.insert( { fn, { arg } } );
        }

        std::vector< llvm::Function * > unordered;
        for ( auto it : funToArgMap )
            unordered.push_back( it.first );
        auto order = analysis::callPostorder< llvm::Function * >( m, unordered );

        for ( auto & fn : order ) {
            std::vector < llvm::Type * > arg_types;
            for ( auto &a : fn->args() ) {
                auto t = isAbstractType( a.getType() ) ? abstractionType : a.getType();
                arg_types.push_back( t );
            }
            auto rty = isAbstractType( fn->getFunctionType()->getReturnType() )
                     ? abstractionType
                     : fn->getFunctionType()->getReturnType();
            auto fty = llvm::FunctionType::get( rty,
                                                arg_types,
                                                fn->getFunctionType()->isVarArg() );
            auto newfn = cloneFunction( fn, fty );

            assert( !function_store.contains( fn ) );
            function_store.insert( { fn, newfn } );

            for ( auto & arg : funToArgMap[ fn ] ) {
                auto newarg = getArgument( newfn, arg->getArgNo() );
                abstraction_store.insert( { newarg, newarg } );
                propagateAndProcess( m, newarg );
            }
        }
    }

    void propagateAndProcess( llvm::Module & m, llvm::Value * value ) {
        auto deps = analysis::postorder< llvm::Value * >( value );
        for ( auto dep : lart::util::reverse( deps ) )
            if( auto inst = llvm::dyn_cast< llvm::Instruction >( dep ) )
                    process( m, inst );
    }


    void process( llvm::Module &m, llvm::Instruction * inst ) {
        llvmcase( inst,
            [&]( llvm::SelectInst * i ) {
                doSelect( i );
            },
            [&]( llvm::BranchInst * i ) {
                doBranch( i );
            },
			[&]( llvm::PHINode * i ) {
                doPhi( i );
            },
			[&]( llvm::CallInst * call ) {
                auto name =  call->getCalledFunction()->getName();
                if ( name.startswith( "lart.abstract.lift" ) ||
                     name.startswith( "lart.tristate.lift" ) )
                    handleLiftCall( m, call );
                else if ( name.startswith( "lart" ) )
                    handleAbstractCall( m, call );
                else
                    handleGenericCall( m, call );
            },
 			[&]( llvm::ReturnInst * i ) {
                doReturn( i );
            },
			[&]( llvm::Instruction *i ) {
				std::cerr << "ERR: unknown instruction: ";
                i->dump();
                std::exit( EXIT_FAILURE );
			} );
    }

    void handleLiftCall( llvm::Module & m, llvm::CallInst * call ) {
        auto name = getAbstractFunctionName( call->getCalledFunction()->getName() );
        auto fn = m.getFunction( name );

        llvm::IRBuilder<> irb( call );
        auto ncall = irb.CreateCall( fn, { call->getArgOperand( 0 ) } );
        store( call, ncall );

        abstractedTypes.insert( ncall->getType() );
    }

    void handleAbstractCall( llvm::Module & m, llvm::CallInst * call ) {
        auto name = getAbstractFunctionName( call->getCalledFunction()->getName() );
        auto fn = getFunctionWithName( name, m );
        processFunctionCall( fn, call );
    }

    void handleGenericCall( llvm::Module & m, llvm::CallInst * call ) {
        auto name = call->getCalledFunction()->getName();
        auto fn = getFunctionWithName( name, m );
        processFunctionCall( fn, call );
    }

    llvm::Function * getFunctionWithName( llvm::StringRef name, llvm::Module & m ) {
        auto fn = m.getFunction( name );
        if ( function_store.contains( fn ) )
            fn = function_store[ fn ];
        assert ( fn != nullptr );
        return fn;
    }

    void processFunctionCall( llvm::Function * fn, llvm::CallInst * call ) {
        std::vector < llvm::Value * > args;

        for ( auto &arg : call->arg_operands() ) {
            if ( isAbstractType( arg->getType() )
              && abstraction_store.find( arg ) == abstraction_store.end() ) {
                //not all incoming values substituted
                //wait till have all args
                break;
            }
            auto tmp = isAbstractType( arg->getType() )
                     ? abstraction_store[ arg ]
                     : arg;
            args.push_back( tmp );
        }

        //skip if do not have enough substituted arguments
        if ( call->getNumArgOperands() == args.size() ) {
            llvm::IRBuilder<> irb( call );
            auto ncall = irb.CreateCall( fn, args );
            store( call, ncall );
        }
    }

    void doSelect( llvm::SelectInst * i ) {
        auto cond = i->getCondition();
        if ( abstraction_store.contains( cond ) ) {
            cond = abstraction_store[ cond ];
            auto tv = abstraction_store.contains( i->getTrueValue() )
                    ? abstraction_store[ i->getTrueValue() ]
                    : nullptr;
            auto fv = abstraction_store.contains( i->getFalseValue() )
                    ? abstraction_store[ i->getFalseValue() ]
                    : nullptr;
            if ( tv == nullptr || fv == nullptr )
                return;
            llvm::IRBuilder<> irb( i );
            auto newsel = irb.CreateSelect( cond, tv, fv );
            store( i, newsel );
        }
    }

    void doBranch( llvm::BranchInst * i ) {
        assert( i->isConditional() );
        if ( abstraction_store.contains( i ) )
            return;
        llvm::IRBuilder<> irb( i );
        auto cond = i->getCondition();
        if ( abstraction_store.contains( cond ) ) {
            cond = abstraction_store[ cond ];
            auto tbb = i->getSuccessor( 0 );
            auto fbb = i->getSuccessor( 1 );
            auto newbr = irb.CreateCondBr( cond, tbb, fbb );
            store( i, newbr );
        }
    }

    void doPhi( llvm::PHINode * phi ) {
        unsigned int niv = phi->getNumIncomingValues();

        std::vector< std::pair< llvm::Value *, llvm::BasicBlock * > > incoming;
        for ( unsigned int i = 0; i < niv; ++i ) {
            auto val = llvm::cast< llvm::Value >( phi->getIncomingValue( i ) );
            auto parent = phi->getIncomingBlock( i );
            if ( abstraction_store.contains( val ) )
                incoming.push_back( { abstraction_store[ val ], parent } );
            else {
                if ( isAbstractedType( val->getType() ) )
                    incoming.push_back( { val, parent } );
            }
        }

        if ( incoming.size() > 0 ) {
            llvm::PHINode * node = nullptr;
            if ( abstraction_store.contains( phi ) )
                node = llvm::cast< llvm::PHINode >( abstraction_store[ phi ] );
            else {
                llvm::IRBuilder<> irb( phi );
                node = irb.CreatePHI( abstractionType, niv );
                store( phi, node );
            }

            for ( size_t i = 0; i < node->getNumIncomingValues(); ++i )
                node->removeIncomingValue( i,  false );
            for ( auto & in : incoming )
                node->addIncoming( in.first, in.second );
        }
    }

    void doReturn( llvm::ReturnInst * i ) {
        if ( abstraction_store.contains( i->getReturnValue() ) ) {
            llvm::IRBuilder<> irb( i );
            auto arg = abstraction_store[ i->getReturnValue() ];
            auto ret = irb.CreateRet( arg );
            store( i, ret );
        }
    }

    template< typename Container >
    void removeInstructions( Container & insts ) {
        for ( auto &inst : insts )
            inst->replaceAllUsesWith( llvm::UndefValue::get( inst->getType() ) );
        for ( auto &inst : insts ) {
            auto stored = abstraction_store.find( inst );
            if ( stored != abstraction_store.end() )
                abstraction_store.erase( stored );
            inst->eraseFromParent();
        }
    }

    void removeAbstractDeclarations( llvm::Module & m ) {
        auto toErase = query::query( m )
                        .map( query::refToPtr )
                        .filter( []( llvm::Function * fn ) {
                            return isAbstractDeclaration( fn );
                        } ).freeze();
        for ( auto &fn : toErase )
            fn->eraseFromParent();
    }

    void store( llvm::Value * val, llvm::Value * newval ) {
        abstraction_store[ val ] = newval;
        abstractedValues.insert( llvm::cast< llvm::Instruction >( val ) );
    }

    static bool isAbstractDeclaration( llvm::Function * fn ) {
        return fn->getName().startswith( "lart.abstract" )
            || fn->getName().startswith( "lart.tristate" );
    }

    //helpers
    llvm::Argument * getArgument( llvm:: Function * fn, size_t index ) {
        auto it = fn->arg_begin();
        for ( unsigned i = 0; i < index; ++i )
            ++it;
        return &(*it);
    }

    std::string getAbstractFunctionName( llvm::StringRef callName, bool pointer = false ) {
        llvm::SmallVector< llvm::StringRef, 4 > nameParts;
        callName.split( nameParts, "." );
        auto suffix = callName.endswith( "*" ) ? "_p" : "";
        if ( nameParts[ 1 ].startswith( "tristate" ) )
            return "__abstract_tristate_" + nameParts[ 2 ].str() + suffix;
        if ( nameParts[ 2 ].startswith( "icmp" ) )
            return abstractionName + "icmp_" + nameParts[ 3 ].str() + suffix;
        if ( nameParts[ 2 ].startswith( "lift" ) )
            return abstractionName + nameParts[ 2 ].str() + suffix + "_" + nameParts[ 3 ].str();
        if ( nameParts[ 2 ].startswith( "lower" ) )
            return abstractionName + nameParts[ 2 ].str() + suffix + "_" + nameParts[ 3 ].str();
        else
            return abstractionName + nameParts[ 2 ].str() + suffix;
    }

    template < typename Value, typename Filter >
    auto identify( llvm::Module &m, Filter filter ) -> std::vector< Value * > {
         return query::query( m ).flatten().flatten()
                      .map( query::refToPtr )
                      .map( query::llvmdyncast< Value > )
                      .filter( query::notnull )
                      .filter( filter )
                      .freeze();
    }

    bool isAbstractedType( llvm::Type * type ) {
        return abstractedTypes.find( type ) != abstractedTypes.end();
    }

    bool isAbstractType( llvm::Type * t ) {
        auto typeName = lart::abstract::getTypeName( t );
        std::string prefix = "%lart";
        return !typeName.compare( 0, prefix.size(), prefix );
    }

private:
    template < typename T, typename V >
    using TToVStoreMap = lart::util::Store< std::map< T, std::vector< V  > > >;

    template < typename V >
    using AbstractStore = lart::util::Store< std::map< V, V > >;

    // substituted values
    AbstractStore< llvm::Value * > abstraction_store;

	// substituted functions
    AbstractStore < llvm::Function * > function_store;

    std::set< llvm::Instruction * > abstractedValues;

    std::set < llvm::Type * > abstractedTypes;

    std::string abstractionName;
    llvm::Type * abstractionType;
};

PassMeta abstraction_pass() {
    return Abstraction::meta();
}

PassMeta substitution_pass() {
    return Substitution::meta();
}

PassMeta full_abstraction_pass() {
    return passMetaC< Substitution >( "abstraction", "",
        []( llvm::ModulePassManager &mgr, std::string opt ) {
            Abstraction::meta().create( mgr, "" );
            Substitution::meta().create( mgr, opt );
        } );
};

} /* lart */
} /* abstract */

