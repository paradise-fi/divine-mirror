// -*- C++ -*- (c) 2016-2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/dfa.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
#include <llvm/Transforms/Utils.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/lowerselect.h>
#include <lart/support/util.h>
#include <lart/abstract/domain.h>
#include <lart/abstract/util.h>

#include <brick-llvm>

namespace lart::abstract {

    using MapValue = DataFlowAnalysis::MapValuePtr;

    struct AddAbstractMetaVisitor : llvm::InstVisitor< AddAbstractMetaVisitor >
    {
        AddAbstractMetaVisitor( const MapValue &mval )
            : _mval( mval )
        {}

        static constexpr char op_prefix[] = "lart.abstract.op_";

        void add_meta( llvm::Instruction *val, const std::string& operation )
        {
            // TODO set correct domain kind?
            meta::abstract::set( val, to_string( DomainKind::scalar ) );

            auto m = val->getModule();
            if ( auto op = m->getFunction( operation ) ) {
                auto index = op->getMetadata( meta::tag::operation::index );

                val->setMetadata( meta::tag::operation::index, index );
            }
        }

        void visitStoreInst( llvm::StoreInst &store )
        {
            // TODO if is freeze
            // if ( store.getValueOperand()->getType()->isIntegerTy() ) // TODO remove
            meta::set( &store, meta::tag::operation::freeze );
            // TODO if is store
        }

        void visitLoadInst( llvm::LoadInst &load )
        {
            // TODO if is thaw
            // if ( load.getType()->isIntegerTy() ) // TODO remove
            meta::set( &load, meta::tag::operation::thaw );
            auto op = std::string( op_prefix ) + "thaw";
            add_meta( &load, op );
            // TODO if is load
        }

        void visitCmpInst( llvm::CmpInst &cmp )
        {
            auto op = std::string( op_prefix )
                    + PredicateTable.at( cmp.getPredicate() );
            add_meta( &cmp, op );
        }

        void visitBitCastInst( llvm::BitCastInst & )
        {
            // skip
        }

        void visitCastInst( llvm::CastInst &cast )
        {
            auto op = std::string( op_prefix )
                    + std::string( cast.getOpcodeName() );
            add_meta( &cast, op );
        }

        void visitBinaryOperator( llvm::BinaryOperator &bin )
        {
            auto op = std::string( op_prefix )
                    + std::string( bin.getOpcodeName() );
            add_meta( &bin, op );
        }

        void visitReturnInst( llvm::ReturnInst &ret )
        {
            meta::set( &ret, meta::tag::abstract_return );
        }

        void visitCallInst( llvm::CallInst &call )
        {
            if ( meta::abstract::has( &call ) )
                return;

            auto fn = call.getCalledFunction();
            ASSERT( fn->hasName() );
            auto op = std::string( op_prefix ) + fn->getName().str();
            add_meta( &call, op );
        }

        const MapValue &_mval;
    };

    bool is_call( llvm::Value * val ) noexcept
    {
        return util::is_one_of< llvm::CallInst, llvm::InvokeInst >( val );
    }

    bool is_propagable( llvm::Value * val ) noexcept
    {
        // TODO refactor
        if ( llvm::isa< llvm::Argument >( val ) )
            return true;
        if ( llvm::isa< llvm::ReturnInst >( val ) )
            return true;
        if ( llvm::isa< llvm::GetElementPtrInst >( val ) )
            return true;
        if ( llvm::isa< llvm::CmpInst >( val ) )
            return true;
        if ( llvm::isa< llvm::LoadInst >( val ) )
            return true;
        if ( llvm::isa< llvm::AllocaInst >( val ) )
            return true;
        if ( is_call( val ) )
            return true;
        if ( auto i = llvm::dyn_cast< llvm::Instruction >( val ) )
            return i->isBinaryOp() || i->isCast() || i->isShift();
        UNREACHABLE( "unkonwn value" );
    }

    bool DataFlowAnalysis::join( llvm::Value * lhs, llvm::Value * rhs ) noexcept
    {
        return join( lhs, interval( rhs ) );
    }

    bool DataFlowAnalysis::join( llvm::Value * lhs, const MapValuePtr &val ) noexcept
    {
        return interval( lhs )->join( val );
    }

    bool DataFlowAnalysis::visited( llvm::Value * val ) const noexcept
    {
        return _intervals.has( val );
    }

    void DataFlowAnalysis::propagate( llvm::Value * to, const MapValuePtr& from ) noexcept
    {
        auto task = [=] { propagate( to ); };
        if ( !visited( to ) ) {
            _intervals[ to ] = from;
            push( task );
        } else if ( join( to, from ) ) {
            // if join introduces any new information
            push( task );
        }
    }

