// -*- C++ -*- (c) 2008 Petr Rockai <me@mornfall.net>

#include <wibble/sys/thread.h>
#include <divine/fifo.h>
#include <divine/blob.h>
#include <divine/barrier.h>
#include <divine/mpi.h>

#ifndef DIVINE_PARALLEL_H
#define DIVINE_PARALLEL_H

namespace divine {

/*
  A simple structure that runs a method of a class in a separate thread.
 */
template< typename T >
struct RunThread : wibble::sys::Thread {
    typedef void (T::*F)();
    T *t;
    F f;

    virtual void init() {}
    virtual void fini() {}

    void *main() {
        init();
        (t->*f)();
        fini();
        return 0;
    }

    RunThread( T &_t, F _f ) : t( &_t ), f( _f )
    {
    }
};

template< typename T, template< typename > class R = RunThread >
struct Parallel {
    typedef typename T::Shared Shared;
    std::vector< T > m_instances;
    std::vector< R< T > > m_threads;
    std::vector< wibble::sys::Thread * > m_extra;

    int n;

    void addExtra( wibble::sys::Thread *t ) {
        m_extra.push_back( t );
    }

    T &instance( int i ) {
        assert( i < n );
        assert_eq( n, m_instances.size() );
        return m_instances[ i ];
    }

    Shared &shared( int i ) {
        return instance( i ).shared;
    }

    R< T > &thread( int i ) {
        assert( i < n );
        assert_eq( n, m_threads.size() );
        return m_threads[ i ];
    }

    template< typename Shared, typename F >
    void initThreads( Shared &sh, F f ) {
        m_threads.clear();
        for ( int i = 0; i < n; ++i ) {
            instance( i ).shared = sh;
            m_threads.push_back( R< T >( instance( i ), f ) );
        }
    }

    void runThreads() {
        for ( int i = 0; i < n; ++i )
            thread( i ).start();
        for ( int i = 0; i < m_extra.size(); ++i )
            m_extra[ i ]->start();
        for ( int i = 0; i < n; ++i )
            thread( i ).join();
        for ( int i = 0; i < m_extra.size(); ++i )
            m_extra[ i ]->join();
    }

    template< typename F >
    void run( Shared &sh, F f ) {
        initThreads( sh, f );
        runThreads();
    }

    Parallel( int _n ) : n( _n )
    {
        m_instances.resize( n );
    }
};

template< typename T >
struct BarrierThread : RunThread< T >, Terminable {
    Barrier< Terminable > *m_barrier;

    void setBarrier( Barrier< Terminable > &b ) {
        m_barrier = &b;
    }

    virtual void init() {
        assert( m_barrier );
        m_barrier->started( this );
    }

    virtual void fini() {
        // m_done is true if termination has been done, in which case all of
        // the mutexes are unlocked. (and unlocking an already unlocked mutex
        // locks it... d'OH)
        m_barrier->done( this );
    }

    bool workWaiting() {
        return this->t->workWaiting();
    }

    BarrierThread( T &_t, typename RunThread< T >::F _f )
        : RunThread< T >( _t, _f ), m_barrier( 0 )
    {
    }
};

template< typename > struct Domain;

template< typename T >
struct FifoVector
{
    int m_last;
    typedef divine::Fifo< T, NoopMutex > Fifo;
    std::vector< Fifo > m_vector;

    bool empty() {
        for ( int i = 0; i < m_vector.size(); ++i ) {
            Fifo &fifo = m_vector[ (m_last + i) % m_vector.size() ];
            if ( !fifo.empty() )
                return false;
        }
        return true;
    };

    T &next( bool wait = false ) {
        if ( wait )
            return m_vector[ m_last ].front( wait );

        while ( m_vector[ m_last ].empty() )
            m_last = (m_last + 1) % m_vector.size();
        return m_vector[ m_last ].front( wait );
    }

    Fifo &operator[]( int i ) {
        return m_vector[ i ];
    }

