// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2011-2018 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/mem-heap.hpp>
#include <divine/vm/mem-heap.tpp>

namespace divine::t_vm
{

    template< typename H >
    struct Heap
    {
        using IntV = vm::value::Int< 32, true >;
        using PointerV = vm::value::Pointer;

        H heap;
        PointerV p;

        Heap() { p = heap.make( 16 ); }

        TEST(alloc)
        {
            IntV q;
            heap.write( p.cooked(), IntV( 10 ) );
            heap.read( p.cooked(), q );
            ASSERT_EQ( q.cooked(), 10 );
        }

        TEST(conversion)
        {
            ASSERT_EQ( vm::HeapPointer( p.cooked() ),
                       vm::HeapPointer( vm::GenericPointer( p.cooked() ) ) );
        }

        TEST(write_read)
        {
            PointerV q;
            heap.write( p.cooked(), p );
            heap.read( p.cooked(), q );
            ASSERT( p.cooked() == q.cooked() );
        }

        TEST(write_undef)
        {
            IntV i( 0, 0xFF, false ), j;
            heap.write( p.cooked(), i );
            heap.read( p.cooked(), j );
            ASSERT_EQ( i, j );
            ASSERT_EQ( j.defbits(), 0xFF );
        }

        TEST(write_semidef)
        {
            IntV i( 0, 0xF0, false ), j;
            heap.write( p.cooked(), i );
            heap.read( p.cooked(), j );
            ASSERT_EQ( i, j );
            ASSERT_EQ( j.defbits(), 0xF0 );
        }

        TEST(write_shift_undef)
        {
            IntV i( 0, 0xFF, false ), j;
            auto q = p;
            heap.write_shift( p, i );
            heap.read( q.cooked(), j );
            ASSERT_EQ( i, j );
            ASSERT_EQ( j.defbits(), 0xFF );
        }

        TEST(clone_semidef)
        {
            IntV i( 0, 0xF0, false ), j;
            heap.write( p.cooked(), i );
            auto q = vm::mem::heap::clone( heap, heap, p.cooked() );
            heap.read( q, j );
            ASSERT_EQ( i, j );
            ASSERT_EQ( j.defbits(), 0xF0 );
        }

        TEST(clone_interheap_semidef)
        {
            IntV i( 0, 0xF0, false ), j;
            heap.write( p.cooked(), i );
            decltype( heap ) heap2;
            auto q = vm::mem::heap::clone( heap, heap2, p.cooked() );
            heap2.read( q, j );
            ASSERT_EQ( i, j );
            ASSERT_EQ( j.defbits(), 0xF0 );
        }

        TEST(clone_intertype_semidef)
        {
            IntV i( 0, 0xF0, false ), j;
            heap.write( p.cooked(), i );
            vm::mem::MutableHeap heap2;
            auto q = vm::mem::heap::clone( heap, heap2, p.cooked() );
            heap2.read( q, j );
            ASSERT_EQ( i, j );
            ASSERT_EQ( j.defbits(), 0xF0 );
        }

        TEST(clone_cow_semidef)
        {
            IntV i( 0, 0xF0, false ), j;
            heap.write( p.cooked(), i );
            vm::mem::CowHeap heap2;
            auto q = vm::mem::heap::clone( heap, heap2, p.cooked() );
            heap2.read( q, j );
            ASSERT_EQ( i, j );
            ASSERT_EQ( j.defbits(), 0xF0 );
        }

        TEST(resize)
        {
            PointerV p, q;
            p = heap.make( 16 );
            heap.write( p.cooked(), p );
            heap.resize( p.cooked(), 8 );
            heap.read( p.cooked(), q );
            ASSERT_EQ( p.cooked(), q.cooked() );
            ASSERT_EQ( heap.size( p.cooked() ), 8 );
        }

        TEST(pointers)
        {
            auto p = heap.make( 16 ), q = heap.make( 16 ), r = PointerV( vm::nullPointer() );
            heap.write( p.cooked(), q );
            heap.write( q.cooked(), r );
            int count = 0;
            for ( auto pos : heap.pointers( p.cooked() ) )
                static_cast< void >( pos ), ++ count;
            ASSERT_EQ( count, 1 );
        }

