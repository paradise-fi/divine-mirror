#include <divine/sim/cli.hpp>
#include <lart/support/util.h>

namespace divine::sim {

std::set< llvm::StoreInst* > get_first_stores_to_alloca( llvm::AllocaInst* );

/* Find the origin of a local variable in the original (pre-LART) LLVM module */
llvm::Value* CLI::find_tamperee( const DN &dn )
{
    if ( !dn._var_loc )
        return nullptr;

    auto *m = _bc->_pure_module.get();
    if ( auto *origin = llvm::dyn_cast< llvm::Instruction >( dn._var_loc ) )
    {
        auto *f = m->getFunction( origin->getFunction()->getName().str() );
        for ( auto &bb : *f )
        {
            for ( auto &inst : bb )
            {
                if ( llvm::isa< llvm::DbgDeclareInst >( inst ) ||
                     llvm::isa< llvm::DbgValueInst > ( inst ) )
                {
                    // llvm::DbgVariableIntrinsic in LLVM 9
                    auto *di = llvm::cast< llvm::DbgInfoIntrinsic >( &inst );
                    auto *divar = di->getVariable();

                    if ( divar &&
                         divar->getName().equals( dn._di_var->getName() ) &&
                         divar->getLine() == dn._di_var->getLine() &&
                         divar->getFilename().equals( dn._di_var->getFilename() ) &&
                         divar->getDirectory().equals( dn._di_var->getDirectory() ) &&
                         divar->getName().equals( dn._di_var->getName() ) )
                        return di->getVariableLocation();
                }
            }
        }
        return nullptr;
    }
    else if ( auto *origin = llvm::dyn_cast< llvm::Argument >( dn._var_loc ) )
    {
        auto *f = m->getFunction( origin->getParent()->getName().str() );
        unsigned argno = origin->getArgNo();
        return f->arg_begin() + argno;
    }
    else
        UNREACHABLE( "tamperee is something weird" );
}

namespace {
/* Set dummy location metadata (needed for call instructions) */
template< typename IRBuilder >
void prepareDebugMetadata( IRBuilder & irb )
{
    auto *discope = irb.GetInsertBlock()->getParent()->getSubprogram();
    auto dl = llvm::DebugLoc::get( 0, 0, discope, nullptr );
    irb.SetCurrentDebugLocation( dl );
}

/* Get correct __<domain>_<method>_<type> constructor, such as __sym_val_i64 */
template< typename DN >
llvm::Function* getAbstractConstructor( llvm::Module *m, DN & dn,
                                        const std::string & domain, const std::string & method )
{
    int width = dn.size() * 8;
    if ( width != 32 && width != 64 && width != 8 && width != 16 )
        throw brick::except::Error( "unsupported width: " + std::to_string( width ) );

    std::string && name = "__" + domain + "_" + method + "_i" + std::to_string( width );
    auto fn = m->getFunction( name );
    if ( !fn )
        throw brick::except::Error( "function \"" + name + "\" not found." );
    return fn;
}
} /* anonymous namespace */

/* Insert call to __<domain>_val_<type>() */
template< typename IRBuilder >
llvm::Value* CLI::mkCallNondet( IRBuilder & irb, DN &dn, const std::string & domain_name,
                                const std::string & name )
{
    auto aVal = getAbstractConstructor( irb.GetInsertBlock()->getModule(), dn, domain_name, "val" );
    prepareDebugMetadata( irb );
    return irb.CreateCall( aVal, llvm::NoneType::None, name + ".abstract" );
}

/* Insert call to __<domain>_lift_<type>( original_value ) */
template< typename IRBuilder >
llvm::Value* CLI::mkCallLift( IRBuilder & irb, DN & dn, const std::string & domain_name,
                              llvm::Value *original_value, const std::string & name )
{
    auto aLift = getAbstractConstructor( irb.GetInsertBlock()->getModule(), dn, domain_name, "lift" );
    prepareDebugMetadata( irb );
    llvm::Value * args[1] = { original_value };
    return irb.CreateCall( aLift, args, name + ".lifted" );
}

/* Tamper with a function argument -- replace all uses with nondet or lifted value */
void CLI::tamper( const command::Tamper &cmd, DN &dn, llvm::Argument *arg )
{
    auto argname = arg->getName().str();

    llvm::IRBuilder<> irb( arg->getParent()->getEntryBlock().getFirstNonPHIOrDbgOrLifetime() );
    if ( cmd.lift )
    {
        auto aval = mkCallLift( irb, dn, cmd.domain, arg, argname );
        arg->replaceAllUsesWith( aval );
        llvm::cast< llvm::User >( aval )->replaceUsesOfWith( aval, arg );
    }
    else
    {
        auto aval = mkCallNondet( irb, dn, cmd.domain, argname );
        arg->replaceAllUsesWith( aval );
    }
}

/* Tamper with an alloca'd variable -- replace first stores into it with nondet or lifted value */
void CLI::tamper( const command::Tamper &cmd, DN &dn, llvm::AllocaInst *origin_alloca )
{
    auto varname = origin_alloca->getName().str();

    llvm::IRBuilder<> irb( origin_alloca->getNextNonDebugInstruction() );
    llvm::Value *aval = nullptr;

    // create nondet value to initialise the variable with
    if ( !cmd.lift )
        aval = mkCallNondet( irb, dn, cmd.domain, varname );

    // Find first stores to the alloca and replace them with abstract value
    std::set< llvm::StoreInst* > dominant_users = get_first_stores_to_alloca( origin_alloca );

    if ( !dominant_users.empty() ){

        // create freshness flag for the variable (1 = no store yet, 0 = a store has happened)
        auto &entry = origin_alloca->getFunction()->getEntryBlock();
        irb.SetInsertPoint( entry.getFirstNonPHIOrDbgOrLifetime() );
        auto *a_freshness = irb.CreateAlloca( irb.getInt1Ty(), nullptr, varname + ".fresh" );
        irb.SetInsertPoint( origin_alloca->getNextNonDebugInstruction() );
        irb.CreateStore( irb.getTrue(), a_freshness );

        for ( auto *store : dominant_users )
        {
            auto *stored_orig = store->getValueOperand();

            if ( true /* TODO: heuristic */ )
            {
                if ( cmd.lift )
                {
                    // split the basic block and create branching: if the variable is fresh, lift the
                    // original value.
                    auto bb_upper = store->getParent();
                    std::string bb_name = bb_upper->getName();
                    auto bb_lower = bb_upper->splitBasicBlock( store, bb_name + ".cont" );
                    auto bb_lift = llvm::BasicBlock::Create( bb_lower->getContext(),
                            bb_name + ".lift." + varname, bb_lower->getParent(), bb_lower );
                    bb_upper->getTerminator()->eraseFromParent();

                    irb.SetInsertPoint( bb_upper );
                    auto *a_fresh = irb.CreateLoad( a_freshness, varname + ".isfresh" );
                    irb.CreateCondBr( a_fresh, bb_lift, bb_lower );

                    irb.SetInsertPoint( bb_lift );
                    auto *lifted = mkCallLift( irb, dn, cmd.domain, stored_orig, varname );
                    irb.CreateStore( irb.getFalse(), a_freshness );
                    irb.CreateBr( bb_lower );

                    irb.SetInsertPoint( &bb_lower->front() );
                    auto *phi = irb.CreatePHI( stored_orig->getType(), 2 );
                    phi->addIncoming( stored_orig, bb_upper );
                    phi->addIncoming( lifted, bb_lift );

                    store->replaceUsesOfWith( stored_orig, phi );
                }
                else
                {
                    // if fresh, store the nondet value; mark variable as non-fresh
                    irb.SetInsertPoint( store );
                    auto *a_fresh = irb.CreateLoad( a_freshness, varname + ".isfresh" );
                    auto *stored = irb.CreateSelect( a_fresh, aval, stored_orig,
                                                     varname + ".store" );
                    irb.CreateStore( irb.getFalse(), a_freshness );
                    store->replaceUsesOfWith( stored_orig, stored );
                }
            }
            else
            {
                // The straightforward case -- no dispatch needed
                irb.SetInsertPoint( store );
                if ( cmd.lift )
                    aval = mkCallLift( irb, dn, cmd.domain, store->getValueOperand(), varname );
                store->replaceUsesOfWith( store->getValueOperand(), aval );
            }
        }
    }
}

void find_first_stores_to_alloca( llvm::AllocaInst *origin_alloca, llvm::BasicBlock *bb,
                                  std::set< llvm::StoreInst* > &stores,
                                  std::set< llvm::BasicBlock* > &visited )
{
    // DFS
    visited.insert( bb );
    for ( auto &inst : *bb )
    {
        auto *store = llvm::dyn_cast< llvm::StoreInst >( &inst );
        if ( store && store->getPointerOperand() == origin_alloca )
        {
            stores.insert( store );
            return;
        }
    }
    for ( auto &s : lart::util::succs( bb ) )
        if ( ! visited.count( &s ) )
            find_first_stores_to_alloca( origin_alloca, &s, stores, visited );
}

// Return stores, that are potentially first to the given alloca
std::set< llvm::StoreInst* > get_first_stores_to_alloca( llvm::AllocaInst *origin_alloca )
{
    std::set< llvm::BasicBlock* > visited;
    std::set< llvm::StoreInst* > stores;
    find_first_stores_to_alloca( origin_alloca, origin_alloca->getParent(), stores, visited );
    return stores;
}

} /* divine::sim */
