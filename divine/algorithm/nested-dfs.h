// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#include <divine/algorithm/common.h>
#include <divine/algorithm/metrics.h>
#include <divine/porcp.h>
#include <divine/visitor.h>
#include <wibble/sfinae.h> // Unit

#ifndef DIVINE_ALGORITHM_NDFS_H
#define DIVINE_ALGORITHM_NDFS_H

namespace divine {
namespace algorithm {

template< typename G, typename Statistics >
struct NestedDFS : virtual Algorithm, AlgorithmUtils< G >
{
    typedef NestedDFS< G, Statistics > This;
    typedef typename G::Node Node;
    typedef typename AlgorithmUtils< G >::Table Table;

    G g;
    Node seed;
    bool valid;
    bool parallel, finished;

    std::deque< Node > ce_stack;
    std::deque< Node > ce_lasso;
    std::deque< Node > toexpand;

    algorithm::Statistics< G > stats;

    struct Extension {
        bool nested:1;
        bool on_stack:1;
    };

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    void runInner( G &graph, Node n ) {
        seed = n;
        visitor::DFV< InnerVisit > visitor( graph, *this, &this->table() );
        visitor.exploreFrom( n );
    }

    struct : wibble::sys::Thread {
        Fifo< Node > process;
        NestedDFS< G, Statistics > *outer;
        G graph;

        void *main() {
            while ( outer->valid ) {
                if ( !process.empty() ) {
                    Node n = process.front();
                    process.pop();
                    outer->runInner( graph, n ); // run the inner loop
                } else {
                    if ( outer->finished )
                        return 0;
#ifdef POSIX // uh oh...
                    sched_yield();
#endif
                }
            }
            return 0;
        }
    } inner;

    void counterexample() {
        progress() << "generating counterexample... " << std::flush;
        LtlCE< G, wibble::Unit, wibble::Unit > ce;
        ce.generateLinear( *this, g, ce_stack );
        ce.generateLasso( *this, g, ce_lasso );
        progress() << "done" << std::endl;
        result().ceType = meta::Result::Cycle;
    }

    // this is the entrypoint for full expansion... I know the name isn't best,
    // but that's what PORGraph uses
    void queue( Node from, Node to ) {
        visitor::DFV< OuterVisit > visitor( g, *this, &this->table() );
        visitor.exploreFrom( to );
    }

    void run() {
        progress() << " searching...\t\t\t" << std::flush;

        if ( parallel ) {
            inner.graph = g;
            inner.start();
        }

        visitor::DFV< OuterVisit > visitor( g, *this, &this->table() );
        visitor.exploreFrom( g.initial() );

        while ( valid && !toexpand.empty() ) {
            if ( !g.full( toexpand.front() ) )
                g.fullexpand( *this, toexpand.front() );
            toexpand.pop_front();
        }

        finished = true;
        if ( parallel )
            inner.join();

        progress() << "done" << std::endl;
        livenessBanner( valid );

        if ( !valid ) {
            if ( parallel )
                std::cerr << "Sorry, but counterexamples are not implemented in Parallel Nested DFS."
                          << std::endl
                          << "Please re-run with -w 1 to obtain a counterexample." << std::endl;
            else
                counterexample();
        }

        stats.update( meta().statistics );
        result().propertyHolds = valid ? meta::Result::Yes : meta::Result::No;
        result().fullyExplored = valid ? meta::Result::Yes : meta::Result::No;
    }

    visitor::ExpansionAction expansion( Node st ) {
        if ( !valid )
            return visitor::TerminateOnState;
        stats.addNode( g, st );
        ce_stack.push_front( st );
        extension( st ).on_stack = true;
        return visitor::ExpandState;
    }

    visitor::ExpansionAction innerExpansion( Node st ) {
        if ( !valid )
            return visitor::TerminateOnState;
        stats.addExpansion();
        ce_lasso.push_front( st );
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node from, Node to ) {
        stats.addEdge();
        if ( from.valid() && !g.full( from ) && !g.full( to ) && extension( to ).on_stack )
            toexpand.push_back( from );
        return visitor::FollowTransition;
    }

    visitor::TransitionAction innerTransition( Node from, Node to )
    {
        // The search always starts with a transition from "nowhere" into the
        // initial state. Ignore this transition here.
        if ( from.valid() && to.pointer() == seed.pointer() ) {
            valid = false;
            return visitor::TerminateOnTransition;
        }

        if ( !extension( to ).nested ) {
            extension( to ).nested = true;
            return visitor::ExpandTransition;
        }

        return visitor::FollowTransition;
    }

    struct OuterVisit : visitor::Setup< G, This, Table, Statistics > {
        static void finished( This &dfs, Node n ) {

            if ( dfs.g.isAccepting( n ) ) { // run the nested search
                if ( dfs.parallel )
                    dfs.inner.process.push( n );
                else
                    dfs.runInner( dfs.g, n );
            }

            if ( !dfs.ce_stack.empty() ) {
                assert_eq( n.pointer(), dfs.ce_stack.front().pointer() );
                dfs.ce_stack.pop_front();
            }
        }
    };

    struct InnerVisit : visitor::Setup< G, This, Table, Statistics,
                                        &This::innerTransition,
                                        &This::innerExpansion >
    {
        static void finished( This &dfs, Node n ) {
            if ( !dfs.ce_lasso.empty() ) {
                assert_eq( n.pointer(), dfs.ce_lasso.front().pointer() );
                dfs.ce_lasso.pop_front();
            }
        }
    };

    NestedDFS( Meta m, bool = false )
        : Algorithm( m, sizeof( Extension ) )
    {
        valid = true;
        this->init( &g, NULL );
        parallel = m.execution.threads > 1;
        if (m.execution.threads > 2)
            progress() << "WARNING: Nested DFS uses only 2 threads." << std::endl;
        if ( parallel ) {
            progress() << "WARNING: Parallel Nested DFS uses a fixed-size hash table." << std::endl;
            progress() << "Using table size " << m.execution.initialTable
                       << ", please use -i to override." << std::endl;
            this->table().m_maxsize = m.execution.initialTable;
        }
        finished = false;
        inner.outer = this;
    }

};

}
}

#endif
