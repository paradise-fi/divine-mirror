// -*- C++ -*- (c) 2011 Petr Rockai
#include <wibble/test.h>

#include <divine/dve/parse.h>
#include <divine/dve/symtab.h>
#include <divine/dve/expression.h>

#ifndef DIVINE_DVE_INTERPRETER_H
#define DIVINE_DVE_INTERPRETER_H

namespace divine {
namespace dve {

namespace parse {
typedef std::vector< Process > Procs;
typedef std::vector< Property > Props;
typedef std::vector< Declaration > Decls;
}

struct LValue {
    Symbol symbol;
    Expression idx;
    bool _valid;

    bool valid() { return _valid; }

    template< typename T >
    void set( EvalContext &ctx, T value, ErrorState &err ) {
        if ( symbol.item().is_array )
            return symbol.set( ctx.mem, idx.evaluate( ctx, &err ), value, err );
        return symbol.set( ctx.mem, 0, value, err );
    }

    LValue( const SymTab &tab, parse::LValue lv )
        : idx( tab, lv.idx ), _valid( lv.valid() )
    {
        symbol = tab.lookup( NS::Variable, lv.ident.name() );
        assert( symbol.valid() );
    }

    LValue() : _valid( false ) {}
};

struct LValueList {
    std::vector< LValue > lvals;
    bool _valid;
    
    bool valid() { return _valid; }
    
    template< typename T >
    void set( EvalContext &ctx, std::vector< T > values, ErrorState &err ) {
        auto lvals_it = lvals.begin();
        for( auto it = values.begin(); it != values.end(); it++, lvals_it++ ) {
            (*lvals_it).set( ctx, *it, err );
        }
    }
    
    LValueList( const SymTab &tab, parse::LValueList lvl )
        : _valid( lvl.valid() )
    {
        for( auto lval = lvl.lvlist.begin(); lval != lvl.lvlist.end(); lval++ ) {
            lvals.push_back( LValue(tab, *lval) );
        }
    }
    
    LValueList() : _valid( false ) {}
};

struct Channel {
    bool is_buffered, is_compound, is_array;
    int size, bufsize, width;
    std::string name;

    Symbol thischan;

    SymContext context;
    std::vector< Symbol > components;

    char * item(char * data, int i) {
        return data + i*context.offset;
    }

    int & count(char * data) {
        char *place = data + bufsize*context.offset;
        return *reinterpret_cast< int * >( place );
    }

    void enqueue( EvalContext &ctx, std::vector< int > values, ErrorState &err ) {
        char * data = thischan.getref( ctx.mem, 0 );
        char * _item = item( data, count( data ) );
        assert_eq( values.size(), components.size() );
        for ( unsigned i = 0; i < values.size(); i++ ) {
            /* Prevents segfault probably caused by copying this class to other
             * threads and not updating pointers properly.
             * Will look into it later
             */
            components[ i ].context = &context;
            components[ i ].set( _item, 0, values[ i ], err );
        }
        count( data )++;
    }

    std::vector< int > dequeue( EvalContext &ctx, ErrorState &err ) {
        char * data = thischan.getref( ctx.mem, 0 );
        std::vector< int > retval;
        retval.resize( components.size() );
        for ( unsigned i = 0; i < components.size(); i++ ) {
            components[ i ].context = &context; // Prevents segfault (See enqueue)
            retval[ i ] = components[ i ].deref( data, 0 );
        }
        memmove( data, item( data, 1 ), context.offset * ( --count( data ) ) );
        memset( item( data, count( data ) ), 0, context.offset );
        return retval;
    }

    bool full( EvalContext &ctx ) {
        char * data;
        if ( is_buffered )
            data = thischan.getref( ctx.mem, 0 );
        else
            return false;
        return count( data ) >= bufsize;
    };

    bool empty( EvalContext &ctx ) {
        char * data;
        if ( is_buffered )
            data = thischan.getref( ctx.mem, 0 );
        else
            return false;
        return count( data ) <= 0;
    };

