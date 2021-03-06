// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Analysis/CallGraph.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/support/query.h>

#include <stack>
#include <unordered_set>
#include <algorithm>
#include <functional>

#ifndef LART_ANALYSIS_POSTORDERDEPS_H
#define LART_ANALYSIS_POSTORDERDEPS_H

namespace lart {
namespace analysis {

template< typename Nodes, typename Succs >
Nodes postorder_impl( const Nodes& roots, const Succs& succs );

template< typename Nodes, typename Succs >
Nodes roots( const Nodes& nodes, const Succs& succs ) {
    auto has_no_predecessor = [&] ( const auto & node ) {
        return query::query( nodes ).all( [&] ( const auto & n ) {
            if ( n == node )
                return true;
            return query::query( postorder_impl( Nodes{ n }, succs ) )
                .none( [&] ( const auto & succ ) { return succ == node; } );
        } );
    };

    return query::query( nodes ).filter( has_no_predecessor ).freeze();
}

template< typename Nodes, typename Succs >
Nodes postorder( const Nodes& nodes, const Succs& succs ) {
    auto order_roots = roots( nodes, succs );
    return postorder_impl( order_roots, succs );
}

template< typename Nodes, typename Succs >
Nodes postorder_impl( const Nodes & roots, const Succs& succs ) {
    using Node = typename Nodes::value_type;

    std::unordered_set< Node > visited;
    std::stack< std::pair< bool, Node > > stack;
    std::vector< Node > order;

    for ( const auto & r : roots )
        stack.emplace( false, r );

    while ( !stack.empty() ) {
        auto node = stack.top();
        stack.pop();

        if ( node.first ) {
            if ( std::find( order.begin(), order.end(), node.second ) == order.end() )
                order.push_back( node.second );
            continue;
        }

        visited.insert( node.second );

        stack.emplace( true, node.second );
        for ( const auto & s : succs( node.second ) )
            if ( visited.find( s ) == visited.end() )
                stack.emplace( false, s );
    }
    return order;
}

template < typename F >
std::vector< F > callPostorder( llvm::Module &m, std::vector< F > functions ) {
    auto & ctx = m.getContext();
    auto fty = llvm::FunctionType::get( llvm::Type::getVoidTy( ctx ), false );

    auto c = m.getOrInsertFunction( "__callgraph_root", fty );
    auto root = llvm::cast< llvm::Function >( c );

    auto bb = llvm::BasicBlock::Create( ctx, "entry", root );
    llvm::IRBuilder<> irb( bb );
    for ( auto &f : functions ) {
        std::vector< llvm::Value * > args;
        for ( auto &arg : f->args() )
            args.push_back( llvm::UndefValue::get( arg.getType() ) );
        irb.CreateCall( f, args );
    }

    std::vector< F > order;
    {
        llvm::CallGraph cg( m );
        auto node = cg[ root ];
        order = query::query( po_begin( node ), po_end( node ) )
            .map( [] ( auto fn ) { return fn->getFunction(); } )
            .filter( [&] ( auto fn ) {
                bool node = std::find( functions.begin(), functions.end(), fn ) != functions.end();
                return fn != nullptr && fn != root && node;
            } )
            .freeze();
    }
    root->eraseFromParent();
    return order;
}

} // namespace analysis
} // namespace lart

#endif

