#include "SSAPassSSCP.h"
#include "SSAContext.h"
#include "SSCPConstantFolder.h"
#include "SSCPControlFlowOptimizer.h"
#include "SSCPFunctionInliner.h"
#include "SSCPIdentityOptimizer.h"
#include "SSCPSideEffectAnalyzer.h"

namespace PExpr::ssa {

namespace intrinsics {
extern void setupIntrinsics(SSCPFunctionInliner& inliner);
}

SSAPassSSCP::SSAPassSSCP(const SSAOptions& opts)
    : mOptions(opts)
    , mContext(std::make_unique<SSAContext>())
    , mConstantFolder(std::make_unique<SSCPConstantFolder>())
    , mControlFlowOptimizer(std::make_unique<SSCPControlFlowOptimizer>())
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
    sscp.runProgram(program);
}

void SSAPassSSCP::Run(const SSAOptions& opts, InstructionList& body)
{
    SSAPassSSCP sscp(opts);

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

        // Process main program body
        if (processBody(program.Body))
            changed = true;

        // Process all functions
        for (auto& func : program.Functions) {
            if (processBody(func.Body))
                changed = true;
        }

        mFunctionInliner->analyzeCallGraph(program);

        if (mOptions.InlineFunctions) {
            // Check if we can inline some functions
            for (auto& func : program.Functions) {
                if (mFunctionInliner->attempFunctionInlining(program, func))
                    changed = true;
            }
        }

        // Remove unused functions after inlining
        if (mOptions.RemoveDeadCode)
            mFunctionInliner->removeUnusedFunctions(program);
    }
}

bool SSAPassSSCP::processBody(InstructionList& body)
{
    bool changed = false;

    // 0) Analyze body to update SSA context with current variable counters
    mContext->reset();
    mContext->analyze(body);

    // 1) Replace operands with known constants where possible
    if (mConstantFolder->replaceOperandIfConst(body))
        changed = true;

    // 2) Try to fold assignments into constants
    if (mOptions.EnableConstantFolding) {
        if (mConstantFolder->foldToConstants(mOptions.EnableConstantFoldingNumber, body))
            changed = true;
    }

    if (mOptions.ApplyMathIdentities) {
        // 3) Apply mathematical identity optimizations
        if (mIdentityOptimizer->applyIdentities(mContext.get(), body))
            changed = true;
    }

    // 4) Count uses
    mControlFlowOptimizer->resetAndCountUses(body);

    if (mOptions.RemoveDeadCode) {
        // 5) Remove dead assigns without side effects
        if (mControlFlowOptimizer->removeDeadAssigns(body, mSideEffectAnalyzer->getSideEffectFunctions()))
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

    return changed;
}

} // namespace PExpr::ssa