    Channel( SymTab &sym, const parse::ChannelDeclaration &chandecl ) : is_compound( 0 ),  is_array( 0 ), size( 1 )
    {
        is_buffered = chandecl.is_buffered;
        bufsize = chandecl.size;
        name = chandecl.name;
        components.resize( chandecl.components.size() );
        for( unsigned i = 0; i < chandecl.components.size(); i++ ) {
            context.ids.push_back( SymContext::Item() );
            SymContext::Item &it = context.ids[ context.id ];
            it.offset = context.offset;
            it.width = chandecl.components[ i ];
            context.offset += it.width;
            components[ i ] =  Symbol( &context, context.id );
            context.id++;
        }
        width = bufsize * context.offset + sizeof( int );
        if ( is_buffered )
            thischan = sym.allocate( NS::Channel, *this );
    }

};

struct Transition {
    Symbol process;
    Symbol from, to;

    Channel *sync_channel;
    LValueList sync_lval;
    ExpressionList sync_expr;

    // Reader transition to this one (in case of atomic sync)
    Transition *sync;

    std::vector< Expression > guards;
    typedef std::vector< std::pair< LValue, Expression > > Effect;
    Effect effect;
    
    parse::Transition parse;

    bool from_commited, to_commited;
    Symbol flags;

    bool enabled( EvalContext &ctx, ErrorState &err ) {
        if ( process.deref( ctx.mem ) != from.deref( 0 ) )
            return false;
        for ( int i = 0; i < guards.size(); ++i )
            if ( !guards[i].evaluate( ctx, &err ) )
                return false;
        if ( sync_channel )
            if ( sync_expr.valid() && sync_channel->full( ctx ) )
                return false;
            if ( sync_lval.valid() && sync_channel->empty( ctx ) )
                return false;
        if ( sync ) {
            if ( from_commited && !sync->from_commited )
                return false;
            if ( !from_commited && sync->from_commited )
                return false;
            if ( !sync->enabled( ctx, err ) )
                return false;
        }
        return true;
    }

    void apply( EvalContext &ctx, ErrorState &err ) {
        if ( sync_channel && sync_channel->is_buffered ) {
                if( sync_expr.valid() )
                    sync_channel->enqueue( ctx, sync_expr.evaluate( ctx, &err ), err );
                else if ( sync_lval.valid() )
                    sync_lval.set( ctx, sync_channel->dequeue( ctx, err ), err );
                else
                    assert_die();
        }
        if ( sync ) {
            if (sync->sync_lval.valid() && sync_expr.valid() )
                    sync->sync_lval.set( ctx, sync_expr.evaluate( ctx, &err ), err );
            sync->apply( ctx, err );
        }
        for ( Effect::iterator i = effect.begin(); i != effect.end(); ++i )
            i->first.set( ctx, i->second.evaluate( ctx, &err ), err );
        process.set( ctx.mem, 0, to.deref(), err );
        StateFlags sflags;
        flags.deref( ctx.mem, 0, sflags );
        if ( from_commited )
            sflags.commited_dirty |= !to_commited;
        else
            sflags.commited |= to_commited;
        flags.set( ctx.mem, 0, sflags );
    }

    Transition( SymTab &sym, Symbol proc, parse::Transition t )
        : process( proc ), sync_channel( 0 ), sync( 0 ), parse( t )
    {
        for ( int i = 0; i < t.guards.size(); ++ i )
            guards.push_back( Expression( sym, t.guards[i] ) );
        for ( int i = 0; i < t.effects.size(); ++ i )
            effect.push_back( std::make_pair( LValue( sym, t.effects[i].lhs ),
                                              Expression( sym, t.effects[i].rhs ) ) );
        from = sym.lookup( NS::State, t.from );
        assert( from.valid() );
        to = sym.lookup( NS::State, t.to );
        assert( to.valid() );

        if ( t.syncexpr.valid() ) {
            sync_channel = sym.lookupChannel( t.syncexpr.chan );
            if ( t.syncexpr.write )
                sync_expr = ExpressionList( sym, t.syncexpr.exprlist );
            else if ( t.syncexpr.lvallist.valid() )
                sync_lval = LValueList( sym, t.syncexpr.lvallist );
        }

        flags = sym.lookup( NS::Flag, "Flags" );
    }
};

static inline void declare( SymTab &symtab, const parse::Decls &decls )
{
    for ( parse::Decls::const_iterator i = decls.begin(); i != decls.end(); ++i ) {
        symtab.allocate( NS::Variable, *i );
        std::vector< int > init;
        EvalContext ctx;
        for ( int j = 0; j < i->initial.size(); ++j ) {
            Expression ex( symtab, i->initial[ j ] );
            init.push_back( ex.evaluate( ctx ) );
        }
        while ( init.size() < i->size )
            init.push_back( 0 );
        symtab.constant( NS::Initialiser, i->name, init );
    }
}

struct Process {
    Symbol id;
    SymTab symtab;