    void DataFlowAnalysis::propagate_wrap( llvm::Value * lhs, llvm::Value * rhs ) noexcept
    {
        // forward propagation
        if ( visited( rhs ) ) {
            ASSERT( visited( rhs ) );
            propagate( lhs, _intervals[ rhs ]->peel() );
        }
        // backward propagation
        if ( visited( lhs ) ) {
            ASSERT( visited( lhs ) );
            propagate( rhs, _intervals[ lhs ]->cover() );
        }

    }

    void DataFlowAnalysis::propagate_identity( llvm::Value * lhs, llvm::Value * rhs ) noexcept
    {
        // forward propagation
        if ( visited( rhs ) ) {
            ASSERT( visited( rhs ) );
            propagate( lhs, _intervals[ rhs ] );
        }
        // backward propagation
        if ( visited( lhs ) ) {
            ASSERT( visited( lhs ) );
            propagate( rhs, _intervals[ lhs ] );
        }
    }

    void DataFlowAnalysis::propagate( llvm::Value * val ) noexcept
    {
        // TODO refactor

        // memory operations
        if ( auto store = llvm::dyn_cast< llvm::StoreInst >( val ) ) {
            auto op = store->getValueOperand();
            auto ptr = store->getPointerOperand();
            if ( visited( op ) )
                propagate( ptr, _intervals[ op ]->cover() );
            // TODO store to abstract value?
        } else if ( auto load = llvm::dyn_cast< llvm::LoadInst >( val ) ) {
            auto ptr = load->getPointerOperand();
            propagate_wrap( load, ptr );
        } else if ( auto cast = llvm::dyn_cast< llvm::CastInst >( val ) ) {
            auto op = cast->getOperand( 0 );
            propagate_identity( cast, op );
        } else if ( auto gep = llvm::dyn_cast< llvm::GetElementPtrInst >( val ) ) {
            auto ptr = gep->getPointerOperand();
            propagate_wrap( gep, ptr );
        }

        // ssa operations
        if ( is_propagable( val ) ) {
            for ( auto u : val->users() ) {
                if ( util::is_one_of< llvm::LoadInst, llvm::StoreInst
                                    , llvm::CastInst, llvm::GetElementPtrInst >( u ) )
                    push( [=] { propagate( u ); } );
                else if ( auto call = llvm::dyn_cast< llvm::CallInst >( u ) )
                    push( [=] { propagate_in( call ); } );
                else if ( auto ret = llvm::dyn_cast< llvm::ReturnInst >( u ) )
                    push( [=] { propagate_out( ret ); } );
                else if ( join( u, val ) )
                    push( [=] { propagate( u ); } );
            }
        }
    }

    void DataFlowAnalysis::propagate_in( llvm::CallInst * call ) noexcept
    {
        auto fn = call->getCalledFunction();

        for ( const auto & arg : fn->args() ) {
            auto oparg = call->getArgOperand( arg.getArgNo() );
            if ( visited( oparg ) ) {
                auto a = const_cast< llvm::Argument * >( &arg );
                propagate( a, _intervals[ oparg ] );
            }
        }
    }

    void DataFlowAnalysis::propagate_out( llvm::ReturnInst * ret ) noexcept
    {
        auto fn = ret->getFunction();
        for ( auto u : fn->users() ) {
            if ( auto call = llvm::dyn_cast< llvm::CallInst >( u ) )
                if ( join( call, ret->getReturnValue() ) )
                    push( [=] { propagate( call ); } );
        }
    }

    void DataFlowAnalysis::run( llvm::Module &m )
    {
        for ( auto * val : meta::enumerate( m ) ) {
            // TODO preprocessing
            interval( val )->to_top();
            push( [=] { propagate( val ); } );
        }

        while ( !_tasks.empty() ) {
            _tasks.front()();
            _tasks.pop_front();
        }

        for ( const auto & [ val, in ] : _intervals ) {
            // TODO decide what to annotate
            if ( in->is_core() )
                add_meta( val, in );
        }

        auto stores = query::query( m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::StoreInst > )
            .filter( query::notnull )
            .freeze();

        for ( auto store : stores ) {
            auto val = store->getValueOperand();
            if ( _intervals.has( val ) ) {
                auto& mval =_intervals[ val ];
                if ( mval->is_core() )
                    add_meta( store, mval );
            }
        }
    }

    void DataFlowAnalysis::add_meta( llvm::Value * v, const MapValuePtr& mval ) noexcept
    {
        if ( llvm::isa< llvm::Constant >( v ) )
            return;

        AddAbstractMetaVisitor visitor( mval );
        if ( auto inst = llvm::dyn_cast< llvm::Instruction >( v ) ) {
            visitor.visit( inst );
        }
        else if ( auto arg = llvm::dyn_cast< llvm::Argument >( v ) ) {
            // TODO set correct domain kind?
            meta::argument::set( arg, to_string( DomainKind::scalar ) );
        }

        for ( auto u : v->users() ) {
            if ( auto ret = llvm::dyn_cast< llvm::ReturnInst >( u ) ) {
                visitor.visit( ret );
            }
        }
    }

} // namespace lart::abstract
