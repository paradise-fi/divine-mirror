// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2016 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdexcept>

#include <divine/vm/program.hpp>
#include <divine/vm/xg-code.hpp>
#include <lart/divine/cppeh.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Module.h>
#include <llvm/CodeGen/IntrinsicLowering.h>
#include <llvm/ADT/StringMap.h>
DIVINE_UNRELAX_WARNINGS

using namespace divine::vm;
using llvm::isa;
using llvm::dyn_cast;
using llvm::cast;

CodePointer Program::functionByName( std::string s )
{
    llvm::Function *f = module->getFunction( s );
    if ( !f )
        return CodePointer();
    if ( xg::hypercall( f ) )
        return CodePointer();
    if ( !functionmap.count( f ) )
        UNREACHABLE_F( "function %s does not have a functionmap entry", s.c_str() );
    return CodePointer( functionmap[ f ], 0 );
}

GenericPointer Program::globalByName( std::string s )
{
    llvm::GlobalVariable *g = module->getGlobalVariable( s );
    if ( !g )
        return GlobalPointer();
    if ( g->isConstant() )
        return s2ptr( valuemap.find( g->getInitializer() )->second );
    else
        return s2ptr( globalmap.find( g )->second );
}

bool Program::isCodePointerConst( llvm::Value *val )
{
    return ( isa< llvm::Function >( val ) ||
             isa< llvm::BlockAddress >( val ) ||
             isa< llvm::BasicBlock >( val ) );
}

bool Program::isCodePointer( llvm::Value *val )
{
    if ( isa< llvm::BlockAddress >( val ) )
        return true;

    if ( auto ty = dyn_cast< llvm::PointerType >( val->getType() ) )
        return ty->getElementType()->isFunctionTy();

    return isCodePointerConst( val );
}

CodePointer Program::getCodePointer( llvm::Value *val )
{
    if ( auto B = dyn_cast< llvm::BasicBlock >( val ) ) {
        ASSERT( blockmap.count( B ) );
        return blockmap[ B ];
    } else if ( auto B = dyn_cast< llvm::BlockAddress >( val ) ) {
        ASSERT( blockmap.count( B->getBasicBlock() ) );
        return blockmap[ B->getBasicBlock() ];
    } else if ( auto F = dyn_cast< llvm::Function >( val ) ) {
        if ( !functionmap.count( F ) && xg::hypercall( F ) == lx::NotHypercall )
            throw brick::except::Error(
                "Program::insert: " +
                std::string( "Unresolved symbol (function): " ) + F->getName().str() );

        if ( xg::hypercall( F ) )
            return CodePointer();

        return CodePointer( functionmap[ F ], 0 );
    }
    return CodePointer();
}

Program::Slot Program::initSlot( llvm::Value *val, Slot::Location loc )
{
    auto t = val->getType();
    Slot result( loc, xg::type( t ),
                 t->isSized() ? 8 * TD.getTypeAllocSize( t ) : 0 );

    if ( isCodePointer( val ) )
        result.type = Slot::PtrC;

    if ( auto CDS = dyn_cast< llvm::ConstantDataSequential >( val ) )
        ASSERT_EQ( result.width(), 8 * CDS->getNumElements() * CDS->getElementByteSize() );

    if ( isa< llvm::AllocaInst >( val ) || intrinsic_id( val ) == llvm::Intrinsic::stacksave )
    {
        ASSERT_EQ( loc, Slot::Local );
        result.type = Slot::PtrA;
    }

    return result;
}

bool Program::lifetimeOverlap( llvm::Value *v_a, llvm::Value *v_b ) {
    auto a = dyn_cast< llvm::Instruction >( v_a );
    auto b = dyn_cast< llvm::Instruction >( v_b );
    if ( !a || !b )
        return true;

    auto id = a->getMetadata( "lart.id" );
    auto list = b->getMetadata( "lart.interference" );

    if ( !list || !id )
        return true;
    for ( int i = 0; i < int( list->getNumOperands() ); ++i ) {
        auto item = dyn_cast< llvm::MDNode >( list->getOperand( i ) );
        if ( item == id )
            return true;
    }
    return false;
}

