// -*- C++ -*- (c) 2011, 2012 Petr Rockai <me@mornfall.net>

#define NO_RTTI
#include <divine/llvm/machine.h>

using namespace divine::llvm;

void MachineState::rewind( Blob to, int thread )
{
    _alloc.pool().free( _blob );
    _blob = _alloc.pool().allocate( _alloc.pool().size( to ) );
    _alloc.pool().copy( to, _blob );

    _thread = -1; // special

    _thread_count = threads().get().length();
    nursery.reset( heap().segcount );
    freed.clear();
    problems.clear();
    for ( int i = 0; i < int( _stack.size() ); ++i ) /* deactivate detached stacks */
        _stack[i].first = false;

    if ( thread >= 0 && thread < _thread_count )
        switch_thread( thread );

}

void MachineState::switch_thread( int thread )
{
    _thread = thread;
    if ( stack().get().length() )
        _frame = &stack().get( stack().get().length() - 1 );
    else
        _frame = nullptr;
}

int MachineState::new_thread()
{
    detach_stack( _thread_count, 0 );
    _thread = _thread_count ++;
    stack().get().length() = 0;
    return _thread;
}

int MachineState::pointerSize( Pointer p )
{
    if ( !validate( p ) )
        return 0;

    /*
     * Pointers into global memory are not used for copying during
     * canonization, so we don't actually need to know their size.
     */
    assert( !globalPointer( p ) );

    if ( heap().owns( p ) )
        return heap().size( p );
    else
        return nursery.size( p );
}

struct divine::llvm::Canonic
{
    MachineState &ms;
    std::map< int, int > segmap;
    int allocated, segcount;
    int stack;
    int boundary, segdone;

    Canonic( MachineState &ms )
        : ms( ms ), allocated( 0 ), segcount( 0 ), stack( 0 ), boundary( 0 ), segdone( 0 )
    {}

    Pointer operator[]( Pointer idx ) {
        if ( !idx.heap || !ms.validate( idx ) )
            return idx;
        if ( !segmap.count( idx.segment ) ) {
            segmap.insert( std::make_pair( int( idx.segment ), segcount ) );
            ++ segcount;
            allocated += ms.pointerSize( idx );
        }
        return Pointer( idx.heap, segmap[ int( idx.segment ) ], idx.offset );
    }

    bool seen( Pointer p ) {
        return segmap.count( p.segment );
    }
};

void MachineState::trace( Pointer p, Canonic &canonic )
{
    if ( p.heap && !freed.count( p.segment ) && !canonic.seen( p ) ) {
        int size = pointerSize( p );
        canonic[ p ];
        for ( p.offset = 0; p.offset < size; p.offset += 4 )
            if ( memoryflag( p ).get() == MemoryFlag::HeapPointer ) {
                trace( followPointer( p ), canonic );
            }
    }
}

template< typename Fun >
void forPointers( MachineState::Frame &f, ProgramInfo &i, ValueRef v, Fun fun )
{
    if ( f.memoryflag( i, v ).get() == MemoryFlag::HeapPointer )
        fun( v, *f.dereference< Pointer >( i, v ) );
/*
    while ( v.offset < v.v.width - 4 ) {
        v.offset += 4;
        if ( f.isPointer( i, v ) )
            fun( v, *f.dereference< Pointer >( i, v ) );
    }
*/
}

void MachineState::trace( Frame &f, Canonic &canonic )
{
    auto vals = _info.function( f.pc ).values;
    for ( auto val : vals )
        forPointers( f, _info, val, [&]( ValueRef, Pointer p ) {
                this->trace( p, canonic );
            } );
    canonic.stack += sizeof( f ) + f.framesize( _info );
}

