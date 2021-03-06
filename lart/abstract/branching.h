// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

namespace lart::abstract {

    struct LowerToBool
    {
        void run( llvm::Module & m );
    };

} // namespace lart::abstract