    void remove() {
        while ( m_vector[ m_last ].empty() )
            m_last = (m_last + 1) % m_vector.size();
        m_vector[ m_last ].pop();
    }

    void resize( int i ) { m_vector.resize( i, Fifo() ); }
    FifoVector() : m_last( 0 ) {}
};

template< typename T >
struct DomainWorker {
    typedef divine::Fifo< Blob, NoopMutex > Fifo;

    Domain< T > *m_master;
    FifoVector< Blob > fifo;
    int m_id;

    Domain< T > &master() {
        assert( m_master );
        return *m_master;
    }

    void connect( Domain< T > &master ) {
        m_master = &master;
        m_id = master.obtainId( *this );
        // FIXME this whole fifo allocation business is an ugly hack...
        fifo.resize( peers() + 1 );
    }

    int peers() {
        return master().n * master().mpi.size();
    }

    bool idle() {
        return master().barrier().idle( terminable() );
    }

    bool workWaiting() {
        return !this->fifo.empty();
    }

    int globalId() {
        assert( m_master );
        return m_id;
    }

    int localId() {
        return m_id - master().minId;
    }

    Terminable *terminable() {
        return &master().parallel().m_threads[ localId() ];
    }

    Fifo &queue( int from, int to ) {
        assert( from < peers() );
        return master().queue( from, to );
    }
};

template< typename T >
struct Domain {
    typedef divine::Fifo< Blob, NoopMutex > Fifo;

    struct Parallel : divine::Parallel< T, BarrierThread >
    {
        Domain< T > *m_domain;
        MpiThread< Domain< T > > mpiThread;

        template< typename Shared, typename F >
        void run( Shared &sh, F f ) {
            initThreads( sh, f );

            for ( int i = 0; i < this->n; ++i )
                this->thread( i ).setBarrier( m_domain->barrier() );

            m_domain->mpi.runOnSlaves( f );

            this->runThreads();
            m_domain->barrier().clear();
        }

        Parallel( Domain< T > &dom, int _n )
            : divine::Parallel< T, BarrierThread >( _n ),
              m_domain( &dom ), mpiThread( dom )
        {
            if ( dom.mpi.size() > 1 )
                addExtra( &mpiThread );
        }
    };


    Mpi< T, Domain< T > > mpi;
    Barrier< Terminable > m_barrier;

    int minId, maxId, lastId;
    std::map< DomainWorker< T >*, int > m_ids;
    Parallel *m_parallel;

    int n;

    Barrier< Terminable > &barrier() {
        return m_barrier;
    }

    Parallel &parallel() {
        if ( !m_parallel ) {
            m_parallel = new Parallel( *this, n );

            int count = n;
            if ( mpi.size() > 1 )
                ++ count;

            m_barrier.setExpect( count );

            for ( int i = 0; i < m_parallel->n; ++i ) {
                m_parallel->instance( i ).connect( *this );
            }
        }
        return *m_parallel;
    }

    int obtainId( DomainWorker< T > &t ) {
        if ( !lastId ) {
            minId = lastId = n * mpi.rank();
            maxId = (n * (mpi.rank() + 1)) - 1;
        }

        if ( !m_ids.count( &t ) )
            m_ids[ &t ] = lastId ++;

        return m_ids[ &t ];
    }

    bool isLocalId( int id ) {
        return id >= minId && id <= maxId;
    }

    int peers() {
        return n * mpi.size();
    }

    typename T::Shared &shared( int i ) {
        if ( isLocalId( i ) )
            return parallel().shared( i - minId );
        return mpi.shared( i );
    }

    Fifo &queue( int from, int to )
    {
        if ( isLocalId( to ) )
            return parallel().instance( to - minId ).fifo[ from + 1 ];
        else
            return parallel().mpiThread.fifo[ from + to * peers() ];
    }

    Domain( typename T::Shared *shared = 0, int _n = 4 )
        : mpi( shared, this ),
          lastId( 0 ),
          m_parallel( 0 ),
          n( _n )
    {}
};

}

#endif