void Program::overlaySlot( int fun, Slot &result, llvm::Value *val )
{
    if ( !result.width() )
    {
        result.offset = 0;
        return;
    }

    int iter = 0;
    auto &f = functions[ fun ];
    makeFit( coverage, fun );
    auto &c = coverage[ fun ];
    llvm::Instruction *insn;

    if ( !val )
        goto alloc;

    insn = dyn_cast< llvm::Instruction >( val );
    if ( !insn || !insn->getMetadata( "lart.interference" ) )
        goto alloc;

    ASSERT( !f.instructions.empty() );
    c.resize( f.framesize );

    for ( result.offset = 2 * PointerBytes; result.offset < f.framesize; ) {
        iter ++;
        if ( result.offset + result.size() > f.framesize )
            goto alloc;

        bool good = true;
        for ( int i = 0; good && i < result.size(); ++i )
            for ( auto v : c[ result.offset + i ] )
                if ( lifetimeOverlap( val, v ) ) {
                    ASSERT( valuemap.find( v ) != valuemap.end());
                    auto s = valuemap[ v ].slot;
                    result.offset = s.offset + s.size();
                    good = false;
                    break;
                }

        if ( good )
            goto out;
    }
alloc:
    result.offset = f.framesize;
    f.framesize += result.size();
    c.resize( f.framesize );
out:
    c[ result.offset ].push_back( val );
}

Program::SlotRef Program::insert( int function, llvm::Value *val )
{
    Slot::Location sl;

    /* global scope is only applied to GlobalVariable initializers */
    if ( !function || isa< llvm::Constant >( val ) || isCodePointerConst( val ) )
        sl = Slot::Const;
    else
        sl = Slot::Local;

    auto val_i = valuemap.find( val );
    if ( val_i != valuemap.end() )
    {
        ASSERT_EQ( val_i->second.slot.location, sl );
        return val_i->second;
    }

    if ( auto GA = dyn_cast< llvm::GlobalAlias >( val ) )
    {
        auto r = insert( function, const_cast< llvm::GlobalObject * >( GA->getBaseObject() ) );
        valuemap.insert( std::make_pair( val, r ) );
        ASSERT( r.slot.location != Slot::Invalid );
        return r;
    }

    if ( auto CE = dyn_cast< llvm::ConstantExpr >( val ) )
        initSubcode( CE );

    makeFit( functions, function );

    auto slot = initSlot( val, sl );

    if ( slot.size() % framealign )
        return SlotRef(); /* ignore for now, later pass will assign this one */

    auto sref = allocateSlot( slot, function, val );

    if ( auto G = dyn_cast< llvm::GlobalVariable >( val ) )
    {
        /* G is a pointer type, G's initializer is of the actual value type  */
        if ( !G->hasInitializer() )
            throw std::logic_error(
                "Program::insert: " +
                std::string( "Unresolved symbol (global variable): " ) +
                G->getValueName()->getKey().str() );

        insert( 0, G->getInitializer() );

        if ( !G->isConstant() )
        {
            Slot slot = initSlot( G->getInitializer(), Slot::Global );
            globalmap[ G ] = allocateSlot( slot );
        }
    }
    if ( isa< llvm::Constant >( val ) || isCodePointerConst( val ) )
        _toinit.emplace_back( [=]{ initConstant( sref.slot, val ); } );

    ASSERT( sref.slot.location != Slot::Invalid );
    valuemap.insert( std::make_pair( val, sref ) );

    if ( auto U = dyn_cast< llvm::User >( val ) )
        for ( int i = 0; i < int( U->getNumOperands() ); ++i )
            insert( function, U->getOperand( i ) );

    return sref;
}

void Program::hypercall( Position p )
{
    Program::Instruction &insn = instruction( p.pc );
    llvm::CallSite CS( p.I );
    llvm::Function *F = CS.getCalledFunction();
    if ( insn.opcode != llvm::Instruction::Call )
        throw std::logic_error(
            std::string( "Program::hypercall: " ) +
            "Cannot 'invoke' a hypercall, use 'call' instead: " + F->getName().str() );
    insn.opcode = lx::OpHypercall;
    insn.subcode = xg::hypercall( F );
    if ( insn.subcode == lx::NotHypercall )
        throw std::logic_error(
            std::string( "Program::hypercall: " ) +
            "Can't call an undefined function: " + F->getName().str() );
}

template< typename Insn >
void Program::insertIndices( Position p )
{
    Insn *I = cast< Insn >( p.I );
    Program::Instruction &insn = instruction( p.pc );

    int shift = insn.values.size();
    insn.values.resize( shift + I->getNumIndices() );

    for ( unsigned i = 0; i < I->getNumIndices(); ++i )
    {
        Slot v( Slot::Const, Slot::I32 );
        auto sr = allocateSlot( v );
        _toinit.emplace_back(
            [=]{ initConstant( sr.slot, value::Int< 32 >( I->getIndices()[ i ] ) ); } );
        insn.values[ shift + i ] = sr.slot;
    }
}

