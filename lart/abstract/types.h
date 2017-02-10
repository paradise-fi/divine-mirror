// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/lib/IR/LLVMContextImpl.h>
#include <llvm/IR/TypeBuilder.h>

#include <llvm/Support/raw_ostream.h>
DIVINE_UNRELAX_WARNINGS

#include <string>
#include <sstream>
#include <iostream>

#ifndef LART_ABSTRACTION_TYPES_H
#define LART_ABSTRACTION_TYPES_H

namespace lart {
namespace abstract {
namespace types {

namespace {

std::string str( llvm::Type * type ) {
    std::string type_str;
    llvm::raw_string_ostream rso( type_str );
    type->print( rso );
    return rso.str();
}

static llvm::Type * voidType( llvm::LLVMContext & ctx ) {
   return llvm::Type::getVoidTy( ctx );
}

} // empty namespace

struct Base {
    static const std::string name() { return "lart"; }
};

struct IntegerType : Base {
    static std::string name( const llvm::Type * type ) {
        assert( type->isIntegerTy() );
        return IntegerType::name( llvm::cast< llvm::IntegerType >( type )->getBitWidth() );
    }

    static std::string name( unsigned bw ) {
        return Base::name() + ".abstract.i" + std::to_string( bw );
    }

    static llvm::StructType * get( llvm::Type * type ) {
        assert( type->isIntegerTy() );
        auto ity = llvm::cast< llvm::IntegerType >( type );
        return IntegerType::get(  ity->getContext(), ity->getBitWidth() );
    }

    static llvm::StructType * get( llvm::LLVMContext & ctx, unsigned bw ) {
        if( auto lookup = ctx.pImpl->NamedStructTypes.lookup( name( bw ) ) )
            return lookup;
        return llvm::StructType::create( ctx,
              { llvm::IntegerType::get( ctx, bw ) }, IntegerType::name( bw ) );
    }

    static unsigned bw( const llvm::StructType * type ) {
        assert( type->getElementType( 0 )->isIntegerTy() );
        return llvm::cast< llvm::IntegerType >( type->getElementType( 0 ) )->getBitWidth();
    }

    static bool isa( const llvm::Type * type ) {
        if ( ! type->isStructTy() )
            return false;
        auto sty = llvm::cast< llvm::StructType >( type );
        if ( ! sty->getElementType( 0 )->isIntegerTy() )
            return false;
        return IntegerType::get( type->getContext(), IntegerType::bw( sty ) ) == type;
    }

    static bool isa( const llvm::Type * type, unsigned bw ) {
        return isa( type ) && IntegerType::get( type->getContext(), bw ) == type;
    }

    static llvm::Type * lower( const llvm::Type * type ) {
        assert( IntegerType::isa( type ) );
        auto st = llvm::cast< llvm::StructType >( type );
        return llvm::Type::getIntNTy( type->getContext(), bw( st ) );
    }
};

struct Tristate : Base {
    static std::string name() {
        return Base::name() + ".tristate";
    }

    static llvm::StructType * get( llvm::LLVMContext & ctx ) {
        if( auto lookup = ctx.pImpl->NamedStructTypes.lookup( name() ) )
            return lookup;
        return llvm::StructType::create( { body( ctx ) }, Tristate::name() );
    }

    static bool isa( const llvm::Type * type ) {
        return Tristate::get( type->getContext() ) == type;
    }

    static llvm::Constant * True() {
        auto & ctx = llvm::getGlobalContext();
        return llvm::ConstantStruct::get( get( ctx ), { llvm::ConstantInt::getTrue( ctx ) } );
    }

    static llvm::Constant * False() {
        auto & ctx = llvm::getGlobalContext();
        return llvm::ConstantStruct::get( get( ctx ), { llvm::ConstantInt::getFalse( ctx ) } );
    }

    static llvm::Type * lower( const llvm::Type * type ) {
        assert( Tristate::isa( type ) );
        return llvm::Type::getInt1Ty( type->getContext() );
    }

private:
    static llvm::IntegerType * body( llvm::LLVMContext & ctx ) {
        return llvm::IntegerType::get( ctx, 1 );
    }
};

static inline llvm::Type * stripPtr( llvm::Type * t ) {
    return t->isPointerTy() ? t->getPointerElementType() : t;
}

namespace {
static std::vector< std::string > parseTypeName( llvm::StructType * type ) {
    std::stringstream ss;
    ss.str( type->getName().str() );
    std::vector< std::string > parts;
    std::string part;
    while( std::getline( ss, part, '.' ) )
        parts.push_back( part );
    return parts;
}

} // empty namespace

static std::string name( const llvm::Type * type ) {
    if ( auto s = llvm::dyn_cast< llvm::StructType >( type ) ) {
        assert( s->getNumElements() == 1 );
        type = s->getElementType( 0 );
        assert( llvm::isa< llvm::IntegerType >( type ) );
    }

    std::string buffer;
    llvm::raw_string_ostream rso( buffer );
    type->print( rso );
    return rso.str();
}

static bool isAbstract( llvm::Type * type ) {
    type = stripPtr( type );
    if ( auto structTy = llvm::dyn_cast< llvm::StructType >( type ) ) {
        if ( structTy->hasName() ) {
            auto name = structTy->getStructName();
            if ( name.empty() )
                return false;
            return std::string( name.str(), 0, 5 ) == "lart.";
        }
    }
    return false;
}


static llvm::Type * lift( llvm::Type * type ) {
    if ( type == voidType( type->getContext() ) )
        return voidType( type->getContext() );
    if ( isAbstract( type ) )
        return type;
    bool isptr = type->isPointerTy();
    type = stripPtr( type );

    llvm::Type * ret;
    auto & ctx = type->getContext();
    if ( type == llvm::Type::getInt1Ty( ctx ) ) {
        ret = Tristate::get( ctx );
    } else if ( type->isIntegerTy() ) {
        ret =  IntegerType::get( type );
    } else if ( type->isStructTy() ) {
        auto s = llvm::cast< llvm::StructType >( type );
        // union type
        assert( s->getNumElements() == 1 );
        return lift( s->getElementType( 0 ) );
    } else {
        std::cerr << "Lifting unsupported type:";
        type->dump();
        std::exit( EXIT_FAILURE );
    }

    return isptr ? ret->getPointerTo() : ret;
}

static llvm::Type * lower( llvm::Type * type ) {
    if ( type == voidType( type->getContext() ) )
        return voidType( type->getContext() );
    if ( !isAbstract( type ) )
        return type;
    bool isptr = type->isPointerTy();
    type = stripPtr( type );
    llvm::Type * ret;
    if ( Tristate::isa( type ) ) {
        ret = Tristate::lower( type );
    } else if ( IntegerType::isa( type ) ) {
        ret = IntegerType::lower( type );
    } else {
        std::cerr << "Lowering unsupported type:";
        type->dump();
        std::exit( EXIT_FAILURE );
    }

    return isptr ? ret->getPointerTo() : ret;
}

// type format: lart.<domain>.<lower type>
static std::string domain( llvm::Type * type ) {
    auto structT = llvm::cast< llvm::StructType >( stripPtr( type ) );
    auto parts = parseTypeName( structT );
    assert( parts.size() >= 2 );
    return parts[ 1 ];
}

static std::string lowerTypeName( llvm::Type * type ) {
    return name( lower( type ) );
}

} // namespace types
} // namespace abstract
} // namespace lart

#endif // LART_ABSTRACTION_TYPES_H