    std::vector< std::vector< Transition > > trans;
    std::vector< std::vector< Transition > > state_readers;

    std::vector< Transition > readers;
    std::vector< Transition > writers;

    std::vector< bool > is_accepting, is_commited;

    std::vector< std::vector< Expression > > asserts;

    int state( EvalContext &ctx ) {
        return id.deref( ctx.mem );
    }

    bool commited( EvalContext &ctx ) {
        return is_commited[ state( ctx ) ];
    }

    bool assertValid( EvalContext &ctx ) {
        for ( auto expr = asserts[ state( ctx ) ].begin(); expr != asserts[ state( ctx ) ].end(); ++ expr )
            if ( !expr->evaluate( ctx ) )
                return false;
        return true;
    }

    int available( EvalContext &ctx ) {
        assert_leq( size_t( state( ctx ) + 1 ), trans.size() );
        return trans[ state( ctx ) ].size() > 0 ||
                state_readers[ state( ctx ) ].size() > 0;
    }

    int enabled( EvalContext &ctx, int i, ErrorState &err ) {
        ErrorState temp_err = ErrorState::e_none;
        assert_leq( size_t( state( ctx ) + 1 ), trans.size() );
        std::vector< Transition > &tr = trans[ state( ctx ) ];
        for ( ; i < tr.size(); ++i ) {
            if ( tr[ i ].enabled( ctx, temp_err ) )
                break;
            temp_err.error = ErrorState::e_none.error;
        }
        err.error |= temp_err.error;
        return i + 1;
    }

    bool valid( EvalContext &ctx, int i ) {
        return i <= trans[ state( ctx ) ].size();
    }

    Transition &transition( EvalContext &ctx, int i ) {
        assert_leq( size_t( state( ctx ) + 1 ), trans.size() );
        assert_leq( i, trans[ state( ctx ) ].size() );
        assert_leq( 1, i );
        return trans[ state( ctx ) ][ i - 1 ];
    }

    template< bool X >
    Process( SymTab *parent, Symbol id, const parse::Automaton< X > &proc )
        : id( id ), symtab( parent )
    {
        int states = 0;
        assert( id.valid() );

        declare( symtab, proc.decls );
        for ( std::vector< parse::Identifier >::const_iterator i = proc.states.begin();
              i != proc.states.end(); ++i ) {
            if ( proc.inits.size() && i->name() == proc.inits.front().name() )
                parent->constant( NS::InitState, proc.name.name(), states );
            symtab.constant( NS::State, i->name(), states++ );
        }

        assert_eq( states, proc.states.size() );

        is_accepting.resize( proc.states.size(), false );
        is_commited.resize( proc.states.size(), false );
        asserts.resize( proc.states.size() );
        for ( int i = 0; i < is_accepting.size(); ++ i ) {
            for ( int j = 0; j < proc.accepts.size(); ++ j )
                if ( proc.states[ i ].name() == proc.accepts[ j ].name() )
                    is_accepting[i] = true;
            for ( int j = 0; j < proc.commits.size(); ++ j )
                if ( proc.states[ i ].name() == proc.commits[ j ].name() )
                    is_commited[i] = true;
            for ( int j = 0; j < proc.asserts.size(); ++ j )
                if ( proc.states[ i ].name() == proc.asserts[ j ].state.name() )
                    asserts[ i ].push_back( Expression( symtab, proc.asserts[ j ].expr) );
        }

        trans.resize( proc.states.size() );
        state_readers.resize( proc.states.size() );

        for ( std::vector< parse::Transition >::const_iterator i = proc.trans.begin();
              i != proc.trans.end(); ++i ) {
            Symbol from = symtab.lookup( NS::State, i->from.name() );
            Transition t( symtab, id, *i );
            t.from_commited = is_commited[ t.from.deref( 0 ) ];
            t.to_commited = is_commited[ t.to.deref( 0 ) ];
            if ( i->syncexpr.valid() && !t.sync_channel->is_buffered ) {
                if ( i->syncexpr.write )
                    writers.push_back( t );
                else {
                    readers.push_back( t );
                    state_readers[ t.from.deref() ].push_back( t );
                }
            } else
                trans[ from.deref() ].push_back( t );
        }

    }

