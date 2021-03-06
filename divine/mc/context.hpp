// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016-2019 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once
#include <divine/smt/extract.hpp>

namespace divine::mc
{
    using Snapshot = vm::CowHeap::Snapshot;

    struct Context : vm::Context< vm::Program, vm::CowHeap >
    {
        using Super = vm::Context< vm::Program, vm::CowHeap >;
        using MemMap = Super::MemMap;
        struct Critical { MemMap loads, stores; };

        std::vector< std::string > _trace;
        std::string _info;
        std::vector< vm::Choice > _stack;
        std::deque< vm::Choice > _lock;
        std::vector< vm::HeapPointer > _assume;
        vm::GenericPointer _tid;
        MemMap _mem_loads, _mem_stores, _crit_loads, _crit_stores;
        std::unordered_map< vm::GenericPointer, Critical > _critical;
        int _level;

        Context( vm::Program &p ) : _level( 0 ) { this->program( p ); }

        template< typename I >
        int choose( int count, I, I )
        {
            ASSERT( !this->debug_mode() );
            if ( _lock.size() )
            {
                auto rv = _lock.front();
                ASSERT_EQ( count, rv.total );
                _lock.pop_front();
                _stack.push_back( rv );
                return rv.taken;
            }

            ASSERT_LEQ( _level, int( _stack.size() ) );
            _level ++;
            if ( _level < int( _stack.size() ) )
                return _stack[ _level - 1 ].taken;
            if ( _level == int( _stack.size() ) )
                return ++ _stack[ _level - 1 ].taken;
            _stack.emplace_back( 0, count );
            return 0;
        }

        void clear() override
        {
            Super::clear();
            _mem_loads.clear();
            _mem_stores.clear();
            _crit_loads.clear();
            _crit_stores.clear();
        }

        using Super::trace;

        void trace( vm::TraceSchedInfo ) {} /* noop */
        void trace( vm::TraceSchedChoice ) {} /* noop */
        void trace( vm::TraceStateType ) {}
        void trace( vm::TraceTypeAlias ) {}

        void trace( std::string s ) { _trace.push_back( s ); }
        void trace( vm::TraceDebugPersist t ) { Super::trace( t ); }
        void trace( vm::TraceAssume ta )
        {
            _assume.push_back( ta.ptr );
            if ( debug_allowed() )
                trace( "ASSUME " + smt::extract::to_string( heap(), ta.ptr ) );
        }

        bool test_crit( vm::CodePointer pc, vm::GenericPointer ptr, int size, int type )
        {
            if ( this->flags_all( _VM_CF_IgnoreCrit ) || this->debug_mode() )
                return false;

            auto start = ptr, end = start;
            end.offset( start.offset() + size );

            if ( type == _VM_MAT_Load || type == _VM_MAT_Both )
            {
                if ( _crit_loads.intersect( start, end ) )
                    return track_test( vm::Interrupt::Mem, pc );
                else if ( _track_mem )
                    _mem_loads.insert( start, end );
            }

            if ( type == _VM_MAT_Store || type == _VM_MAT_Both )
            {
                if ( _crit_stores.intersect( start, end ) )
                    return track_test( vm::Interrupt::Mem, pc );
                else if ( _track_mem )
                    _mem_stores.insert( start, end );
            }

            return false;
        }

        void swap_critical()
        {
            if ( !this->_track_mem ) return;
            ASSERT( !_tid.null() );
            swap( _mem_loads, _critical[ _tid ].loads );
            swap( _mem_stores, _critical[ _tid ].stores );
        }

        void trace( vm::TraceTaskID tid )
        {
            ASSERT( _tid.null() );
            ASSERT( _mem_loads.empty() );
            ASSERT( _mem_stores.empty() );
            _tid = tid.ptr;
            swap_critical();
        }

        void trace( vm::TraceInfo ti )
        {
            _info += heap().read_string( ti.text ) + "\n";
        }

        bool finished()
        {
            if ( !_tid.null() )
                swap_critical();
            _stack.resize( _level, vm::Choice( 0, -1 ) );
            _level = 0;
            _tid = vm::GenericPointer();
            _trace.clear();
            _assume.clear();
            while ( !_stack.empty() && _stack.back().taken + 1 == _stack.back().total )
                _stack.pop_back();
            return _stack.empty();
        }
    };

}
