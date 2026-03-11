#include "SSAOptimizer.h"

#include "SSATupleDissolvePass.h"
#include "SSCPCommonSubexpressionEliminator.h"
#include "SSCPConstantFolder.h"
#include "SSCPControlFlowOptimizer.h"
#include "SSCPDeadCodeOptimizer.h"
#include "SSCPFunctionInliner.h"
#include "SSCPIdentityOptimizer.h"
#include "SSCPPreOptimizer.h"
#include "SSCPSideEffectAnalyzer.h"
#include "SSCPTailCallOptimizer.h"

namespace PExpr::opt {

namespace intrinsics {
extern void setupIntrinsics(SSCPFunctionInliner& inliner);
}

SSAOptimizer::SSAOptimizer(const OptimizerOptions& opts)
    : mOptions(opts)
    , mContext(std::make_unique<ssa::SSAContext>())
    , mConstantFolder(std::make_unique<SSCPConstantFolder>())
    , mControlFlowOptimizer(std::make_unique<SSCPControlFlowOptimizer>())
    , mDeadCodeOptimizer(std::make_unique<SSCPDeadCodeOptimizer>())
    , mFunctionInliner(std::make_unique<SSCPFunctionInliner>(opts))
    , mIdentityOptimizer(std::make_unique<SSCPIdentityOptimizer>(opts))
    , mSideEffectAnalyzer(std::make_unique<SSCPSideEffectAnalyzer>())
    , mCommonSubexpressionEliminator(std::make_unique<SSCPCommonSubexpressionEliminator>(opts))
    , mPreOptimizer(std::make_unique<SSCPPreOptimizer>(opts))
    , mTailCallOptimizer(std::make_unique<SSCPTailCallOptimizer>())
    , mTupleDissolvePass(std::make_unique<SSATupleDissolvePass>())
{
    intrinsics::setupIntrinsics(*mFunctionInliner);
}

SSAOptimizer::~SSAOptimizer() = default;

void SSAOptimizer::Run(const OptimizerOptions& opts, ssa::SSAProgram& program)
{
    SSAOptimizer sscp(opts);

    // 0) Analyze body to update SSA context with current variable counters
    sscp.mContext->reset();
    sscp.mContext->analyze(program);

    sscp.runProgram(program);
}

void SSAOptimizer::Run(const OptimizerOptions& opts, InstructionList& body)
{
    SSAOptimizer sscp(opts);

    // 0) Analyze body to update SSA context with current variable counters
    sscp.mContext->reset();
    sscp.mContext->analyze(body);

    // Keep optimizing until no changes are possible
    bool changed = true;
    while (changed)
        changed = sscp.processBody(body);
}

void SSAOptimizer::runProgram(ssa::SSAProgram& program)
{
    mSideEffectAnalyzer->propagateSideEffects(program);
    mTupleDissolvePass->clear();

    // Keep optimizing until no changes are possible
    bool changed = true;
    while (changed) {
        changed = false;

        mFunctionInliner->analyzeCallGraph(program);

        // Remove unused functions at the beginning to speed up stuff later
        if (mOptions.RemoveDeadCode)
            changed |= mFunctionInliner->removeUnusedFunctions(program);

        // Process main program body
        changed |= processBody(program.Body);

        // Process all functions
        for (auto& func : program.Functions)
            changed |= processBody(func.Body);

        if (mOptions.InlineFunctions || mOptions.ForceInlineFunctions) {
            // Check if we can inline some functions
            for (auto& func : program.Functions)
                changed |= mFunctionInliner->attempFunctionInlining(mContext.get(), program, func);
        }
    }
}

bool SSAOptimizer::processBody(InstructionList& body)
{
    if (body.empty())
        return false;

    bool changed = false;

    // 1) Replace operands with known constants where possible
    changed |= mConstantFolder->replaceOperandIfConst(body);

    // 2) Try to fold assignments into constants. This requires dead code removal, or it will just go on for ever.
    if (mOptions.EnableConstantFolding && mOptions.RemoveDeadCode)
        changed |= mConstantFolder->foldToConstants(mOptions.EnableConstantFoldingNumber, body);

    // 3) Apply identity optimizations per basic block
    changed |= mIdentityOptimizer->applyIdentities(mContext.get(), body);

    // 4) Apply common subexpression elimination per basic block
    if (mOptions.EliminateCommonSubexpressions)
        changed |= mCommonSubexpressionEliminator->applyCSE(mContext.get(), body, mSideEffectAnalyzer->getSideEffectFunctions());

    // 5) Remove dead assigns without side effects
    if (mOptions.RemoveDeadCode)
        changed |= mDeadCodeOptimizer->removeDeadAssigns(body, mSideEffectAnalyzer->getSideEffectFunctions());

    // 6) Remove empty branches
    changed |= mControlFlowOptimizer->removeEmptyBranches(body);

    // 7) Handle unused labels
    changed |= mControlFlowOptimizer->removeObsoleteLabels(body);

    // 8) Collapse condition based nodes
    if (mOptions.RemoveDeadCode)
        changed |= mControlFlowOptimizer->collapse(body);

    if (mOptions.EliminatePartialRedundancies) {
        // 9) Apply partial redundancy elimination
        // if (mPreOptimizer->applyPRE(mContext.get(), body, mSideEffectAnalyzer->getSideEffectFunctions()))
        //     changed = true;
    }

    // 10) Apply tail call optimization
    if (mOptions.OptimizeTailCalls)
        changed |= mTailCallOptimizer->optimizeTailCalls(mContext.get(), body);

    return changed;
}

} // namespace PExpr::opt
