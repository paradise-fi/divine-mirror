// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
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

#include <divine/smt/builder.hpp>
#include <runtime/abstract/formula.h>

namespace divine::smt
{

template< typename Builder >
struct Extract : Builder
{
    using Node = typename Builder::Node;

    template< typename... Args >
    Extract( vm::CowHeap &heap, Args && ... args )
        : Builder( std::forward< Args >( args )... ), _heap( heap )
    {}

    sym::Formula *read( vm::HeapPointer ptr )
    {
        return reinterpret_cast< sym::Formula * >( _heap.unsafe_bytes( ptr ).begin() );
    }

    Node build( vm::HeapPointer p );
    Node convert( vm::HeapPointer p )
    {
        auto it = _values.find( p );
        if ( it == _values.end() )
            it = _values.emplace( p, build( p ) ).first;
        return it->second;
    }

    vm::CowHeap &_heap;
    std::unordered_map< vm::HeapPointer, Node > _values;
};

}

namespace divine::smt::extract
{

using SMTLib2 = Extract< builder::SMTLib2 >;
#if OPT_Z3
using Z3      = Extract< builder::Z3 >;
#endif

}