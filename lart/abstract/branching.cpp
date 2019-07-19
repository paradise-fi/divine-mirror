// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/branching.h>

#include <lart/support/util.h>
#include <lart/abstract/operation.h>

namespace lart::abstract {

    void LowerToBool::run( llvm::Module & m )
    {
        auto brs = query::query( m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::BranchInst > )
            .filter( query::notnull )
            .filter( [] ( auto br ) {
                if ( br->isConditional() )
                    return meta::abstract::has( br->getCondition() );
                return false;
            } ).freeze();

        auto tag = meta::tag::operation::index;
        auto index = m.getFunction( "lart.abstract.to_tristate" )->getMetadata( tag );

        OperationBuilder builder;
        for ( auto * br : brs ) {
            auto cond = llvm::cast< llvm::Instruction >( br->getCondition() );
            auto abs = cond->getNextNonDebugInstruction();
            auto op = builder.construct< Operation::Type::ToBool >( abs );
            op.inst->setMetadata( tag, index );
            br->setCondition( op.inst );
        };
    }

} // namespace lart::abstract

