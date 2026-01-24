#include "SSAOptimizer.h"

namespace PExpr::ssa {

namespace intrinsics {
extern void setupIntrinsics(SSCPFunctionInliner& inliner);
}

SSAOptimizer::SSAOptimizer(const SSAOptions& opts)
    : mOptions(opts)
    , mContext(std::make_unique<SSAContext>())
    , mConstantFolder(std::make_unique<SSCPConstantFolder>())
    , mControlFlowOptimizer(std::make_unique<SSCPControlFlowOptimizer>())
    , mDeadCodeOptimizer(std::make_unique<SSCPDeadCodeOptimizer>())
    , mFunctionInliner(std::make_unique<SSCPFunctionInliner>(opts))
    , mIdentityOptimizer(std::make_unique<SSCPIdentityOptimizer>(opts))
    , mSideEffectAnalyzer(std::make_unique<SSCPSideEffectAnalyzer>())
    , mCommonSubexpressionEliminator(std::make_unique<SSCPCommonSubexpressionEliminator>(opts))
    , mPreOptimizer(std::make_unique<SSCPPreOptimizer>(opts))
{
    intrinsics::setupIntrinsics(*mFunctionInliner);
}

SSAOptimizer::~SSAOptimizer() = default;

void SSAOptimizer::Run(const SSAOptions& opts, SSAProgram& program)
{
    SSAOptimizer sscp(opts);

    // 0) Analyze body to update SSA context with current variable counters
    sscp.mContext->reset();
    sscp.mContext->analyze(program);

    sscp.runProgram(program);
}

void SSAOptimizer::Run(const SSAOptions& opts, InstructionList& body)
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

void SSAOptimizer::runProgram(SSAProgram& program)
{
    mSideEffectAnalyzer->propagateSideEffects(program);

    // Keep optimizing until no changes are possible
    bool changed = true;
    while (changed) {
        changed = false;

        mFunctionInliner->analyzeCallGraph(program);

        // Remove unused functions at the beginning to speed up stuff later
        if (mOptions.RemoveDeadCode) {
            if (mFunctionInliner->removeUnusedFunctions(program))
                changed = true; // < Should not really change something, but lets still try
        }

        // Process main program body
        if (processBody(program.Body))
            changed = true;

        // Process all functions
        for (auto& func : program.Functions) {
            if (processBody(func.Body))
                changed = true;
        }

        if (mOptions.InlineFunctions) {
            // Check if we can inline some functions
            for (auto& func : program.Functions) {
                if (mFunctionInliner->attempFunctionInlining(mContext.get(), program, func))
                    changed = true;
            }
        }
    }
}

bool SSAOptimizer::processBody(InstructionList& body)
{
    if (body.empty())
        return false;

    bool changed = false;

    // 1) Replace operands with known constants where possible
    if (mConstantFolder->replaceOperandIfConst(body))
        changed = true;

    // 2) Try to fold assignments into constants
    if (mOptions.EnableConstantFolding) {
        if (mConstantFolder->foldToConstants(mOptions.EnableConstantFoldingNumber, body))
            changed = true;
    }

    // 3) Apply identity optimizations
    if (mIdentityOptimizer->applyIdentities(mContext.get(), body))
        changed = true;

    if (mOptions.EliminateCommonSubexpressions) {
        // 4) Apply common subexpression elimination per basic block
        if (mCommonSubexpressionEliminator->applyCSE(mContext.get(), body, mSideEffectAnalyzer->getSideEffectFunctions()))
            changed = true;
    }

    if (mOptions.RemoveDeadCode) {
        // 5) Remove dead assigns without side effects
        if (mDeadCodeOptimizer->removeDeadAssigns(body, mSideEffectAnalyzer->getSideEffectFunctions()))
            changed = true;
    }

    // 6) Remove empty branches
    if (mControlFlowOptimizer->removeEmptyBranches(body))
        changed = true;

    // 7) Handle unused labels
    if (mControlFlowOptimizer->removeObsoleteLabels(body))
        changed = true;

    if (mOptions.RemoveDeadCode) {
        // 8) Collapse phi nodes
        if (mControlFlowOptimizer->collapsePhiNodes(body))
            changed = true;
    }

    if (mOptions.EliminatePartialRedundancies) {
        // 9) Apply partial redundancy elimination
        // if (mPreOptimizer->applyPRE(mContext.get(), body, mSideEffectAnalyzer->getSideEffectFunctions()))
        //     changed = true;
    }

    return changed;
}

} // namespace PExpr::ssa
