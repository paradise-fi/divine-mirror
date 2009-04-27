// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/config.h>
#include <divine/blob.h>
#include <divine/hashmap.h>

#ifndef DIVINE_ALGORITHM_H
#define DIVINE_ALGORITHM_H

namespace divine {
namespace algorithm {

template< typename T >
struct _MpiId
{
    static int to_id( void (T::*f)() ) {
        // assert( 0 );
        return -1;
    }

    static void (T::*from_id( int ))() {
        // assert( 0 );
        return 0;
    }

    template< typename O >
    static void writeShared( typename T::Shared, O ) {
    }

    template< typename I >
    static I readShared( typename T::Shared &, I i ) {
        return i;
    }
};

inline int workerCount( Config *c ) {
    if ( !c )
        return 1;
    return c->workers();
}

struct Hasher {
    int slack;

    Hasher( int s = 0 ) : slack( s ) {}
    void setSlack( int s ) { slack = s; }

    inline hash_t operator()( Blob b ) const {
        assert( b.valid() );
        return b.hash( slack, b.size() );
    }
};

struct Equal {
    int slack;
    Equal( int s = 0 ) : slack( s ) {}
    void setSlack( int s ) { slack = s; }
    inline hash_t operator()( Blob a, Blob b ) const {
        assert( a.valid() );
        assert( b.valid() );
        return a.compare( b, slack, std::max( a.size(), b.size() ) ) == 0;
    }
};

struct Algorithm
{
    typedef Blob Node; // Umm.

    typedef HashMap< Node, Unit, Hasher,
                     divine::valid< Node >, Equal > Table;

    Config *m_config;
    int m_slack;
    Hasher hasher;
    Equal equal;
    Table *m_table;

    bool want_ce;

    Table &table() {
        if ( !m_table )
            m_table = new Table( hasher, divine::valid< Node >(), equal );
        return *m_table;
    }

    void livenessBanner( bool valid ) {
        std::cerr << " ===================================== " << std::endl
                  << ( valid ?
                     "       Accepting cycle NOT found       " :
                     "         Accepting cycle FOUND         " )
                  << std::endl
                  << " ===================================== " << std::endl;
    }

    void safetyBanner( bool valid ) {
        std::cerr << " ===================================== " << std::endl
                  << ( valid ?
                     "          Goal state NOT found         " :
                     "            Goal state FOUND           " )
                  << std::endl
                  << " ===================================== " << std::endl;
    }

    template< typename G >
    void initGraph( G &g ) {
        g.setSlack( m_slack );
        if ( m_config ) { // this is the master instance
            g.read( m_config->input() );
        }
    }

    Algorithm( Config *c = 0, int slack = 0 )
        : m_config( c ), m_slack( slack ), m_table( 0 )
    {
        hasher.setSlack( slack );
        equal.setSlack( slack );
        if ( c ) {
            want_ce = c->generateCounterexample();
        }
    }
};

}
}

#endif
