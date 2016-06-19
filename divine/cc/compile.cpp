// -*- C++ -*- (c) 2016 Vladimír Štill

#include <divine/cc/compile.hpp>
#include <lart/divine/vaarg.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Support/raw_os_ostream.h>
#include <brick-llvm>
DIVINE_UNRELAX_WARNINGS

#include <brick-fs>
#include <brick-string>

extern const char *DIVINE_RUNTIME_SHA;

namespace divine {

struct stringtable { const char *n, *c; };
extern stringtable runtime_list[];

namespace cc {

std::string stringifyToCode( std::vector< std::string > ns, std::string name, std::string value ) {
    std::stringstream ss;
    for ( auto &n : ns )
        ss << "namespace " << n << "{" << std::endl;

    ss << "const char " << name << "[] = {" << std::hex << std::endl;
    int i = 0;
    for ( auto c : value ) {
        ss << "0x" << c << ", ";
        if ( ++i > 12 ) {
            ss << std::endl;
            i = 0;
        }
    }
    ss << "};";

    for ( auto &n : ns )
        ss << "} // namespace " << n << std::endl;
}

static std::string getWrappedMDS( llvm::NamedMDNode *meta, int i = 0, int j = 0 ) {
    auto *op = llvm::cast< llvm::MDTuple >( meta->getOperand( i ) );
    auto *str = llvm::cast< llvm::MDString >( op->getOperand( j ).get() );
    return str->getString().str();
}

template< typename ... T >
static std::string join( T &&... xs ) { return brick::fs::joinPath( std::forward< T >( xs )... ); }

Compile::Compile( Options opts ) :
    opts( opts ), compilers( 1 ), workers( 1 ), linker( new brick::llvm::Linker() )
{
    ASSERT_LEQ( 1ul, workers.size() );
    ASSERT_EQ( workers.size(), compilers.size() );

    commonFlags = { "-D__divine__"
                  , "-isystem", includeDir
                  , "-isystem", join( includeDir, "pdclib" )
                  , "-isystem", join( includeDir, "libm" )
                  , "-isystem", join( includeDir, "libcxx/include" )
                  , "-D_POSIX_C_SOURCE=2008098L"
                  , "-D_LITTLE_ENDIAN=1234"
                  , "-D_BYTE_ORDER=1234"
//                  , "-fno-slp-vectorize"
//                  , "-fno-vectorize"
                  };

    setupFS();
    if ( !opts.dont_link )
        setupLibs();
}

Compile::~Compile() { }

void Compile::compileAndLink( std::string path, std::vector< std::string > flags )
{
    linker->link( compile( path, flags ) );
}

std::unique_ptr< llvm::Module > Compile::compile( std::string path,
                                    std::vector< std::string > flags )
{
    std::vector< std::string > allFlags;
    std::copy( commonFlags.begin(), commonFlags.end(), std::back_inserter( allFlags ) );
    std::copy( flags.begin(), flags.end(), std::back_inserter( allFlags ) );

    std::cerr << "compiling " << path << std::endl;
    auto mod = mastercc().compileModule( path, allFlags );
    tagWithRuntimeVersionSha( *mod );

    lart::divine::VaArgInstr().run( *mod );
    return mod;
}

llvm::Module *Compile::getLinked() {
    tagWithRuntimeVersionSha( *linker->get() );
    return linker->get();
}

void Compile::writeToFile( std::string filename ) {
    writeToFile( filename, getLinked() );
}

void Compile::writeToFile( std::string filename, llvm::Module *mod )
{
    llvm::raw_os_ostream cerr( std::cerr );
    if ( llvm::verifyModule( *mod, &cerr ) ) {
        cerr.flush(); // otherwise nothing will be displayed
        UNREACHABLE( "invalid bitcode" );
    }
    std::error_code serr;
    ::llvm::raw_fd_ostream outs( filename, serr, ::llvm::sys::fs::F_None );
    WriteBitcodeToFile( mod, outs );
}

std::string Compile::serialize() {
    return mastercc().serializeModule( *getLinked() );
}

void Compile::addDirectory( std::string path ) {
    mastercc().allowIncludePath( path );
}

void Compile::addFlags( std::vector< std::string > flags ) {
    std::copy( flags.begin(), flags.end(), std::back_inserter( commonFlags ) );
}

void Compile::prune( std::vector< std::string > r ) {
    linker->prune( r, brick::llvm::Prune::UnusedModules );
}

void Compile::tagWithRuntimeVersionSha( llvm::Module &m ) const {
    auto *meta = m.getNamedMetadata( runtimeVersMeta );
    // check old metadata
    std::set< std::string > found;
    found.emplace( DIVINE_RUNTIME_SHA );
    if ( meta ) {
        for ( unsigned i = 0; i < meta->getNumOperands(); ++i )
            found.emplace( getWrappedMDS( meta, i ) );
        m.eraseNamedMetadata( meta );
    }
    meta = m.getOrInsertNamedMetadata( runtimeVersMeta );
    auto *tag = llvm::MDNode::get( m.getContext(),
                    llvm::MDString::get( m.getContext(),
                        found.size() == 1
                            ? DIVINE_RUNTIME_SHA
                            : "!mismatched version of divine cc and runtime!" ) );
    meta->addOperand( tag );
}

std::string Compile::getRuntimeVersionSha( llvm::Module &m ) const {
    auto *meta = m.getNamedMetadata( runtimeVersMeta );
    if ( !meta )
        return "";
    return getWrappedMDS( meta );
}

std::shared_ptr< llvm::LLVMContext > Compile::context() { return mastercc().context(); }

Compiler &Compile::mastercc() { return compilers[0]; }

static bool isSource( std::string x ) {
    using brick::string::endsWith;
    return endsWith( x, ".c") || endsWith( x, ".cpp" ) || endsWith( x, ".cc" );
}

template< typename Src >
void Compile::prepareSources( std::string basedir, Src src, Type type,
                              std::function< bool( std::string ) > filter )
{
    while ( src->n ) {
        if ( ( !filter || filter( src->n ) )
             && ( ( type == Type::Header && !isSource( src->n ) )
                || ( type == Type::Source && isSource( src->n ) )
                || type == Type::All ) )
        {
            auto path = join( basedir, src->n );
            mastercc().mapVirtualFile( path, src->c );
        }
        ++src;
    }
}

void Compile::setupFS() {
    prepareSources( includeDir, runtime_list, Type::Header );
    prepareSources( srcDir, runtime_list, Type::Source,
        [&]( std::string name ) { return !brick::string::startsWith( name, "filesystem" ); } );
}

void Compile::setupLibs() {
    if ( opts.precompiled.size() ) {
        auto input = std::move( llvm::MemoryBuffer::getFile( opts.precompiled ).get() );
        ASSERT( !!input );

        auto inputData = input->getMemBufferRef();
        auto parsed = parseBitcodeFile( inputData, *context() );
        if ( !parsed )
            throw std::runtime_error( "Error parsing input model; probably not a valid bitcode file." );
        if ( getRuntimeVersionSha( *parsed.get() ) != DIVINE_RUNTIME_SHA )
            std::cerr << "WARNING: runtime version of the precompiled library does not match the current runtime version"
                      << std::endl;
        linker->load( std::move( parsed.get() ) );
    } else {
        compileLibrary( join( srcDir, "pdclib" ), { "-D_PDCLIB_BUILD" } );
        compileLibrary( join( srcDir, "limb" ) );
        std::initializer_list< std::string > cxxflags = { "-std=c++14"
                                                        // , "-fstrict-aliasing"
                                                        , "-I", join( includeDir, "libcxxabi/include" )
                                                        , "-I", join( includeDir, "libcxxabi/src" )
                                                        , "-I", join( includeDir, "libcxx/src" )
                                                        , "-I", join( includeDir, "filesystem" )
                                                        , "-Oz" };
        compileLibrary( join( srcDir, "libcxxabi" ), cxxflags );
        compileLibrary( join( srcDir, "libcxx" ), cxxflags );
        compileLibrary( join( srcDir, "divine" ), cxxflags );
        compileLibrary( join( srcDir, "filesystem" ), cxxflags );
        compileLibrary( join( srcDir, "lart" ), cxxflags );
    }
}

void Compile::compileLibrary( std::string path, std::initializer_list< std::string > flags )
{
    for ( const auto &f : mastercc().filesMappedUnder( path ) )
        compileAndLink( f, flags );
}
}
}
