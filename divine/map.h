// -*- C++ -*- (c) 2007, 2008 Petr Rockai <me@mornfall.net>

#ifndef DIVINE_MAP_H
#define DIVINE_MAP_H

namespace divine {
namespace algorithm {

template< typename G >
struct Map : Algorithm, DomainWorker< Map< G > >
{
    typedef typename G::Node Node;

    struct Shared {
        int expanded, eliminated, accepting;
        int iteration;
        Blob cycle_node;
        G g;
    } shared;

    struct Extension {
        Blob parent;
        unsigned id:30;
        unsigned elim:2;
        unsigned map:30;
        unsigned oldmap:30;
        unsigned iteration;
    };

    Result m_result;

    Domain< Map< G > > &domain() {
        return DomainWorker< Map< G > >::domain();
    }

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    Node cycleNode() {
        for ( int i = 0; i < domain().peers(); ++i ) {
            if ( domain().shared( i ).cycle_node.valid() )
                return domain().shared( i ).cycle_node;
        }
        return Node();
    }

    visitor::TransitionAction updateIteration( Node t ) {
        int old = extension( t ).iteration;
        extension( t ).iteration = shared.iteration;
        return (old != shared.iteration) ?
            visitor::ExpandTransition :
            visitor::ForgetTransition;
    }


    visitor::ExpansionAction expansion( Node st )
    {
        ++ shared.expanded;
        if ( shared.g.isAccepting ( st ) ) {
            if ( !extension( st ).id ) { // first encounter
                extension( st ).id = reinterpret_cast< intptr_t >( st.ptr );
                ++ shared.accepting;
            }
            if ( extension( st ).id == extension( st ).map ) {
                // found accepting cycle
                shared.cycle_node = st;
                return visitor::TerminateOnState;
            }
        }
        return visitor::ExpandState;
    }

    std::pair< int, bool > map( Node f, Node st )
    {
        int map;
        bool elim = false;
        if ( extension( f ).oldmap != 0 && extension( st ).oldmap != 0 )
            if ( extension( f ).oldmap != extension( st ).oldmap )
                return std::make_pair( -1, false );

        if ( extension( f ).map < extension( f ).id ) {
            if ( extension( f ).elim == 2 )
                map = extension( f ).map;
            else {
                map = extension( f ).id;
                if ( extension( f ).elim != 1 )
                    elim = true;
            }
        } else
            map = extension( f ).map;

        return std::make_pair( map, elim );
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        if ( !f.valid() ) {
            assert( equal( t, shared.g.initial() ) );
            return updateIteration( t );
        }

        if ( extension( f ).id == 0 )
            assert( !shared.g.isAccepting( f ) );
        if ( shared.g.isAccepting( f ) )
            assert( extension( f ).id > 0 );

        if ( !extension( t ).parent.valid() )
            extension( t ).parent = f;

        std::pair< int, bool > a = map( f, t );
        int map = a.first;
        bool elim = a.second;

        if ( map == -1 )
            return updateIteration( t );

        if ( extension( t ).map < map ) {
            if ( elim ) {
                ++ shared.eliminated;
                extension( f ).elim = 1;
            }
            extension( t ).map = map;
            return visitor::ExpandTransition;
        }

        return updateIteration( t );
    }

    void _visit() {
        typedef visitor::Setup< G, Map< G >, Table > Setup;
        typedef visitor::Parallel< Setup, Map< G >, Hasher > Visitor;

        shared.expanded = 0;
        shared.eliminated = 0;

        Visitor visitor( shared.g, *this, *this, hasher, &table() );
        if ( visitor.owner( shared.g.initial() ) == this->globalId() )
            visitor.queue( Blob(), shared.g.initial() );
        visitor.visit();

        for ( size_t i = 0; i < table().size(); ++i ) {
            Node st = table()[ i ].key;
            if ( st.valid() ) {
                extension( st ).oldmap = extension( st ).map;
                extension( st ).map = 0;
                if ( extension( st ).elim == 1 )
                    extension( st ).elim = 2;
            }
        }
    }

    void visit() {
        domain().parallel().run( shared, &Map< G >::_visit );
    }

    Result run()
    {
        shared.iteration = 1;
        int acceptingCount = 0, eliminated = 0, d_eliminated = 0, expanded = 0;
        bool valid = true;
        do {
            std::cerr << " iteration " << shared.iteration
                      << "...\t\t" << std::flush;
            shared.accepting = shared.eliminated = shared.expanded = 0;
            visit();
            d_eliminated = 0;
            expanded = 0;
            for ( int i = 0; i < domain().peers(); ++ i ) {
                if ( shared.iteration == 1 )
                    acceptingCount += domain().shared( i ).accepting;
                d_eliminated += domain().shared( i ).eliminated;
                expanded += domain().shared( i ).expanded;
            }
            eliminated += d_eliminated;
            assert( eliminated <= acceptingCount );
            std::cerr << eliminated << " eliminated (";
            std::cerr << expanded << " expanded, ";
            std::cerr << acceptingCount << " accepting)" << std::endl;
            ++ shared.iteration;
            valid = !cycleNode().valid();
        } while ( d_eliminated > 0 && valid );

        m_result.ltlPropertyHolds = valid ? Result::Yes : Result::No;

        livenessBanner( valid );

        if ( !valid && want_ce ) {
            std::cerr << " generating counterexample..." << std::flush;
            // counterexample()
        }

        return m_result;
    }

    Map( Config *c = 0 )
        : Algorithm( c, sizeof( Extension ) )
    {
        initGraph( shared.g );
        if ( c ) {
            becomeMaster( &shared, workerCount( c ) );
        }
    }

};

}
}

#endif
