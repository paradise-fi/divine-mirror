// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/toolkit/blob.h>
#include <divine/toolkit/pool.h>
#include <divine/toolkit/hashset.h>
#include <divine/toolkit/bitset.h>
#include <divine/toolkit/parallel.h>
#include <divine/toolkit/rpc.h>
#include <divine/utility/output.h>
#include <divine/utility/meta.h>
#include <divine/graph/visitor.h>
#include <wibble/sfinae.h>

#include <memory>

#ifndef DIVINE_ALGORITHM_H
#define DIVINE_ALGORITHM_H

namespace divine {
namespace algorithm {

struct Hasher {
    const int slack;
    uint32_t seed;
    bool allEqual;
    Pool& pool;

    Hasher( Pool& pool, int s = 0 ) : slack( s ), seed( 0 ), allEqual( false ),
        pool( pool ) {}
    Hasher( Hasher& other, int slack ) : slack( slack ), seed( other.seed ),
            allEqual( other.allEqual ), pool( other.pool )
    { }
    void setSeed( uint32_t s ) { seed = s; }

    inline hash_t hash( Blob b ) const {
        assert( b.valid() );
        return pool.hash( b, slack, pool.size( b ), seed );
    }

    inline bool equal( Blob a, Blob b ) const {
        assert( a.valid() );
        assert( b.valid() );
        return allEqual || ( pool.compare( a, b, slack, std::max( pool.size( a ), pool.size( b ) ) ) == 0 );
    }

    bool valid( Blob a ) const { return a.valid(); }
};

template< typename _Listener, typename _AlgorithmSetup >
struct Visit : _AlgorithmSetup, visitor::SetupBase {
    typedef _Listener Listener;
    typedef _AlgorithmSetup AlgorithmSetup;
    typedef typename AlgorithmSetup::Graph::Node Node;
    typedef typename AlgorithmSetup::Graph::Label Label;
    typedef typename AlgorithmSetup::Vertex Vertex;
    typedef typename AlgorithmSetup::VertexId VertexId;

    template< typename A, typename V >
    void queueInitials( A &a, V &v ) {
        a.graph().initials( [ &a, &v ] ( Node f, Node n, Label l ) {
                Vertex fV = f.valid()
                              ? a.store().fetch( f, a.store().hash( f ) )
                              : Vertex();
                assert( !f.valid() || a.store().valid( fV ) );
                v.queue( fV, n, l );
            } );
    }

    template< typename A, typename V >
    void queuePOR( A &a, V &v ) {
        a.graph().porExpand( [&v] ( Node f, Node n, Label l ) { v.queueAny( f, n, l ); } );
    }
};

struct Algorithm
{
    typedef divine::Meta Meta;

    Meta m_meta;
    const int m_slack;

    meta::Result &result() { return meta().result; }

    virtual void run() = 0;

    bool want_ce;

    Meta &meta() {
        return m_meta;
    }

    std::ostream &progress() {
        return Output::output().progress();
    }

    void resultBanner( bool valid ) {
        progress() << " ============================================= " << std::endl
                   << ( valid ?
                      "               The property HOLDS          " :
                      "           The property DOES NOT hold      " )
                   << std::endl
                   << " ============================================= " << std::endl;
    }

    /// Initializes the graph generator by reading a file
    template< typename Self >
    typename Self::Graph *initGraph( Self &self, Self *master = nullptr ) {
        typename Self::Graph *g = new typename Self::Graph;
        g->read( meta().input.model, master ? &master->graph() : nullptr );
        g->useProperty( meta().input.propertyName );
        meta().algorithm.reduce =
            g->useReductions( meta().algorithm.reduce );
        g->setDomainSize( meta().execution.thisNode,
                          meta().execution.nodes,
                          meta().execution.nodes * meta().execution.threads );
        g->fairnessEnabled( meta().algorithm.fairness );
        return g;
    }

    template< typename Self >
    typename Self::Store *initStore( Self &self, Self* master ) {
        self.graph().setSlack( m_slack );
        int slack = self.graph().alloc.slack();
        typename Self::Store *s =
            new typename Self::Store( self.graph(), slack,
                    master ? &master->store() : nullptr );
        s->hasher().setSeed( meta().algorithm.hashSeed );
        s->hasher().setSlack( self.graph().setSlack( m_slack ) );
        s->setSize( meta().execution.initialTable );
        s->id = &self;
        return s;
    }

    template< typename T >
    void ring( typename T::Shared (T::*fun)( typename T::Shared ) ) {
        T *self = static_cast< T * >( this );
        self->shared = self->topology().ring( self->shared, fun );
    }

    template< typename T >
    void parallel( void (T::*fun)() ) {
        T *self = static_cast< T * >( this );

        self->comms().enableSaving( self->meta().execution.diskFifo );

        self->topology().distribute( self->shared, &T::setShared );
        self->topology().parallel( fun );
        self->shareds.clear();
        self->topology().template collect( self->shareds, &T::getShared );
    }

    template< typename Self, typename Setup >
    void visit( Self *self, Setup setup, bool por = false ) {
        typename Setup::Visitor::template Implementation< Setup, Self >
            visitor( *self, *self, self->graph(), self->store(), self->data );

        if ( por )
            setup.queuePOR( *self, visitor );
        else
            setup.queueInitials( *self, visitor );
        visitor.processQueue();
    }

    Algorithm( Meta m, int slack = 0 )
        : m_meta( m ), m_slack( slack )
    {
        want_ce = meta().output.wantCe;
    }

    virtual ~Algorithm() {}
};

template< typename _Setup >
struct AlgorithmUtils {
    typedef _Setup Setup;
    typedef typename Setup::Store Store;
    typedef typename Setup::Graph Graph;

    std::shared_ptr< Store > m_store;
    std::shared_ptr< Graph > m_graph;

    typename Setup::Visitor::template Data< Setup > data;

    template< typename Self >
    void init( Self *self, Self *master = nullptr ) {
        m_graph = std::shared_ptr< Graph >( self->initGraph( *self, master ) );
        if ( master )
            data = master->data;
        m_store = std::shared_ptr< Store >( self->initStore( *self, master ) );
    }

    Graph &graph() {
        assert( m_graph );
        return *m_graph;
    }

    Pool& pool() {
        return graph().base().alloc.pool();
    }

    Store &store() {
        assert( m_store );
        return *m_store;
    }
};

}
}

#define ALGORITHM_CLASS(_setup, _shared)                        \
    RPC_CLASS;                                                  \
    typedef _shared Shared;                                     \
    Shared shared;                                              \
    std::vector< Shared > shareds;                              \
    void setShared( Shared sh ) { shared = sh; }                \
    Shared getShared() { return shared; }                       \
    typedef typename _setup::Graph Graph;                       \
    typedef typename _setup::Statistics Statistics;             \
    typedef typename Graph::Node Node;                          \
    typedef typename Graph::Label Label;                        \
    typedef typename _setup::Store Store

#define ALGORITHM_RPC_ID(_type, _id, _fun) \
    template< typename Setup > RPC_ID( _type< Setup >, _fun, 2 + _id )
#define ALGORITHM_RPC(alg) \
    ALGORITHM_RPC_ID(alg, -1, getShared);       \
    ALGORITHM_RPC_ID(alg, 0, setShared)

#endif
