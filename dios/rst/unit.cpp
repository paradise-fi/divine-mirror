// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <rst/unit.hpp>

#include <rst/common.hpp>

#include <cstdint>

namespace __dios::rst::abstract {

    // unit base maintains single global base id for all unit values
    unit_t unit_base_id;

    __attribute__((constructor, optnone)) void __lart_unit_init()
    {
        unit_base_id = unit_t::lift_any();
    }

    template< typename C >
    _LART_INLINE C make_unit() noexcept
    {
        return make_abstract< C, unit_t >();
    }

    template< typename C >
    _LART_INLINE C make_unit( C c ) noexcept
    {
        return make_abstract< C, unit_t >( c );
    }

    extern "C" {
        _LART_SCALAR uint8_t __unit_val_i8() { return make_unit< uint8_t >(); }
        _LART_SCALAR uint16_t __unit_val_i16() { return make_unit< uint16_t >(); }
        _LART_SCALAR uint32_t __unit_val_i32() { return make_unit< uint32_t >(); }
        _LART_SCALAR uint64_t __unit_val_i64() { return make_unit< uint64_t >(); }

        _LART_AGGREGATE void * __unit_aggregate() { return make_unit< void * >(); }

        _LART_SCALAR float __unit_val_float32() { return make_unit< float >(); }
        _LART_SCALAR double __unit_val_float64() { return make_unit< double >(); }

        _LART_SCALAR uint8_t __unit_lift_i8( uint8_t c ) { return make_unit( c ); }
        _LART_SCALAR uint16_t __unit_lift_i16( uint16_t c ) { return make_unit( c ); }
        _LART_SCALAR uint32_t __unit_lift_i32( uint32_t c ) { return make_unit( c ); }
        _LART_SCALAR uint64_t __unit_lift_i64( uint64_t c ) { return make_unit( c ); }
        _LART_SCALAR float __unit_lift_float32( float c ) { return make_unit( c ); }
        _LART_SCALAR double __unit_lift_float64( double c ) { return make_unit( c ); }
    }

} // namespace __dios::rst::abstract