void MachineState::snapshot( Pointer &edit, Pointer original, Canonic &canonic, Heap &heap )
{
    if ( !original.heap ) { /* non-heap pointers are always canonic */
        edit = original;
        return;
    }

    /* clear invalid pointers, in case they would accidentally become valid later */
    if ( !validate( original ) ) {
        if ( original.null() )
            edit = Pointer( false, 0, 0 );
        else
            edit = Pointer( false, 0, 1 ); /* not NULL, so we can detect an invalid free */
        return;
    }

    assert( canonic.seen( original ) );
    edit = canonic[ original ]; /* canonize */

    if ( edit.segment < canonic.segdone )
        return; /* we already followed this pointer */

    Pointer edited = edit;
    assert_eq( int( edit.segment ), canonic.segdone );
    canonic.segdone ++;

    /* Re-allocate the object... */
    int size = pointerSize( original );
    heap.jumptable( edit ) = canonic.boundary / 4;
    canonic.boundary += size;
    heap.jumptable( edit.segment + 1 ) = canonic.boundary / 4;

    /* And trace it. We work in 4 byte steps, pointers are 4 bytes and 4-byte aligned. */
    original.offset = edited.offset = 0;
    for ( ; original.offset < size; original.offset += 4, edited.offset += 4 ) {
        bool recurse = this->memoryflag( original ).get() == MemoryFlag::HeapPointer;
        if ( recurse )
            heap.memoryflag( edited ).set( MemoryFlag::HeapPointer );
        if ( recurse ) /* points to a pointer, recurse */
            snapshot( *heap.dereference< Pointer >( edited ),
                      followPointer( original ), canonic, heap );
        else
            memcopy( original, edited, 4, *this, heap );
    }
}

void MachineState::snapshot( Frame &f, Canonic &canonic, Heap &heap, StateAddress &address )
{
    auto vals = _info.function( f.pc ).values;
    Frame &target = address.as< Frame >();
    target.pc = f.pc;

    for ( auto val = vals.begin(); val != vals.end(); ++val ) {
        /* make a straight copy first, we will rewrite pointers next */
        FrameContext fctx( _info, f ), tctx( _info, target );
        memcopy( *val, *val, val->width, fctx, tctx );
        forPointers( f, _info, *val, [&]( ValueRef v, Pointer p ) {
                target.memoryflag( _info, v ).set( MemoryFlag::HeapPointer );
                snapshot( *target.dereference< Pointer >( _info, v ), p, canonic, heap );
            } );
    }

    address = target.advance( address, 0 );
    assert_eq( (address.offset - _alloc._slack) % 4, 0 );
}

divine::Blob MachineState::snapshot()
{
    Canonic canonic( *this );
    int dead_threads = 0;

    /* TODO inefficient, split globals into globals and constants */
    for ( int i = 0; i < int( _info.globals.size() ); ++i ) {
        ProgramInfo::Value v = _info.globals[i];
        if ( v.constant )
            continue;
        Pointer p( false, i, 0 );
        for ( p.offset = 0; p.offset < v.width; p.offset += 4 )
            if ( global().memoryflag( _info, p ).get() == MemoryFlag::HeapPointer )
                trace( followPointer( p ), canonic );
    }

    for ( int tid = 0; tid < _thread_count; ++tid ) {
        if ( !stack( tid ).get().length() ) { /* leave out dead threads */
            ++ dead_threads;
            continue;
        }

        /* non-tail dead threads become zombies, with 4 byte overhead; put them back */
        canonic.stack += dead_threads * sizeof( Stack );
        dead_threads = 0;
        canonic.stack += sizeof( Stack );
        eachframe( stack( tid ), [&]( Frame &fr ) {
                trace( fr, canonic );
            } );
    }

    /* we only store new problems, discarding those we inherited */
    for ( int i = 0; i < problems.size(); ++i )
        if ( !problems[ i ].pointer.null() )
            trace( problems[ i ].pointer, canonic );

    Pointer p( true, 0, 0 );
    for ( p.segment = 0; p.segment < heap().segcount + nursery.offsets.size() - 1; ++ p.segment )
        if ( !canonic.seen( p ) && !freed.count( p.segment ) ) {
            trace( p, canonic );
            problem( Problem::MemoryLeak, p );
        }

    Blob b = _alloc.makeBlobCleared(
        size( canonic.stack, canonic.allocated, canonic.segcount, problems.size() ) );

    StateAddress address( &_alloc.pool(), &_info, b, _alloc._slack );
    Flags &fl = address.as< Flags >();
    fl = flags();
    fl.problemcount = problems.size();
    address.advance( sizeof( Flags ) );

    for ( int i = 0; i < problems.size(); ++i ) {
        address.advance( sizeof( Problem ) );
        fl.problems( i ) = problems[ i ];
    }

    Globals *_global = &address.as< Globals >();
    address = state().sub( Globals() ).copy( address );

    /* skip the heap */
    Heap *_heap = &address.as< Heap >();
    _heap->segcount = canonic.segcount;
    /* heap needs to know its size in order to correctly dereference! */
    _heap->jumptable( canonic.segcount ) = canonic.allocated / 4;
    address.advance( size_heap( canonic.segcount, canonic.allocated ) );
    assert_eq( size_heap( canonic.segcount, canonic.allocated ) % 4, 0 );

    address.as< int >() = _thread_count - dead_threads;
    address.advance( sizeof( int ) ); // ick. length of the threads array

    for ( int i = 0; i < int( _info.globals.size() ); ++i ) {
        ProgramInfo::Value v = _info.globals[i];
        if ( v.constant )
            continue;
        Pointer p( false, i, 0 );
        for ( p.offset = 0; p.offset < v.width; p.offset += 4 )
            if ( global().memoryflag( _info, p ).get() == MemoryFlag::HeapPointer )
                snapshot( *(_global->dereference< Pointer >( _info, p )),
                          followPointer( p ), canonic, *_heap );
    }

    for ( int tid = 0; tid < _thread_count - dead_threads; ++tid ) {
        address.as< int >() = stack( tid ).get().length();
        address.advance( sizeof( int ) );
        eachframe( stack( tid ), [&]( Frame &fr ) {
                snapshot( fr, canonic, *_heap, address );
            });
    }

    for ( int i = 0; i < fl.problemcount; ++i ) {
        auto &p = fl.problems( i ).pointer;
        if ( !p.null() )
            snapshot( p, p, canonic, *_heap );
    }

    assert_eq( canonic.segdone, canonic.segcount );
    assert_eq( canonic.boundary, canonic.allocated );
    assert_eq( address.offset, _alloc.pool().size( b ) );
    assert_eq( (_alloc.pool().size( b ) - _alloc._slack) % 4, 0 );

    return b;
}


