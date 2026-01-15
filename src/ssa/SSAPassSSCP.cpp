#include "SSAPassSSCP.h"
#include "SSCPConstantFolder.h"
#include "SSCPControlFlowOptimizer.h"
#include "SSCPFunctionInliner.h"
#include "SSCPSideEffectAnalyzer.h"

namespace PExpr::ssa {

SSAPassSSCP::SSAPassSSCP()
    : mConstantFolder(std::make_unique<SSCPConstantFolder>(mConstants))
    , mControlFlowOptimizer(std::make_unique<SSCPControlFlowOptimizer>(mUseCount))
    , mFunctionInliner(std::make_unique<SSCPFunctionInliner>())
    , mSideEffectAnalyzer(std::make_unique<SSCPSideEffectAnalyzer>())
{
}

SSAPassSSCP::~SSAPassSSCP() = default;

void SSAPassSSCP::run(SSAProgram& program)
{
    mSideEffectAnalyzer->propagateSideEffects(program);

    // Reset inlining state for this run
    // Note: mInlineAttempts is now managed by SSCPFunctionInliner

    // Now proceed with the usual SSCP iterations (replace operands, fold, DCE), but
    // consider calls side-effecting only if the callee is marked side-effecting.
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
    for (auto& instrPtr : body) {
        if (!instrPtr)
            continue;
        if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
            auto folded = mConstantFolder->foldAssign(asg);
            if (folded) {
                if (!asg->Target.Name.empty()) {
                    mConstants[asg->Target.Name] = *folded;
                    // mutate instruction to literal form
                    SSAInstrAssign lit;
                    lit.Target   = asg->Target;
                    lit.Operator = SSAInstrAssign::OpKind::Assign;
                    lit.Operands = { *folded };
                    *asg         = std::move(lit);
                    changed      = true;
                }
            }
        }
    }

    // 3) Dead code elimination: compute use counts and remove dead assigns without side effects
    mUseCount.clear();
    for (const auto& instrPtr : body)
        mControlFlowOptimizer->countUsesInInstr(instrPtr.get());

    // TODO: This can be done more efficiently
    InstructionList newBody;
    newBody.reserve(body.size());
    for (const auto& instrPtr : body) {
        if (!instrPtr)
            continue;
        if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
            int uses = 0;
            auto it  = mUseCount.find(asg->Target.Name);
            if (it != mUseCount.end())
                uses = it->second;
            if (uses == 0 && !mControlFlowOptimizer->instrHasSideEffects(instrPtr.get(), mSideEffectAnalyzer->getSideEffectFunctions())) {
                changed = true;
                continue;
            }
        }
        newBody.push_back(instrPtr);
    }
    body.swap(newBody);

    // 4) Remove empty branches
    if (mControlFlowOptimizer->removeEmptyBranches(body))
        changed = true;

    // 5) Handle unused labels
    if (mControlFlowOptimizer->removeObsoleteLabels(body))
        changed = true;

    // 6) Collapse phi nodes
    if (mControlFlowOptimizer->collapsePhiNodes(body))
        changed = true;

    return changed;
}

void SSAPassSSCP::resetState()
{
    mConstants.clear();
    mUseCount.clear();
    mSideEffectAnalyzer.reset(new SSCPSideEffectAnalyzer());
}

} // namespace PExpr::ssa
