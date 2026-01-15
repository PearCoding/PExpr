#pragma once

#include "SSAOptions.h"
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

    /// Run the passes on a program. Modifies the program in-place.
    static void Run(const SSAOptions& opts, SSAProgram& program);

    /// Run the passes on a subset of instructions. Modifies the instructions in-place. This prevents function inlining
    static void Run(const SSAOptions& opts, InstructionList& body);

private:
    SSAPassSSCP(const SSAOptions& opts);
    ~SSAPassSSCP();

    /// Run the pass on a program. Modifies the program in-place.
    void runProgram(SSAProgram& program);
    bool processBody(InstructionList& body);

    const SSAOptions mOptions;

    // Components
    std::unique_ptr<SSCPConstantFolder> mConstantFolder;
    std::unique_ptr<SSCPControlFlowOptimizer> mControlFlowOptimizer;
    std::unique_ptr<SSCPFunctionInliner> mFunctionInliner;
    std::unique_ptr<SSCPSideEffectAnalyzer> mSideEffectAnalyzer;
};

} // namespace PExpr::ssa