void MachineState::problem( Problem::What w, Pointer ptr )
{
    Problem p;
    p.what = w;
    if ( _frame )
        p.where = frame().pc;
    p.tid = _thread;
    p.pointer = ptr;
    problems.push_back( p );
}

bool MachineState::isPrivate( Pointer needle, Pointer p, Canonic &canonic )
{
    if ( p.heap && needle.segment == p.segment )
        return false;

    if ( canonic.seen( p ) )
        return true;

    canonic[p];

    if ( p.heap && !freed.count( p.segment ) ) {
        int size = pointerSize( p );
        for ( p.offset = 0; p.offset < size; p.offset += 4 )
            if ( memoryflag( p ).get() == MemoryFlag::HeapPointer ) {
                if ( !isPrivate( needle, followPointer( p ), canonic ) )
                    return false;
            }
    }

    return true;
}

bool MachineState::isPrivate( Pointer needle, Frame &f, Canonic &canonic )
{
    bool result = true;

    auto vals = _info.function( f.pc ).values;
    for ( auto val : vals ) {
        forPointers( f, _info, val, [&]( ValueRef, Pointer p ) {
                if ( !isPrivate( needle, p, canonic ) )
                    result = false;
            } );
        if ( !result ) break; /* bail early */
    }
    return result;
}

bool MachineState::isPrivate( int tid, Pointer needle )
{
    if ( !needle.heap ) /* globals are never private */
        return false;

    bool found = false;

    Canonic canonic( *this );

    for ( int i = 0; i < int( _info.globals.size() ); ++i ) {
        ProgramInfo::Value v = _info.globals[i];
        if ( v.constant )
            continue;
        Pointer p( false, i, 0 );
        for ( p.offset = 0; p.offset < v.width; p.offset += 4 )
            if ( global().memoryflag( _info, p ).get() == MemoryFlag::HeapPointer )
                if ( !isPrivate( needle, followPointer( p ), canonic ) )
                    return false;
    }

    struct Found {};

    try {
        for ( int search = 0; search < _thread_count; ++search ) {
            if ( tid == search || found )
                continue;
            eachframe( stack( search ), [&]( Frame &fr ) {
                    if ( !isPrivate( needle, fr, canonic ) )
                        throw Found();
                } );
        }
    } catch ( Found ) {
        return false;
    }

    return true; /* not found */
}