        TEST(clone_int)
        {
            decltype( heap ) cloned;
            auto p = heap.make( 16 );
            heap.write( p.cooked(), IntV( 33 ) );
            auto c = vm::mem::heap::clone( heap, cloned, p.cooked() );
            IntV i;
            cloned.read( c, i );
            ASSERT( ( i == IntV( 33 ) ).cooked() );
            ASSERT( ( i == IntV( 33 ) ).defined() );
        }

        TEST(clone_ptr_chain)
        {
            decltype( heap ) cloned;
            auto p = heap.make( 16 ), q = heap.make( 16 ), r = PointerV( vm::nullPointer() );
            heap.write( p.cooked(), q );
            heap.write( q.cooked(), r );

            auto c_p = vm::mem::heap::clone( heap, cloned, p.cooked() );
            PointerV c_q, c_r;
            cloned.read( c_p, c_q );
            ASSERT( c_q.pointer() );
            cloned.read( c_q.cooked(), c_r );
            ASSERT( c_r.cooked().null() );
        }

        TEST(clone_ptr_loop)
        {
            decltype( heap ) cloned;
            auto p = heap.make( 16 ), q = heap.make( 16 );
            heap.write( p.cooked(), q );
            heap.write( q.cooked(), p );
            auto c_p1 = vm::mem::heap::clone( heap, cloned, p.cooked() );
            PointerV c_q, c_p2;
            cloned.read( c_p1, c_q );
            ASSERT( c_q.pointer() );
            cloned.read( c_q.cooked(), c_p2 );
            ASSERT( vm::GenericPointer( c_p1 ) == c_p2.cooked() );
        }

        TEST(compare)
        {
            decltype( heap ) cloned;
            auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
            heap.write( p, PointerV( q ) );
            heap.write( q, PointerV( p ) );
            auto c_p = vm::mem::heap::clone( heap, cloned, p );
            ASSERT_EQ( vm::mem::heap::compare( heap, cloned, p, c_p ), 0 );
            p.offset( 8 );
            heap.write( p, vm::value::Int< 32 >( 1 ) );
            ASSERT_LT( 0, vm::mem::heap::compare( heap, cloned, p, c_p ) );
        }

        TEST(hash)
        {
            decltype( heap ) cloned;
            auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
            heap.write( p, PointerV( q ) );
            heap.write( p + vm::PointerBytes, PointerV( p ) );
            heap.write( q, PointerV( p ) );
            auto c_p = vm::mem::heap::clone( heap, cloned, p );
            ASSERT_EQ( vm::mem::heap::hash( heap, p ).first,
                    vm::mem::heap::hash( cloned, c_p ).first );
        }
    };

    struct Mutable : vm::mem::SmallHeap {};
    struct Cow : vm::mem::CowHeap {};

    template struct Heap< Mutable >;
    template struct Heap< Cow >;

    struct CowHeap
    {
        using IntV = vm::value::Int< 32, true >;
        using PointerV = vm::value::Pointer;

        vm::mem::CowHeap heap;
        PointerV p;
        CowHeap() { p = heap.make( 16 ); }

        TEST(basic)
        {
            PointerV check;

            auto p = heap.make( 16 ).cooked();
            heap.write( p, PointerV( p ) );
            auto snap = heap.snapshot();

            heap.read( p, check );
            ASSERT_EQ( check.cooked(), p );

            heap.write( p, PointerV( vm::nullPointer() ) );
            heap.restore( snap );

            check = PointerV( vm::nullPointer() );
            heap.read( p, check );
            ASSERT_EQ( check.cooked(), p );
        }

        TEST(copy)
        {
            vm::mem::CowHeap heap;
            auto copy = heap;
            auto p = heap.make( 16 ).cooked();
            copy.restore( heap.snapshot() );
            ASSERT( copy.valid( p ) );
        }

        TEST(write_undef)
        {
            IntV i( 0, 0xFF, false ), j;
            heap.write( p.cooked(), i );
            auto s = heap.snapshot();
            heap.read( p.cooked(), j );
            ASSERT_EQ( i, j );
            ASSERT_EQ( j.defbits(), 0xFF );

            heap.write( p.cooked(), IntV( 0 ) );
            heap.restore( s );
            heap.read( p.cooked(), j );
            ASSERT_EQ( i, j );
            ASSERT_EQ( j.defbits(), 0xFF );
        }

