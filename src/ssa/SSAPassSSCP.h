#pragma once

#include "SSCPConstantFolder.h"
#include "SSCPControlFlowOptimizer.h"
#include "SSCPFunctionInliner.h"
#include "SSCPSideEffectAnalyzer.h"

#include <memory>

namespace PExpr::ssa {

/// Sparse Conditional Constant Propagation (SSCP) pass for the SSA IR.
class SSAPassSSCP {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    /// Run the pass on a program. Modifies the program in-place.
    static void Run(SSAProgram& program);

private:
    SSAPassSSCP();
    ~SSAPassSSCP();

    /// Run the pass on a program. Modifies the program in-place.
    void runProgram(SSAProgram& program);
    bool processBody(InstructionList& body);

    // Refactored components
    std::unique_ptr<SSCPConstantFolder> mConstantFolder;
    std::unique_ptr<SSCPControlFlowOptimizer> mControlFlowOptimizer;
    std::unique_ptr<SSCPFunctionInliner> mFunctionInliner;
    std::unique_ptr<SSCPSideEffectAnalyzer> mSideEffectAnalyzer;
};

} // namespace PExpr::ssa
