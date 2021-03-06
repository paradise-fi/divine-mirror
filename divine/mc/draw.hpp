// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/mc/builder.hpp>
#include <divine/ss/search.hpp>
#include <divine/vm/memory.hpp>
#include <divine/dbg/dot.hpp>
#include <brick-proc>

namespace divine {
namespace mc {

namespace {

template< typename Builder >
std::string draw_impl( Builder &bld, std::shared_ptr< BitCode > bc, int distance, bool heap )
{
    dbg::Context< vm::CowHeap > dbg( bc->program(), bc->debug() );
    dbg.load( bld.pool(), bld.context() );
    vm::setup::boot( dbg );
    vm::Eval< decltype( dbg ) > dbg_eval( dbg );
    dbg_eval.run();
    dbg.snapshot();

    struct ext_data { int seq; int distance; };
    brick::mem::SlavePool< typename vm::CowHeap::SnapPool > ext_pool( bld.pool() );
    int seq = 0;

    auto ext = [&]( auto st ) -> auto& { return *ext_pool.machinePointer< ext_data >( st.snap ); };

    auto init =
        [&]( auto st )
        {
            ext_pool.materialise( st.snap, sizeof( ext_data ), false );
            if ( ext( st ).seq )
                return false;
            ext( st ).seq = ++ seq;
            ext( st ).distance = distance + 1;
            return true;
        };

    std::stringstream str;
    str << "digraph { node [ fontname = Courier ] edge [ fontname = Courier ]\n";

    bld.initials( [&]( auto st ) { init( st ); ext( st ).distance = 0; } );

    ss::search(
        ss::Order::PseudoBFS, bld, 1, ss::listen(
            [&]( auto f, auto t, auto l )
            {
                init( f );
                bool isnew = init( t );

                ext( t ).distance = std::min( ext( t ).distance, ext( f ).distance + 1 );
                std::string lbl, color;
                for ( auto txt : l.trace )
                    lbl += txt + "\n";
                if ( l.error )
                    color = "color=red";
                if ( l.accepting )
                    color = "color=blue";
                str << ext( f ).seq << " -> " << ext( t ).seq
                    << " [ label = \"" << text2dot( lbl ) << "\" " << color << "]"
                    << std::endl;
                if ( isnew && ext( t ).distance < distance )
                    return ss::Listen::Process;
                return ss::Listen::Ignore;
            },
            [&]( auto st )
            {
                init( st );
                dbg::Node< vm::Program, vm::CowHeap > dn( dbg, st.snap );
                dn._ref.get();
                dn.address( dbg::DNKind::Object, bld.context().state_ptr() );
                dn.type( dbg._state_type );
                dn.di_type( dbg._state_di_type );
                str << ext( st ).seq << " [ style=filled fillcolor=gray ]" << std::endl;
                if ( heap )
                {
                    str << ext( st ).seq << " -> " << ext( st ).seq << ".1 [ label=root ]" << std::endl;
                    str << dotDN( dn, false, std::to_string( ext( st ).seq ) + "." );
                }
                return ss::Listen::Process;
            } ) );
    str << "}";
    return str.str();
}

} // anonymous namespace

#if OPT_STP
using DrawBuilder = STPBuilder;
#else
using DrawBuilder = ExplicitBuilder;
#endif

template< typename Builder = DrawBuilder, typename Ctx = Context >
std::string draw( std::shared_ptr< BitCode > bc, int distance, bool heap,
                  Ctx *ctx = nullptr, typename Builder::Snapshot *initial = nullptr )
{
    Builder bld( bc );
    bld.context().enable_debug();
    if ( ctx && initial )
        bld.start( *ctx, *initial );
    else
        bld.start();
    return draw_impl( bld, bc, distance, heap );
}

}
}