        TEST(write_semidef)
        {
            IntV i( 0, 0xF0, false ), j;
            heap.write( p.cooked(), i );
            auto s = heap.snapshot();
            heap.read( p.cooked(), j );
            ASSERT_EQ( i, j );
            ASSERT_EQ( j.defbits(), 0xF0 );

            heap.write( p.cooked(), IntV( 0 ) );
            heap.restore( s );
            heap.read( p.cooked(), j );
            ASSERT_EQ( i, j );
            ASSERT_EQ( j.defbits(), 0xF0 );
        }

        TEST(hash)
        {
            auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
            heap.write( p, PointerV( q ) );
            heap.write( p + vm::PointerBytes, IntV( 5 ) );
            heap.write( q, PointerV( p ) );
            heap.snapshot();
            ASSERT( vm::mem::heap::hash( heap, p ).first !=
                    vm::mem::heap::hash( heap, q ).first );
        }

        TEST(copy_content)
        {
            auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
            heap.write( p, PointerV( q ) );
            heap.write( p + vm::PointerBytes, IntV( 5 ) );
            heap.write( q, PointerV( p ) );
            decltype( heap ) h2( heap );
            PointerV val;
            h2.read( p, val );
            ASSERT_EQ( val, PointerV( q ) );
            h2.read( q, val );
            ASSERT_EQ( val, PointerV( p ) );
        }

        TEST(copy_isolation)
        {
            auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
            heap.write( p, PointerV( q ) );
            heap.write( p + vm::PointerBytes, IntV( 5 ) );
            heap.write( q, PointerV( p ) );
            decltype( heap ) h2( heap );

            heap.write( p, IntV( 7 ) );
            h2.write( p, PointerV( p ) );

            IntV iv; PointerV pv;
            heap.read( p, iv );
            h2.read( p, pv );
            ASSERT_EQ( iv, IntV( 7 ) );
            ASSERT_EQ( pv.cooked(), p );
        }

        TEST(copy_isolation_snap)
        {
            auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
            heap.write( p, PointerV( q ) );
            heap.write( p + vm::PointerBytes, IntV( 5 ) );
            heap.write( q, PointerV( p ) );
            decltype( heap ) h2( heap );

            heap.write( p, IntV( 7 ) );
            h2.write( p, PointerV( p ) );
            h2.snapshot();

            IntV iv; PointerV pv;
            heap.read( p, iv );
            h2.read( p, pv );
            ASSERT_EQ( iv, IntV( 7 ) );
            ASSERT_EQ( pv.cooked(), p );
        }

        TEST(snap_restore)
        {
            auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
            auto s1 = heap.snapshot();
            heap.write( p, PointerV( q ) );
            heap.write( p + vm::PointerBytes, IntV( 5 ) );
            heap.write( q, PointerV( p ) );
            auto s2 = heap.snapshot();
            heap.write( p, IntV( 7 ) );
            auto s3 = heap.snapshot();

            IntV iv; PointerV pv;

            heap.restore( s1 );
            heap.read( p, iv );
            ASSERT_EQ( iv.defbits(), 0 );
            heap.restore( s2 );
            heap.read( p, pv );
            ASSERT_EQ( pv.cooked(), q );
            heap.restore( s3 );
            heap.read( p, iv );
            ASSERT( iv.defined() );
            ASSERT_EQ( iv.cooked(), 7 );
        }

        TEST(snap_restore_isolation)
        {
            auto p = heap.make( 16 ).cooked(), q = heap.make( 16 ).cooked();
            auto s1 = heap.snapshot();
            heap.write( p, PointerV( q ) );
            heap.write( p + vm::PointerBytes, IntV( 5 ) );
            heap.write( q, PointerV( p ) );
            auto s2 = heap.snapshot();
            heap.write( p, IntV( 7 ) );
            auto s3 = heap.snapshot();

            IntV iv; PointerV pv;

            heap.restore( s1 );
            heap.write( p, IntV( 8 ) );
            heap.restore( s1 );
            heap.read( p, iv );
            ASSERT_EQ( iv.defbits(), 0 );
            heap.restore( s2 );
            heap.write( p, IntV( 8 ) );
            heap.restore( s2 );
            heap.read( p, pv );
            ASSERT_EQ( pv.cooked(), q );
            heap.restore( s3 );
            heap.write( p, IntV( 8 ) );
            heap.restore( s3 );
            heap.read( p, iv );
            ASSERT( iv.defined() );
            ASSERT_EQ( iv.cooked(), 7 );

            heap.write( p, IntV( 8 ) );
            heap.restore( s1 );
            heap.read( p, iv );
            ASSERT_EQ( iv.defbits(), 0 );
        }
    };

}
