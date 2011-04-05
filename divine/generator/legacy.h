// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <divine/legacy/system/dve/dve_explicit_system.hh>
#include <divine/legacy/system/bymoc/bymoc_explicit_system.hh>
#include <divine/legacy/por/por.hh>

#include <sstream>
#include <stdexcept>

#include <divine/generator/common.h>

#ifndef DIVINE_GENERATOR_LEGACY_H
#define DIVINE_GENERATOR_LEGACY_H

namespace divine {
namespace generator {

template< typename _State, typename system_t >
struct LegacyCommon : Common< _State > {
    typedef _State State;
    typedef State Node;
    typedef generator::Common< _State > Common;
    typedef LegacyCommon< _State, system_t > Graph;

    std::string file;
    system_t *m_system;
    por_t *m_por;

    struct Successors {
        typedef Node Type;
        int current;
        succ_container_t m_succs;
        Node _from;
        LegacyCommon *parent;

        bool empty() const __attribute__((pure)) {
            if ( !_from.valid() )
                return true;
            if ( parent->legacy_system()->is_erroneous( parent->alloc.legacy_state( _from ) ) )
                return true;
            return current == m_succs.size();
        }

        Node from() { return _from; }

        State head() const {
            return parent->alloc.unlegacy_state( m_succs[ current ] );
        }

        Successors tail() const {
            Successors s = *this;
            ++ s.current;
            return s;
        }

        Successors() : current( 0 ) {}
    };

    Successors successors( State s ) {
        assert( s.valid() );
        Successors succ;
        succ.parent = this;
        succ._from = s;
        state_t legacy = this->alloc.legacy_state( s );
        legacy_system()->get_succs( legacy, succ.m_succs );
        return succ;
    }

    por_t &por() {
        if ( !m_por ) {
            m_por = new por_t;
            m_por->init( legacy_system() );
            m_por->set_choose_type( POR_SMALLEST ); // XXX
        }
        return *m_por;
    }

    Successors ample( State s ) {
        Successors succ;
        succ.parent = this;
        succ._from = s;
        state_t legacy = this->alloc.legacy_state( s );

        size_t proc_gid; // output parameter, to be discarded
        por().ample_set_succs( legacy, succ.m_succs, proc_gid );
        return succ;
    }

    State initial() {
        return this->alloc.unlegacy_state( legacy_system()->get_initial_state() );
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( State(), initial() );
    }

    void read( std::string path ) {
        file = path;
        legacy_system(); // force
    }

    void print_state( State s, std::ostream &o = std::cerr ) {
        legacy_system()->print_state( legacy_state( s ), o );
    }

    bool isAccepting( State s ) {
        return legacy_system()->is_accepting( this->alloc.legacy_state( s ) );
    }

    /// Is state s in acc_group set or accepting set of acc_group pair?
    bool isInAccepting( State s, const size_int_t acc_group ) {
        return legacy_system()->is_accepting( this->alloc.legacy_state( s ), acc_group, 1 );
    }

    /// Is state s in rejecting set of acc_group pair?
    bool isInRejecting( State s, const size_int_t acc_group ) {
        return legacy_system()->is_accepting( this->alloc.legacy_state( s ), acc_group, 2 );
    }

    /// Number of sets/pairs of acceptance condition
    unsigned acceptingGroupCount() {
        if ( !legacy_system()->get_with_property() ) return 0;
        process_t* propertyProcess = legacy_system()->get_property_process();
        return dynamic_cast< dve_process_t* >( propertyProcess )->get_accepting_group_count();
    }

    /// Type of acceptance condition
    PropertyType propertyType() {
        if ( legacy_system()->get_with_property() ) {
            process_t* propertyProcess = legacy_system()->get_property_process();
            return static_cast< PropertyType >( 
                dynamic_cast< dve_process_t* >( propertyProcess )->get_accepting_type() );
        }
        else
            return AC_None;
    }

    bool isGoal( State s ) { return false; } // XXX
    std::string showNode( State s ) {
        if ( !s.valid() )
            return "[]";
        std::stringstream o;
        legacy_system()->print_state( this->alloc.legacy_state( s ), o );
        return o.str();
    }

    explicit_system_t *legacy_system() {
        if ( !m_system ) {
            m_system = new system_t;
            m_system->setAllocator( &this->alloc );
            if ( !file.empty() ) {
                if ( legacy_system()->read( file.c_str() ) ) // don't ask.
                    throw std::runtime_error( "Error reading input model." );
            }
        }
        return m_system;
    }

    void release( State s ) {
        s.free( this->pool() );
    }

    LegacyCommon &operator=( const LegacyCommon &other ) {
        Common::operator=( other );
        file = other.file;
        safe_delete( m_system );
        safe_delete( m_por );
        legacy_system(); // FIXME, we force read here to keep
                         // dve_explicit_system::read() from happening in
                         // multiple threads at once, no matter the mutex...
        return *this;
    }

    LegacyCommon( const LegacyCommon &other )
        : Common( other ), file( other.file ), m_system( 0 ), m_por( 0 ) {}
    LegacyCommon() : m_system( 0 ), m_por( 0 ) {}

    ~LegacyCommon() {
        safe_delete( m_system );
        safe_delete( m_por );
    }
};

template< typename _State >
struct Dve : LegacyCommon< _State, dve_explicit_system_t >
{};

template< typename _State >
struct Bymoc : LegacyCommon< _State, bymoc_explicit_system_t >
{};

typedef Dve< Blob > NDve;
typedef Bymoc< Blob > NBymoc;

}
}

#endif
