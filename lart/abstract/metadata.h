// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/domains/domains.h>

#include <iostream>

namespace lart {
namespace abstract {

struct MDBuilder {
    MDBuilder( llvm::LLVMContext &ctx )
        : ctx( ctx )
    {}

    llvm::MDNode* domain_node( Domain dom );
private:
    llvm::LLVMContext &ctx;
};


struct CreateAbstractMetadata {
    void run( llvm::Module &m );
};


struct MDValue {
    MDValue( llvm::Metadata * md )
        : _md( llvm::cast< llvm::ValueAsMetadata >( md ) )
    {}

    MDValue( llvm::Value * v )
        : _md( llvm::LocalAsMetadata::get( v ) )
    {}

    std::string name() const;
    llvm::Value * value() const;
    std::vector< Domain > domains() const;
    Domain domain() const; // requires that value has single domain
private:
    llvm::ValueAsMetadata  *_md;
};


std::vector< MDValue > abstract_metadata( llvm::Module &m );
std::vector< MDValue > abstract_metadata( llvm::Function *fn );

void dump_abstract_metadata( llvm::Module &m );

} // namespace abstract
} // namespace lart
