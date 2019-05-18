// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
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

#include <lart/support/annotate.h>

namespace lart {

    void LowerAnnotations::run( llvm::Module &m ) const noexcept
    {
        brick::llvm::enumerateFunctionsForAnno( _anno, m, [&] ( auto * fn ) { lower( fn ); } );
    }

    void LowerAnnotations::lower( llvm::Function * fn ) const noexcept
    {
        fn->addFnAttr( _anno );
    }

    void AnnotateFunctions::run( llvm::Module &m )
    {
        for ( auto & fn : m )
            if ( match( fn ) )
                annotate( fn );
    }

    bool AnnotateFunctions::match( const llvm::Function & fn ) const noexcept
    {
        auto name = fn.getName();
        return std::regex_match( name.begin(), name.end(), _rgx );
    }

    void AnnotateFunctions::annotate( llvm::Function & fn ) const noexcept
    {
        fn.addFnAttr( _annotation.name() );
    }

} // namespace lart