    void setupSyncs( std::vector< Transition > &readers )
    {
        for ( int w = 0; w < writers.size(); ++ w ) {
            Transition &tw = writers[w];
            for ( int r = 0; r < readers.size(); ++r ) {
                Transition &tr = readers[r];
                if ( tw.sync_channel == tr.sync_channel ) {
                    if ( tw.sync_expr.valid() != tr.sync_lval.valid() )
                        throw "Booh";
                    tw.sync = &tr;
                    trans[ tw.from.deref() ].push_back( tw );
                }
            }
        }
    }
};

struct System {
    SymTab symtab;
    std::vector< Process > processes;
    std::vector< Process > properties;
    std::vector< Channel > channels;
    Process *property;

    Symbol flags;

    struct Continuation {
        unsigned process:16; // the process whose turn it is
        unsigned property:16; // active property transition; 0 = none
        unsigned transition:32; // active process transition; 0 = none
        ErrorState err;
        Continuation() : process( 0 ), property( 0 ), transition( 0 ), err( ErrorState::e_none ) {}
        bool operator==( const Continuation &o ) const {
            return process == o.process && property == o.property && transition == o.transition;
        }
    };

    System( const parse::System &sys )
        : property( 0 )
    {
        assert( !sys.synchronous ); // XXX

        declare( symtab, sys.decls );
        flags = symtab.lookup( NS::Flag, "Flags" );

        // declare channels
        for( auto i = sys.chandecls.begin(); i != sys.chandecls.end(); i++ ) {
            channels.push_back( Channel( symtab, *i )  );
            symtab.channels[ i->name ] =  &channels.back();
        }

        // ensure validity of pointers into the process array
        processes.reserve( sys.processes.size() );
        properties.reserve( sys.properties.size() );

        for ( parse::Procs::const_iterator i = sys.processes.begin();
              i != sys.processes.end(); ++i )
        {
            Symbol id = symtab.allocate( NS::Process, i->name.name(), 4 );
            processes.push_back( Process( &symtab, id, *i ) );
            symtab.children[id] = &processes.back().symtab;
        }

        for ( parse::Props::const_iterator i = sys.properties.begin();
              i != sys.properties.end(); ++i )
        {
            Symbol id = symtab.allocate( NS::Process, i->name.name(), 4 );
            properties.push_back( Process( &symtab, id, *i ) );
            symtab.children[id] = &properties.back().symtab;
        }

        // compute synchronisations
        for ( int i = 0; i < processes.size(); ++ i ) {
            for ( int j = 0; j < processes.size(); ++ j ) {
                if ( i == j )
                    continue;
                processes[ i ].setupSyncs( processes[ j ].readers );
            }
        }

        // find the property process
        if ( sys.property.valid() ) {
            Symbol propid = symtab.lookup( NS::Process, sys.property );
            for ( int i = 0; i < processes.size(); ++ i ) {
                if ( processes[ i ].id == propid )
                    property = &processes[ i ];
            }

            assert( property );
        }
    }

    bool assertValid( EvalContext &ctx ) {
        for ( auto proc = processes.begin(); proc != processes.end(); ++ proc)
            if ( !proc->assertValid( ctx ) )
                return false;
        return true;
    }

    void setCommitedFlag( EvalContext &ctx ) {
        StateFlags sflags;
        flags.deref( ctx.mem, 0, sflags );
        sflags.commited_dirty = 0;
        sflags.commited = 0;
        for ( int i = 0; i < processes.size(); i++ ) {
            sflags.commited |= (bool)processes[i].commited( ctx );
        }
        flags.set( ctx.mem, 0, sflags );
    }

    bool processEnabled( EvalContext &ctx, Continuation &cont ) {
        Process &p = processes[ cont.process ];

        if ( &p == property ) // property process cannot progress alone
            return false;

        // try other property transitions first
        if ( cont.transition && property && property->valid( ctx, cont.property ) ) {
            cont.property = property->enabled( ctx, cont.property, cont.err );
            if ( property->valid( ctx, cont.property ) )
                return true;
        }

        cont.transition = p.enabled( ctx, cont.transition, cont.err );

        // is the result a valid transition?
        if ( p.valid( ctx, cont.transition ) ) {
            if ( !property )
                return true;
            cont.property = property->enabled( ctx, 0, cont.err );
            if ( property->valid( ctx, cont.property ) )
                return true;
        }

        // no more enabled transitions from this process
        cont.transition = 0;
        cont.err.error = ErrorState::e_none.error;

        return false;
    }

