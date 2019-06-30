// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Jan Horáček <me@apophis.cz>
 * (c) 2017-2019 Zuzana Baranová <xbaranov@fi.muni.cz>
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

#include <divine/cc/cc1.hpp> //CompileError
#include <divine/cc/native.hpp>
#include <divine/rt/dios-cc.hpp>
#include <divine/rt/runtime.hpp>
#include <divine/ui/version.hpp>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/ToolChain.h>

using namespace divine;

/* Fetched in form of options {"-isystem", "PATH", etc.} */
template< typename IOVector >
void get_cpp_header_paths( IOVector out )
{
        using clang::driver::Compilation;
        cc::ClangDriver drv;

        std::unique_ptr< Compilation > c( drv.BuildCompilation( { "divcc" } ) );
        /* TODO: "error: no input files" -- BuildCompilation takes args */
        //if ( drv.diag.engine.hasErrorOccurred() )
        //    throw cc::CompileError( "failed to get cpp header paths" );

        llvm::opt::InputArgList drv_args;
        llvm::opt::ArgStringList cc1_args;

        c->getDefaultToolChain().AddClangCXXStdlibIncludeArgs( drv_args, cc1_args );
        c->getDefaultToolChain().AddClangSystemIncludeArgs( drv_args, cc1_args );

        for( auto arg : cc1_args )
            *out++ = arg;
}

/* usage: same as gcc */
int main( int argc, char **argv )
{
    try {
        cc::Native nativeCC( { argv + 1, argv + argc } );
        nativeCC.set_cxx( brick::string::endsWith( argv[0], "divc++" ) );
        auto& po = nativeCC._po;

        if( nativeCC._cxx )
        {
            po.opts.push_back( "-D_GNU_SOURCE=1" );
            get_cpp_header_paths( std::back_inserter( po.cc1_args ) );
        }

        if ( po.hasHelp || po.hasVersion )
        {
            nativeCC.print_info( ui::version() );
            return 0;
        }

        return nativeCC.run();

    } catch ( cc::CompileError &err ) {
        std::cerr << err.what() << std::endl;
        return 1;
    } catch ( std::runtime_error &err ) {
        std::cerr << "compilation failed: " << err.what() << std::endl;
        return 2;
    }
}
