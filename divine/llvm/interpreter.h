// -*- C++ -*- (c) 2011, 2012 Petr Rockai <me@mornfall.net>

#define NO_RTTI

#include <divine/llvm/program.h>
#include <divine/llvm/machine.h>
#include <divine/graph/allocator.h> // hmm.

#include <llvm/Function.h>
#include <llvm/Module.h>
#include <llvm/Instructions.h>
#include <llvm/Target/TargetData.h>

#ifndef DIVINE_LLVM_INTERPRETER_H
#define DIVINE_LLVM_INTERPRETER_H

namespace divine {
namespace llvm {

struct Interpreter;

using namespace ::llvm;

struct Interpreter
{
    TargetData TD;
    std::map< std::string, std::string > properties;
    ::llvm::Module *module; /* The bitcode. */
    MachineState state; /* the state we are dealing with */
    ProgramInfo info;
    Allocator &alloc;

    bool jumped;
    int choice;

    void parseProperties( Module *M );
    bool observable() {
        return isa< StoreInst >( instruction().op );
    }

    // the currently executing one, i.e. what pc of the top frame of the active thread points at
    ProgramInfo::Instruction instruction() { return info.instruction( pc() ); }
    MDNode *node( MDNode *root ) { return root; }

    template< typename N, typename... Args >
    MDNode *node( N *root, int n, Args... args ) {
        if (root)
            return node( cast< MDNode >( root->getOperand(n) ), args... );
        return NULL;
    }

    MDNode *findEnum( std::string lookup ) {
        assert( module );
        MDNode *enums = node( module->getNamedMetadata( "llvm.dbg.cu" ), 0, 10, 0 );
        if ( !enums )
            return NULL;
        for ( int i = 0; i < enums->getNumOperands(); ++i ) {
            MDNode *n = cast< MDNode >( enums->getOperand(i) );
            MDString *name = cast< MDString >( n->getOperand(2) );
            if ( name->getString() == lookup )
                return cast< MDNode >( n->getOperand(10) ); // the list of enum items
        }
        return NULL;
    }

    explicit Interpreter(Allocator &a, Module *M);

    typedef std::pair< std::string, char * > Describe;
    typedef std::set< std::pair< Pointer, Type * > > DescribeSeen;

    Describe describeAggregate( Type *t, char *where, DescribeSeen& );
    Describe describeValue( Type *t, char *where, DescribeSeen& );
    std::string describePointer( Type *t, Pointer p, DescribeSeen& );
    std::string describeValue( const ::llvm::Value *, int thread,
                               DescribeSeen * = nullptr,
                               int *anonymous = nullptr,
                               std::vector< std::string > *container = nullptr );
    std::string describe( bool detailed = false );

    Blob initial( Function *f ); /* Make an initial state from Function. */
    void rewind( Blob b ) { state.rewind( b, 0 ); }
    void choose( int32_t i );

    void advance() {
        pc().instruction ++;
        if ( !instruction().op ) {
            PC to = pc();
            to.block ++;
            to.instruction = 0;
            evaluateSwitchBB( to );
        }
    }

    template< typename Yield >
    void run( Blob b, Yield yield ) {
        state.rewind( b, -1 ); int tid = 0;
        int threads = state._thread_count;
        while ( threads ) {
            run( tid, yield );
            if ( ++tid == threads )
                break;
            state.rewind( b, -1 );
        }
    }

    void evaluate();
    void evaluateSwitchBB( PC to );

    template< typename Yield >
    void run( int tid, Yield yield ) {
        std::set< PC > seen;

        if ( !state._thread_count )
            return; /* no more successors for you */

        assert_leq( tid, state._thread_count - 1 );
        if ( state._thread != tid )
            state.switch_thread( tid );
        assert( state.stack().get().length() );

        while ( true ) {
            /* do not execute the instruction yet! */
            if ( ( observable() && !pc().masked && !seen.empty() ) ||
                 seen.count( pc() ) ) {
                yield( state.snapshot() );
                return;
            }

            state.flags().assert = false;
            jumped = false;
            choice = 0;
            seen.insert( pc() );
            evaluate();

            if ( !state.stack().get().length() )
                break; /* this thread is done */

            if ( choice ) {
                assert( !jumped );
                Blob fork = state.snapshot();
                int limit = choice; /* make a copy, sublings must overwrite the original */
                for ( int i = 0; i < limit; ++i ) {
                    state.rewind( fork, tid );
                    choose( i );
                    advance();
                    run( tid, yield );
                }
                fork.free( alloc.pool() );
                return;
            }

            if ( !jumped )
                advance();
        }

        yield( state.snapshot() );
    }

    /* EvalContext interface. */
    char *dereference( ProgramInfo::Value v, int frame = 0 ) { return state.dereference( v, -1, frame ); }
    char *dereference( Pointer p ) { return state.dereference( p ); }
    Pointer malloc( int size ) { return state.nursery.malloc( size ); }

    /* ControlContext interface. */
    int stackDepth() { return state.stack().get().length(); }
    MachineState::Frame &frame( int depth = 0 ) { return state.frame( -1, depth ); }
    void leave() { state.leave(); }
    void enter( int fun ) { state.enter( fun ); }
    void new_thread( Function *f );
    void new_thread( PC pc );
    PC &pc() { return state._frame->pc; }
};

}
}

#endif