    Continuation enabledPrioritized( EvalContext &ctx, Continuation cont ) {
        if ( cont == Continuation() ) {
            bool commitEnable = false;
            for ( int i = 0; i < processes.size(); i++ ) {
                Process &pa = processes[ i ];
                if ( &pa == property )
                    continue;
                if ( pa.commited( ctx ) && pa.available( ctx ) ) {
                    commitEnable = true;
                    break;
                }
            }
            if ( !commitEnable ) {
                StateFlags sflags;
                flags.deref( ctx.mem, 0, sflags );
                sflags.commited = 0;
                flags.set( ctx.mem, 0, sflags );
                return enabledAll( ctx, cont );
            }
        }
        for ( ; cont.process < processes.size(); ++cont.process ) {
            Process &pb = processes[ cont.process ];
            if ( !pb.commited( ctx ) )
                continue;
            if ( processEnabled( ctx, cont ) )
                return cont;
        }
        return cont;
    }

    Continuation enabledAll( EvalContext &ctx, Continuation cont ) {
        for ( ; cont.process < processes.size(); ++cont.process ) {
            if ( processEnabled( ctx, cont ) )
                return cont;
        }
        return cont;
    }

    Continuation enabled( EvalContext &ctx, Continuation cont ) {
        bool system_deadlock = cont == Continuation();
        cont.err.error = ErrorState::e_none.error;
        StateFlags sflags;
        flags.deref( ctx.mem, 0, sflags );

        if ( sflags.commited ) {
            cont = enabledPrioritized( ctx, cont );
        }
        else {
            cont = enabledAll( ctx, cont );
        }

        system_deadlock = system_deadlock && cont.process >= processes.size();

        if ( system_deadlock && property )
            cont.property = property->enabled( ctx, cont.property, cont.err );

        return cont;
    }

    void initial( EvalContext &ctx ) {
        symtab.lookup( NS::Flag, "Flags" ).set(ctx.mem, 0, StateFlags::f_none);
        for ( int i = 0; i < processes.size(); ++i ) {
            initial( ctx, processes[i].symtab, NS::Variable, NS::Initialiser );
        }
        initial( ctx, symtab, NS::Variable, NS::Initialiser );
        initial( ctx, symtab, NS::Process, NS::InitState );
        symtab.lookup( NS::Flag, "Error" ).set( ctx.mem, 0, ErrorState::e_none );
        setCommitedFlag( ctx );
    }

    void initial( EvalContext &ctx, SymTab &tab, NS::Namespace vns, NS::Namespace ins ) {
        typedef std::map< std::string, SymId > Scope;
        Scope &scope = tab.tabs[ vns ];
        ErrorState err;

        for ( Scope::iterator i = scope.begin(); i != scope.end(); ++i ) {
            Symbol vsym = tab.lookup( vns, i->first ), isym = tab.lookup( ins, i->first );
            assert( vsym.valid() );
            if ( isym.valid() ) {
                if ( vsym.item().is_array )
                    for ( int index = 0; index < vsym.item().array; index++ )
                        vsym.set( ctx.mem, index, isym.deref( 0, index ), err );
                else
                    vsym.set( ctx.mem, 0, isym.deref(), err );
	    }
        }
    }

    void apply( EvalContext &ctx, Continuation c ) {
        if ( c.process < processes.size() )
            processes[ c.process ].transition( ctx, c.transition ).apply( ctx, c.err );
        if ( property )
            property->transition( ctx, c.property ).apply( ctx, c.err );
        if ( c.err.error )
            bail( ctx, c );
        StateFlags sflags;
        flags.deref( ctx.mem, 0, sflags );
        if ( sflags.commited_dirty )
            setCommitedFlag( ctx );

    }

    void bail( EvalContext &ctx, Continuation c ) {
        for( int i = 0; i < symtab.context->offset; i++ )
            ctx.mem[i] = 0;
        symtab.lookup( NS::Flag, "Error" ).set( ctx.mem, 0, c.err );
    }

    bool valid( EvalContext &ctx, Continuation c ) {
        ErrorState err;
        symtab.lookup( NS::Flag, "Error" ).deref( ctx.mem, 0, err );
        if ( err.error )
            return false;
        if ( c.process < processes.size() )
            return true;
        if ( property && property->valid( ctx, c.property ) )
            return true;
        return false;
    }

    bool accepting( EvalContext &ctx ) {
        if ( property )
            return property->is_accepting[ property->state( ctx ) ];
        return false;
    }
};

}
}

#endif
