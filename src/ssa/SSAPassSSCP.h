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

    SSAPassSSCP();
    ~SSAPassSSCP();

    /// Run the pass on a program. Modifies the program in-place.
    void run(SSAProgram& program);

private:
    bool processBody(InstructionList& body);
    void resetState();
    
    // map from SSA value name -> constant value (string representation + type)
    std::unordered_map<std::string, SSAValue> mConstants;

    // usage counts for SSA named/temp values
    std::unordered_map<std::string, int> mUseCount;

    // Refactored components
    std::unique_ptr<SSCPConstantFolder> mConstantFolder;
    std::unique_ptr<SSCPControlFlowOptimizer> mControlFlowOptimizer;
    std::unique_ptr<SSCPFunctionInliner> mFunctionInliner;
    std::unique_ptr<SSCPSideEffectAnalyzer> mSideEffectAnalyzer;
};

} // namespace PExpr::ssa
