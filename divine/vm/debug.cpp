// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/debug.hpp>
#include <divine/vm/print.hpp>
#include <divine/vm/eval.hpp>

namespace divine {
namespace vm {

std::pair< std::string, int > fileline( const llvm::Instruction &insn )
{
    auto loc = insn.getDebugLoc().get();
    if ( loc && loc->getNumOperands() )
        return std::make_pair( loc->getFilename().str(),
                               loc->getLine() );
    auto prog = llvm::getDISubprogram( insn.getParent()->getParent() );
    if ( prog )
        return std::make_pair( prog->getFilename().str(),
                               prog->getScopeLine() );
    return std::make_pair( "", 0 );
}

std::string location( const llvm::Instruction &insn )
{
    auto fl = fileline( insn );
    if ( fl.second )
        return fl.first + ":" + brick::string::fmt( fl.second );
    return "(unknown location)";
}

template< typename Prog, typename Heap >
using DNEval = Eval< Prog, ConstContext< Prog, Heap >, value::Void >;

template< typename Prog, typename Heap >
int DebugNode< Prog, Heap >::size()
{
    int sz = INT_MAX;
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    if ( _type )
        sz = _ctx.program().TD.getTypeAllocSize( _type );
    if ( !_address.null() )
        sz = std::min( sz, eval.ptr2sz( PointerV( _address ) ) - _offset );
    return sz;
}

template< typename Prog, typename Heap >
llvm::DIDerivedType *DebugNode< Prog, Heap >::di_derived( uint64_t tag, llvm::DIType *t )
{
    t = t ?: _di_type;
    if ( !t )
        return nullptr;
    auto derived = llvm::dyn_cast< llvm::DIDerivedType >( t );
    if ( derived && derived->getTag() == tag )
        return derived;
    return nullptr;
}

template< typename Prog, typename Heap >
llvm::DIDerivedType *DebugNode< Prog, Heap >::di_member( llvm::DIType *t )
{
    return di_derived( llvm::dwarf::DW_TAG_member, t );
}

template< typename Prog, typename Heap >
llvm::DIDerivedType *DebugNode< Prog, Heap >::di_pointer( llvm::DIType *t )
{
    return di_derived( llvm::dwarf::DW_TAG_pointer_type, t );
}

template< typename Prog, typename Heap >
llvm::DIType *DebugNode< Prog, Heap >::di_base( llvm::DIType *t )
{
    if ( di_member( t ) )
        return di_member( t )->getBaseType().resolve( _ctx.program().ditypemap );
    if ( di_pointer( t ) )
        return di_pointer( t )->getBaseType().resolve( _ctx.program().ditypemap );
    return nullptr;
}

template< typename Prog, typename Heap >
llvm::DICompositeType *DebugNode< Prog, Heap >::di_composite()
{
    return llvm::dyn_cast< llvm::DICompositeType >( di_base() ?: _di_type );
}

template< typename Prog, typename Heap >
int DebugNode< Prog, Heap >::width()
{
    if ( di_member() )
        return di_member()->getSizeInBits();
    return 8 * size();
}

template< typename Prog, typename Heap >
int DebugNode< Prog, Heap >::bitoffset()
{
    int rv = 0;
    if ( di_member() )
        rv = di_member()->getOffsetInBits() - 8 * _offset;
    ASSERT_LEQ( rv, 8 * size() );
    ASSERT_LEQ( rv + width(), 8 * size() );
    return rv;
}

template< typename Prog, typename Heap >
bool DebugNode< Prog, Heap >::valid()
{
    if ( _address.null() )
        return false;
    if ( _address.type() == PointerType::Heap && !_ctx.heap().valid( _address ) )
        return false;

    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    PointerV addr( _address );
    if ( !eval.boundcheck( []( auto ) { return std::stringstream(); }, addr, 1, false ) )
        return false;
    if ( !eval.boundcheck( []( auto ) { return std::stringstream(); }, addr, size(), false ) )
        return false;
    return true;
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::value( YieldAttr yield )
{
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    PointerV loc( _address + _offset );
    if ( _type && _type->isIntegerTy() )
        eval.template type_dispatch< IsIntegral >(
            _type->getPrimitiveSizeInBits(), Prog::Slot::Integer,
            [&]( auto v )
            {
                auto raw = v.get( loc );
                using V = decltype( raw );
                if ( bitoffset() || width() != size() * 8 )
                {
                    yield( "@raw_value", brick::string::fmt( raw ) );
                    auto val = raw >> value::Int< 32 >( bitoffset() );
                    val = val & V( bitlevel::ones< typename V::Raw >( width() ) );
                    ASSERT_LEQ( bitoffset() + width(), size() * 8 );
                    yield( "@value", brick::string::fmt( val ) );
                }
                else
                    yield( "@value", brick::string::fmt( raw ) );
            } );
    if ( _type && _type->isPointerTy() )
        eval.template type_dispatch< Any >(
            PointerBytes, Prog::Slot::Pointer,
            [&]( auto v ) { yield( "@value", brick::string::fmt( v.get( loc ) ) ); } );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::attributes( YieldAttr yield )
{
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    Prog &program = _ctx.program();

    yield( "@address", brick::string::fmt( _address ) + "+" +
           brick::string::fmt( _offset ) );

    std::string typesuf;
    auto dit = _di_type, base = di_base();
    do
        typesuf += di_pointer( dit ) ? "*" : "", base = dit;
    while (( dit = di_base( dit ) ));

    if ( base )
        yield( "@type", base->getName().str() + typesuf );

    if ( !valid() )
        return;

    auto hloc = eval.ptr2h( PointerV( _address ) );
    value( yield );

    yield( "@raw", print::raw( _ctx.heap(), hloc + _offset, size() ) );

    if ( _address.type() == PointerType::Const || _address.type() == PointerType::Global )
        yield( "@slot", brick::string::fmt( eval.ptr2s( _address ) ) );

    if ( _kind == DNKind::Frame )
    {
        yield( "@pc", brick::string::fmt( pc() ) );
        if ( pc().null() || pc().type() != PointerType::Code )
            return;
        auto *insn = &program.instruction( pc() );
        if ( insn->op )
        {
            eval._instruction = insn;
            yield( "@instruction", print::instruction( eval ) );
        }
        if ( !insn->op )
            insn = &program.instruction( pc() + 1 );
        ASSERT( insn->op );
        auto op = llvm::cast< llvm::Instruction >( insn->op );
        yield( "@location", location( *op ) );

        auto sym = op->getParent()->getParent()->getName().str();
        yield( "@symbol", print::demangle( sym ) );
    }
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::bitcode( std::ostream &out )
{
    ASSERT_EQ( _kind, DNKind::Frame );
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );
    CodePointer iter = pc();
    iter.instruction( 0 );
    auto &instructions = _ctx.program().function( iter ).instructions;
    for ( auto &i : instructions )
    {
        eval._instruction = &i;
        out << ( iter == CodePointer( pc() ) ? ">>" : "  " );
        if ( i.op )
            out << "  " << print::instruction( eval, 4 ) << std::endl;
        else
        {
            auto iop = llvm::cast< llvm::Instruction >( instructions[ iter.instruction() + 1 ].op );
            out << print::value( eval, iop->getParent() ) << ":" << std::endl;
        }
        iter = iter + 1;
    }
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::source( std::ostream &out )
{
    ASSERT_EQ( _kind, DNKind::Frame );
    out << print::source( subprogram(), _ctx.program(), pc() );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::related( YieldDN yield )
{
    if ( !valid() )
        return;

    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );

    PointerV ptr;
    auto hloc = eval.ptr2h( PointerV( _address ) );
    int hoff = hloc.offset();

    _related_ptrs.clear();
    _related_count.clear();

    if ( _kind == DNKind::Frame )
        framevars( yield );

    if ( _type && _di_type && _type->isPointerTy() )
    {
        PointerV addr;
        _ctx.heap().read( hloc + _offset, addr );
        _related_ptrs.insert( addr.cooked() );
        auto kind = DNKind::Object;
        auto base = di_base( di_base() );
        if ( base && base->getName() == "_VM_Frame" )
            kind = DNKind::Frame;
        DebugNode rel( _ctx, _snapshot );
        rel.address( kind, addr.cooked() );
        rel.type( _type->getPointerElementType() );
        rel.di_type( di_base() );
        yield( "@deref", rel );
    }

    if ( _type && _di_type && _type->isStructTy() )
        struct_fields( hloc, yield );

    for ( auto ptroff : _ctx.heap().pointers( hloc, hoff + _offset, size() ) )
    {
        hloc.offset( hoff + _offset + ptroff->offset() );
        _ctx.heap().read( hloc, ptr );
        auto pp = ptr.cooked();
        if ( pp.type() == PointerType::Code || _related_ptrs.find( pp ) != _related_ptrs.end() )
            continue;
        pp.offset( 0 );
        DebugNode deref( _ctx, _snapshot );
        deref.address( DNKind::Object, pp );
        yield( "@" + brick::string::fmt( ptroff->offset() ), deref );
    }
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::struct_fields( HeapPointer hloc, YieldDN yield )
{
    llvm::DIType *base = _di_type;
    llvm::DIDerivedType *DT = nullptr;

    do if ( DT = llvm::dyn_cast< llvm::DIDerivedType >( base ) )
           base = DT->getBaseType().resolve( _ctx.program().ditypemap );
    while ( DT );

    auto CT = llvm::dyn_cast< llvm::DICompositeType >( base );
    auto ST = llvm::cast< llvm::StructType >( _type );
    auto STE = ST->element_begin();
    auto SLO = _ctx.program().TD.getStructLayout( ST );
    int idx = 0;
    for ( auto subtype : CT->getElements() )
        if ( auto CTE = llvm::dyn_cast< llvm::DIDerivedType >( subtype ) )
        {
            if ( idx + 1 < int( ST->getNumElements() ) &&
                 CTE->getOffsetInBits() >= 8 * SLO->getElementOffset( idx + 1 ) )
                idx ++, STE ++;

            int offset = SLO->getElementOffset( idx );
            if ( (*STE)->isPointerTy() )
            {
                PointerV ptr;
                _ctx.heap().read( hloc + offset, ptr );
                _related_ptrs.insert( ptr.cooked() );
            }

            DebugNode field( _ctx, _snapshot );
            field.address( DNKind::Object, _address );
            field.offset( _offset + offset );
            field.type( *STE );
            field.di_type( CTE );
            yield( CTE->getName().str(), field );
        }
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::localvar( YieldDN yield, llvm::DbgDeclareInst *DDI )
{
    DNEval< Prog, Heap > eval( _ctx.program(), _ctx );

    auto divar = DDI->getVariable();
    auto ditype = divar->getType().resolve( _ctx.program().ditypemap );
    auto var = DDI->getAddress();
    auto &vmap = _ctx.program().valuemap;
    if ( vmap.find( var ) == vmap.end() )
        return;

    PointerV ptr;
    _ctx.heap().read( eval.s2ptr( _ctx.program().valuemap[ var ].slot ), ptr );
    _related_ptrs.insert( ptr.cooked() );

    auto type = var->getType()->getPointerElementType();
    auto name = divar->getName().str();

    if ( divar->getScope() != subprogram() )
        name += "$" + brick::string::fmt( ++ _related_count[ name ] );

    DebugNode lvar( _ctx, _snapshot );
    lvar.address( DNKind::Object, ptr.cooked() );
    lvar.type( type );
    lvar.di_type( ditype );
    yield( name, lvar );
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::framevars( YieldDN yield )
{
    PointerV fr( _address );
    _ctx.heap().skip( fr, PointerBytes );
    _ctx.heap().read( fr.cooked(), fr );
    if ( !fr.cooked().null() )
    {
        _related_ptrs.insert( fr.cooked() );
        DebugNode parent( _ctx, _snapshot );
        parent.address( DNKind::Frame, fr.cooked() );
        yield( "@parent", parent );
    }

    auto *insn = &_ctx.program().instruction( pc() );
    if ( !insn->op )
        insn = &_ctx.program().instruction( pc() + 1 );
    auto op = llvm::cast< llvm::Instruction >( insn->op );
    auto F = op->getParent()->getParent();

    for ( auto &BB : *F )
        for ( auto &I : BB )
            if ( auto DDI = llvm::dyn_cast< llvm::DbgDeclareInst >( &I ) )
                localvar( yield, DDI );
}

static std::string rightpad( std::string s, int i )
{
    while ( int( s.size() ) < i )
        s += ' ';
    return s;
}

template< typename Prog, typename Heap >
void DebugNode< Prog, Heap >::format( std::ostream &out, int depth, bool compact, int indent )
{
    std::string ind;
    if ( indent >= 2 )
    {
        ind = std::string( indent - 2, ' ' );
        ind += "| ";
    }

    std::set< std::string > ck{ "@value", "@type", "@location", "@symbol" };

    attributes(
        [&]( std::string k, auto v )
        {
            if ( k == "@raw" || ( compact && ck.count( k ) == 0 ) )
                return;
            out << ind << rightpad( k + ": ", 14 - indent ) << v << std::endl;
        } );

    int col = 0, relrow = 0;

    std::stringstream rels;

    if ( depth > 0 )
        related(
            [&]( std::string n, auto sub )
            {
                std::stringstream str;
                sub.format( str, depth - 1, true, indent + 2 );
                if ( !str.str().empty() && n != "@parent" && depth > 0 )
                    out << ind << n << ":" << std::endl << str.str();
                else
                {
                    if ( indent + col + n.size() >= 68 )
                    {
                        if ( relrow == 3 )
                            rels << "[...]";
                        else if ( relrow < 3 )
                            col = 0, rels << std::endl << ind << rightpad( "", 13 - indent );
                        ++ relrow;
                    }
                    rels << " " << n;
                    col += n.size();
                }
            } );
    if ( !rels.str().empty() )
        out << rightpad( "related:", 13 - indent ) << rels.str() << std::endl;
}

template< typename Prog, typename Heap >
std::string DebugNode< Prog, Heap >::attribute( std::string key )
{
    std::string res = "-";
    attributes( [&]( auto k, auto v )
                {
                    if ( k == key )
                        res = v;
                } );
    return res;
}

template struct DebugNode< Program, CowHeap >;

}
}