Program::Position Program::insert( Position p )
{
    makeFit( function( p.pc ).instructions, p.pc.instruction() );
    Program::Instruction &insn = instruction( p.pc );

    insn.opcode = p.I->getOpcode();
    insn.subcode = initSubcode( &*p.I );

    if ( dyn_cast< llvm::CallInst >( p.I ) ||
         dyn_cast< llvm::InvokeInst >( p.I ) )
    {
        llvm::CallSite CS( p.I );
        llvm::Function *F = CS.getCalledFunction();
        if ( F ) // you can actually invoke a label
            switch ( F->getIntrinsicID() )
            {
                case llvm::Intrinsic::not_intrinsic:
                    if ( xg::hypercall( F ) )
                        hypercall( p );
                    break;
                case llvm::Intrinsic::dbg_declare:
                case llvm::Intrinsic::dbg_value: p.I++; return p;
                default: ;
            }
    }

    if ( !codepointers )
    {
        insn.values.resize( 1 + p.I->getNumOperands() );
        for ( int i = 0; i < int( p.I->getNumOperands() ); ++i )
            insn.values[ i + 1 ] = insert( p.pc.function(), p.I->getOperand( i ) ).slot;
        insn.values[0] = insert( p.pc.function(), &*p.I ).slot;

        if ( auto PHI = dyn_cast< llvm::PHINode >( p.I ) )
            for ( unsigned idx = 0; idx < PHI->getNumOperands(); ++idx )
            {
                auto from = pcmap[ PHI->getIncomingBlock( idx )->getTerminator() ];
                auto slot = allocateSlot( Slot( Slot::Const, Slot::PtrC ) ).slot;
                _toinit.emplace_back( [=]{ initConstant( slot, value::Pointer( from ) ); } );
                insn.values.push_back( slot );
            }

        if ( isa< llvm::ExtractValueInst >( p.I ) )
            insertIndices< llvm::ExtractValueInst >( p );

        if ( isa< llvm::InsertValueInst >( p.I ) )
            insertIndices< llvm::InsertValueInst >( p );
    } else
        pcmap.insert( std::make_pair( p.I, p.pc ) );

    ++ p.I; /* next please */
    p.pc.instruction( p.pc.instruction() + 1 );

    if ( !p.pc.instruction() )
        throw std::logic_error(
            "Program::insert() in " + p.I->getParent()->getParent()->getName().str() +
            "\nToo many instructions in a function, capacity exceeded" );

    return p;
}

void Program::setupRR()
{
    /* null pointers are heap = 0, segment = 0, where segment is an index into
     * the "globals" vector */
    Slot nullpage( Slot::Global );
    _globals.push_back( nullpage );

    for ( auto &f : *module )
        if ( f.getName() == "memset" || f.getName() == "memmove" || f.getName() == "memcpy" )
            f.setLinkage( llvm::GlobalValue::ExternalLinkage );
}

void Program::computeRR()
{
    framealign = 1;

    codepointers = true;
    pass();
    codepointers = false;

    for ( auto var = module->global_begin(); var != module->global_end(); ++ var )
        insert( 0, &*var );

    framealign = 4;
    pass();

    framealign = 1;
    pass();

    _types.reset( new LXTypes( _ccontext._heap, _types_gen.emit( _ccontext._heap ) ) );
    coverage.clear();
}

