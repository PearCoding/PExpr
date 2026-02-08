#pragma once

#include "OptimizerOptions.h"

#include <memory>

namespace PExpr::ssa {
class SSAInstr;
class SSAContext;
class SSAProgram;
} // namespace PExpr::ssa

namespace PExpr::opt {
class SSCPConstantFolder;
class SSCPControlFlowOptimizer;
class SSCPDeadCodeOptimizer;
class SSCPFunctionInliner;
class SSCPIdentityOptimizer;
class SSCPSideEffectAnalyzer;
class SSCPCommonSubexpressionEliminator;
class SSCPPreOptimizer;
class SSCPTailCallOptimizer;
class SSATupleDissolvePass;

/// Optimizer containing multiple optimization passes for the SSA IR.
class SSAOptimizer {
public:
    using InstructionList = std::vector<std::shared_ptr<ssa::SSAInstr>>;

    /// Run the passes on a program. Modifies the program in-place.
    static void Run(const OptimizerOptions& opts, ssa::SSAProgram& program);

    /// Run the passes on a subset of instructions. Modifies the instructions in-place.
    /// This does not apply function inlining
    static void Run(const OptimizerOptions& opts, InstructionList& body);

private:
    SSAOptimizer(const OptimizerOptions& opts);
    ~SSAOptimizer();

    /// Run the pass on a program. Modifies the program in-place.
    void runProgram(ssa::SSAProgram& program);
    bool processBody(InstructionList& body);

    const OptimizerOptions mOptions;

    // SSA Context for variable name generation
    std::unique_ptr<ssa::SSAContext> mContext;

    // Components
    std::unique_ptr<SSCPConstantFolder> mConstantFolder;
    std::unique_ptr<SSCPControlFlowOptimizer> mControlFlowOptimizer;
    std::unique_ptr<SSCPDeadCodeOptimizer> mDeadCodeOptimizer;
    std::unique_ptr<SSCPFunctionInliner> mFunctionInliner;
    std::unique_ptr<SSCPIdentityOptimizer> mIdentityOptimizer;
    std::unique_ptr<SSCPSideEffectAnalyzer> mSideEffectAnalyzer;
    std::unique_ptr<SSCPCommonSubexpressionEliminator> mCommonSubexpressionEliminator;
    std::unique_ptr<SSCPPreOptimizer> mPreOptimizer;
    std::unique_ptr<SSCPTailCallOptimizer> mTailCallOptimizer;
    std::unique_ptr<SSATupleDissolvePass> mTupleDissolvePass;
};

} // namespace PExpr::opt
