#include "SSAPassSSCP.h"

namespace PExpr::ssa {

namespace intrinsics {
extern void setupIntrinsics(SSCPFunctionInliner& inliner);
}

SSAPassSSCP::SSAPassSSCP(const SSAOptions& opts)
    : mOptions(opts)
    , mContext(std::make_unique<SSAContext>())
    , mConstantFolder(std::make_unique<SSCPConstantFolder>())
    , mControlFlowOptimizer(std::make_unique<SSCPControlFlowOptimizer>())
    , mDeadCodeOptimizer(std::make_unique<SSCPDeadCodeOptimizer>())
    , mFunctionInliner(std::make_unique<SSCPFunctionInliner>(opts))
    , mIdentityOptimizer(std::make_unique<SSCPIdentityOptimizer>(opts))
    , mSideEffectAnalyzer(std::make_unique<SSCPSideEffectAnalyzer>())
{
    intrinsics::setupIntrinsics(*mFunctionInliner);
}

SSAPassSSCP::~SSAPassSSCP() = default;

void SSAPassSSCP::Run(const SSAOptions& opts, SSAProgram& program)
{
    SSAPassSSCP sscp(opts);

    // 0) Analyze body to update SSA context with current variable counters
    sscp.mContext->reset();
    sscp.mContext->analyze(program);

    sscp.runProgram(program);
}

void SSAPassSSCP::Run(const SSAOptions& opts, InstructionList& body)
{
    SSAPassSSCP sscp(opts);

    // 0) Analyze body to update SSA context with current variable counters
    sscp.mContext->reset();
    sscp.mContext->analyze(body);

    // Keep optimizing until no changes are possible
    bool changed = true;
    while (changed)
        changed = sscp.processBody(body);
}

void SSAPassSSCP::runProgram(SSAProgram& program)
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

bool SSAPassSSCP::processBody(InstructionList& body)
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

    if (mOptions.RemoveDeadCode) {
        // 4) Remove dead assigns without side effects
        if (mDeadCodeOptimizer->removeDeadAssigns(body, mSideEffectAnalyzer->getSideEffectFunctions()))
            changed = true;
    }

    // 5) Remove empty branches
    if (mControlFlowOptimizer->removeEmptyBranches(body))
        changed = true;

    // 6) Handle unused labels
    if (mControlFlowOptimizer->removeObsoleteLabels(body))
        changed = true;

    if (mOptions.RemoveDeadCode) {
        // 7) Collapse phi nodes
        if (mControlFlowOptimizer->collapsePhiNodes(body))
            changed = true;
    }

    return changed;
}

} // namespace PExpr::ssa
