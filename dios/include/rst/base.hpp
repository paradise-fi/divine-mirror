// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/common.hpp>

#include <cstdint>

namespace __dios::rst::abstract {

    using BaseID = uint8_t;

    // Each abstract domain needs to inherit from Base
    // to maintain domain index
    struct tagged_abstract_domain_t
    {
        // Placeholder constructor for LART to initialize domain index
        _LART_NOINLINE tagged_abstract_domain_t( BaseID id = 0 ) noexcept
            : _id( id )
        {}

        BaseID _id;
    };

} // namespace __dios::rst::abstract
