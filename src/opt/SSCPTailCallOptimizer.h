#pragma once

#include "PExpr.h"
#include "ssa/SSAContext.h"
#include "ssa/SSAStructs.h"

namespace PExpr::opt {

class SSCPTailCallOptimizer {
public:
    using InstructionList = std::vector<std::shared_ptr<ssa::SSAInstr>>;

    SSCPTailCallOptimizer() = default;

    /// Optimize tail calls in a program
    /// Returns true if any changes were made
    bool optimizeTailCalls(ssa::SSAProgram& program);

    /// Optimize tail calls in a function body
    /// Returns true if any changes were made
    bool optimizeTailCalls(ssa::SSAContext* ctx, InstructionList& instructions, const ssa::SSAFunction* currentFunction = nullptr);

private:
    /// Check if an instruction is a tail call (call followed immediately by return)
    bool isTailCall(const std::shared_ptr<ssa::SSAInstr>& callInstr,
                    const std::shared_ptr<ssa::SSAInstr>& nextInstr) const;

    /// Check if a call is recursive (calls the same function)
    bool isRecursiveCall(const ssa::SSAInstrCall* callInstr,
                         const ssa::SSAFunction* currentFunction) const;

    /// Transform a tail recursive call into a loop
    bool transformTailRecursion(ssa::SSAContext* ctx,
                                InstructionList& instructions,
                                size_t callIndex,
                                ssa::SSAInstrCall* callInstr,
                                ssa::SSAInstrReturn* returnInstr);

    /// Transform a general tail call (non-recursive)
    bool transformTailCall(ssa::SSAContext* ctx,
                           InstructionList& instructions,
                           size_t callIndex,
                           ssa::SSAInstrCall* callInstr,
                           ssa::SSAInstrReturn* returnInstr);
};

} // namespace PExpr::opt