void Program::computeStatic()
{
    _ccontext.setup( _globals_size, _constants_size );

    while ( !_toinit.empty() )
    {
        _toinit.front()();
        _toinit.pop_front();
    }

    for ( auto var = module->global_begin(); var != module->global_end(); ++ var )
    {
        if ( !var->getInitializer() || var->isConstant() )
            continue;
        ASSERT( globalmap.find( var ) != globalmap.end() );
        ASSERT( valuemap.find( var->getInitializer() ) != valuemap.end() );
        auto location = valuemap[ var->getInitializer() ];
        auto target = globalmap[ var ];
        _ccontext.heap().copy( s2hptr( location.slot ), s2hptr( target.slot ), location.slot.size() );
    }

    auto md_func_var = module->getGlobalVariable( "__md_functions" );
    if ( !md_func_var )
        return;

    auto md_func = dyn_cast< llvm::ConstantArray >( md_func_var->getInitializer() );
    ASSERT( valuemap.count( md_func ) );
    auto slotref = valuemap[ md_func ];

    auto md2name = [&]( auto op )
    {
        return std::string( dyn_cast< llvm::ConstantStruct >( op )->
                                getOperand( 0 )->getOperand( 0 )->getName(),
                            strlen( "lart.divine.index.name." ), std::string::npos );
    };

    auto md2pc = [&]( auto op ) { return functionByName( md2name( op ) ); };

    auto instTableT = llvm::cast< llvm::PointerType >(
                          md_func->getOperand( 0 )->getOperand( 7 )->getType() )->getElementType();
    int instTotal = 0;

    for ( int i = 0; i < int( md_func->getNumOperands() ); ++i )
    {
        auto pc = md2pc( md_func->getOperand( i ) );
        if ( !pc.function() )
            continue;

        instTotal += function( pc ).instructions.size();
    }

    auto instTableObj = _ccontext.heap().make( instTotal * TD.getTypeAllocSize( instTableT ) );
    int instOffset = 0;

    for ( int i = 0; i < int( md_func->getNumOperands() ); ++i )
    {
        auto op = dyn_cast< llvm::ConstantStruct >( md_func->getOperand( i ) );
        const llvm::StructLayout *SL_item = TD.getStructLayout( op->getType() );
        auto pc = md2pc( op );
        if ( !pc.function() )
            continue;
        auto &func = function( pc );

        auto writeMetaElem = [&]( int idx, auto val ) {
            int offset = TD.getTypeAllocSize( md_func->getOperand( 0 )->getType() ) * i
                          + SL_item->getElementOffset( idx );
            _ccontext.heap().write( s2hptr( slotref.slot, offset ), val );
        };
        writeMetaElem( 2, value::Int< 32 >( func.framesize ) ); // frame size
        writeMetaElem( 6, value::Int< 32 >( func.instructions.size() ) ); // inst count

        // create and write write instruction table
        auto instTable = instTableObj + instOffset;
        int instTableSize = TD.getTypeAllocSize( instTableT ) * func.instructions.size();
        writeMetaElem( 7, instTable );

        auto writeInst = [&]( int val ) {
            _ccontext.heap().write_shift( instTable, value::Int< 32 >( val ) );
        };
        for ( int j = 0; j < int( func.instructions.size() ); ++j )
        {
            auto &inst = func.instructions[ j ];
            writeInst( inst.opcode );
            writeInst( inst.subcode );
            writeInst( inst.values.empty() ? 0 : inst.result().offset );
            writeInst( inst.values.empty() ? 0 : inst.result().size() ); /* bytes? */
        }
        ASSERT_EQ( instTable.cooked().offset() - instOffset, instTableSize );

        instOffset += instTableSize;
    }
}

void Program::pass()
{
    CodePointer pc( 0, 0 );
    int _framealign = framealign;

    for ( auto &function : *module )
    {
        if ( function.isDeclaration() )
            continue; /* skip */

        auto name = function.getName();

        pc.function( pc.function() + 1 );
        if ( !pc.function() )
            throw std::logic_error(
                "Program::build in " + name.str() +
                "\nToo many functions, capacity exceeded" );

        if ( function.begin() == function.end() )
            throw std::logic_error(
                "Program::build in " + name.str() +
                "\nCan't deal with empty functions" );

        makeFit( functions, pc.function() );
        functionmap[ &function ] = pc.function();
        pc.instruction( 0 );

        if ( !codepointers )
        {
            framealign = 1; /* force all args to go in in the first pass */

            auto &pi_function = this->functions[ pc.function() ];
            pi_function.argcount = 0;
            if ( function.hasPersonalityFn() )
                pi_function.personality = insert( 0, function.getPersonalityFn() ).slot;

            for ( auto &arg : function.args() )
            {
                insert( pc.function(), &arg );
                ++ pi_function.argcount;
            }

            if ( ( pi_function.vararg = function.isVarArg() ) )
            {
                Slot vaptr( Slot::Local );
                vaptr.type = Slot::Ptr;
                allocateSlot( vaptr, pc.function() );
            }

            framealign = _framealign;

            this->function( pc ).framesize =
                mem::align( this->function( pc ).framesize, framealign );
        }

        for ( auto &block : function )
        {
            blockmap[ &block ] = pc;
            makeFit( this->function( pc ).instructions, pc.instruction() );
            this->instruction( pc ).opcode = lx::OpBB;
            pc.instruction( pc.instruction() + 1 ); /* leave one out for use as a bb label */

            if ( block.begin() == block.end() )
                throw std::logic_error(
                    std::string( "Program::build: " ) +
                    "Can't deal with an empty BasicBlock in function " + name.str() );

            Program::Position p( pc, block.begin() );
            while ( p.I != block.end() )
                p = insert( p );

            pc = p.pc;
        }
    }

    _globals_size = mem::align( _globals_size, 4 );
}

