// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>

#include <divine/ltlce.h>
#include <divine/visitor.h>
#include <divine/parallel.h>

#ifndef DIVINE_ALGORITHM_REACHABILITY_H
#define DIVINE_ALGORITHM_REACHABILITY_H

namespace divine {
namespace algorithm {

template< typename, typename > struct Reachability;

// MPI function-to-number-and-back-again drudgery... To be automated.
template< typename G, typename S >
struct _MpiId< Reachability< G, S > >
{
    typedef Reachability< G, S > A;

    static int to_id( void (A::*f)() ) {
        if( f == &A::_visit ) return 0;
        if( f == &A::_parentTrace ) return 1;
        if( f == &A::_por) return 7;
        if( f == &A::_por_worker) return 8;
        assert_die();
    }

    static void (A::*from_id( int n ))()
    {
        switch ( n ) {
            case 0: return &A::_visit;
            case 1: return &A::_parentTrace;
            case 7: return &A::_por;
            case 8: return &A::_por_worker;
            default: assert_die();
        }
    }

    template< typename O >
    static void writeShared( typename A::Shared s, O o ) {
        o = s.stats.write( o );
        *o++ = s.goal.valid();
        if ( s.goal.valid() )
            o = s.goal.write32( o );
        o = s.ce.write( o );
    }

    template< typename I >
    static I readShared( typename A::Shared &s, I i ) {
        i = s.stats.read( i );
        bool valid = *i++;
        if ( valid ) {
            FakePool fp;
            i = s.goal.read32( &fp, i );
        }
        i = s.ce.read( i );
        return i;
    }
};
// END MPI drudgery

/**
 * A simple parallel reachability analysis implementation. Nothing to worry
 * about here.
 */
template< typename G, typename Statistics >
struct Reachability : virtual Algorithm, AlgorithmUtils< G >, DomainWorker< Reachability< G, Statistics > >
{
    typedef Reachability< G, Statistics > This;
    typedef typename G::Node Node;

    struct Shared {
        Node goal;
        bool deadlocked;
        algorithm::Statistics< G > stats;
        G g;
        CeShared< Node > ce;
        bool need_expand;
    } shared;

    Node goal;
    bool deadlocked;

    Domain< This > &domain() {
        return DomainWorker< This >::domain();
    }

    struct Extension {
        Blob parent;
    };

    LtlCE< G, Shared, Extension > ce;

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    visitor::ExpansionAction expansion( Node st )
    {
        shared.stats.addNode( shared.g, st );
        shared.g.porExpansion( st );
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        if ( !meta().algorithm.hashCompaction && !extension( t ).parent.valid() ) {
            extension( t ).parent = f;
            visitor::setPermanent( f );
        }
        shared.stats.addEdge();

        if ( meta().algorithm.findGoals && shared.g.isGoal( t ) ) {
            shared.goal = t;
            shared.deadlocked = false;
            return visitor::TerminateOnTransition;
        }

        shared.g.porTransition( f, t, 0 );
        return visitor::FollowTransition;
    }

    struct VisitorSetup : visitor::Setup< G, This, typename AlgorithmUtils< G >::Table, Statistics > {
        static visitor::DeadlockAction deadlocked( This &r, Node n ) {
            if ( !r.meta().algorithm.findDeadlocks )
                return visitor::IgnoreDeadlock;

            r.shared.goal = n;
            r.shared.stats.addDeadlock();
            r.shared.deadlocked = true;
            return visitor::TerminateOnDeadlock;
        }
    };

    void _visit() { // parallel
        this->comms().notify( this->globalId(), &shared.g.pool() );
        if ( meta().algorithm.hashCompaction )
            equal.allEqual = true;
        visitor::Partitioned< VisitorSetup, This, Hasher >
            visitor( shared.g, *this, *this, hasher, &this->table(), meta().algorithm.hashCompaction );
        shared.g.queueInitials( visitor );
        visitor.processQueue();
    }

    void _por_worker() {
        shared.g._porEliminate( *this, hasher, this->table() );
    }

    void _por() {
        if ( shared.g.porEliminate( domain(), *this ) )
            shared.need_expand = true;
    }

    Reachability( Meta m, bool master = false )
        : Algorithm( m, sizeof( Extension ) )
    {
        if ( master )
            this->becomeMaster( &shared, m.execution.threads );
        this->init( &shared.g, this );
    }

    void _parentTrace() {
        ce.setup( shared.g, shared ); // XXX this will be done many times needlessly
        ce._parentTrace( *this, hasher, equal, this->table() );
    }

    void counterexample( Node n ) {
        shared.ce.initial = n;
        ce.setup( shared.g, shared );
        ce.linear( domain(), *this );
        ce.goal( *this, n );
    }

    void collect() {
        deadlocked = false;
        for ( int i = 0; i < domain().peers(); ++i ) {
            Shared &s = domain().shared( i );
            shared.stats.merge( s.stats );
            if ( s.goal.valid() ) {
                goal = s.goal;
                if ( s.deadlocked )
                    deadlocked = true;
            }
        }
    }

    void run() {
        progress() << "  searching... \t" << std::flush;

        domain().parallel( meta() ).run( shared, &This::_visit );
        collect();

        while ( !goal.valid() ) {
            shared.need_expand = false;
            domain().parallel( meta() ).runInRing( shared, &This::_por );

            if ( shared.need_expand ) {
                domain().parallel( meta() ).run( shared, &This::_visit );
                collect();
            } else
                break;
        }

        progress() << shared.stats.states << " states, "
                   << shared.stats.transitions << " edges" << std::flush;

        if ( goal.valid() ) {
            if ( deadlocked )
                progress() << ", DEADLOCK";
            else
                progress() << ", GOAL";
        }
        progress() << std::endl;

        safetyBanner( !goal.valid() );
        if ( goal.valid() && !meta().algorithm.hashCompaction ) {
            counterexample( goal );
            result().ceType = deadlocked ? meta::Result::Deadlock : meta::Result::Goal;
        }

        meta().input.propertyType = meta::Input::Reachability;
        result().propertyHolds = goal.valid() ? meta::Result::No : meta::Result::Yes;
        result().fullyExplored = goal.valid() ? meta::Result::No : meta::Result::Yes;
        shared.stats.update( meta().statistics );
    }
};

}
}

#endif
