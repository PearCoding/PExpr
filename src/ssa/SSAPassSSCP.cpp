#include "SSAPassSSCP.h"
#include "SSCPConstantFolder.h"
#include "SSCPControlFlowOptimizer.h"
#include "SSCPFunctionInliner.h"
#include "SSCPSideEffectAnalyzer.h"

namespace PExpr::ssa {

SSAPassSSCP::SSAPassSSCP()
    : mConstantFolder(std::make_unique<SSCPConstantFolder>())
    , mControlFlowOptimizer(std::make_unique<SSCPControlFlowOptimizer>())
    , mFunctionInliner(std::make_unique<SSCPFunctionInliner>())
    , mSideEffectAnalyzer(std::make_unique<SSCPSideEffectAnalyzer>())
{
}

SSAPassSSCP::~SSAPassSSCP() = default;

void SSAPassSSCP::Run(SSAProgram& program)
{
    SSAPassSSCP sscp;
    sscp.runProgram(program);
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

        // Check if we can inline some functions
        for (auto& func : program.Functions) {
            if (func.External)
                continue;

            if (mFunctionInliner->attempFunctionInlining(program, func))
                changed = true;
        }

        // Remove unused functions after inlining
        mFunctionInliner->removeUnusedFunctions(program);
    }
}

bool SSAPassSSCP::processBody(InstructionList& body)
{
    bool changed = false;

    // 1) Replace operands with known constants where possible
    if (mConstantFolder->replaceOperandIfConst(body))
        changed = true;

    // 2) Try to fold assignments into constants
    if (mConstantFolder->foldToConstants(body))
        changed = true;

    // 3) Count uses
    mControlFlowOptimizer->resetAndCountUses(body);

    // 4) Remove dead assigns without side effects
    if (mControlFlowOptimizer->removeDeadAssigns(body, mSideEffectAnalyzer->getSideEffectFunctions()))
        changed = true;

    // 5) Remove empty branches
    if (mControlFlowOptimizer->removeEmptyBranches(body))
        changed = true;

    // 6) Handle unused labels
    if (mControlFlowOptimizer->removeObsoleteLabels(body))
        changed = true;

    // 7) Collapse phi nodes
    if (mControlFlowOptimizer->collapsePhiNodes(body))
        changed = true;

    return changed;
}

} // namespace PExpr::ssa
