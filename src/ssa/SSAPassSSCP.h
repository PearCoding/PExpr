#pragma once

#include "SSAMapper.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace PExpr::ssa {

/// Sparse Conditional Constant Propagation (SSCP) pass for the SSA IR.
class SSAPassSSCP {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    SSAPassSSCP() = default;

    /// Run the pass on a program. Modifies the program in-place.
    void run(SSAProgram& program);

private:
    // Helpers
    std::optional<SSAValue> foldAssign(const SSAInstrAssign* asg);
    std::optional<SSAValue> foldUnaryOp(const SSAValue& operand, UnaryOperation unaryOp);
    std::optional<SSAValue> foldBinaryOp(const SSAValue& L, const SSAValue& R, BinaryOperation binaryOp);
    std::optional<SSAValue> foldAccessOp(const SSAValue& operand, const std::string& swizzle);
    std::optional<SSAValue> foldVectorOp(const std::vector<SSAValue>& operands);
    std::optional<SSAValue> foldCastOp(const SSAValue& operand, ElementaryType targetType);

    bool instrHasSideEffects(const SSAInstr* instr) const;
    void countUsesInInstr(const SSAInstr* instr, std::unordered_map<std::string, int>& counts);

    bool removeEmptyBranches(InstructionList& instructions);
    bool removeObsoleteLabels(InstructionList& instructions);
    bool replaceOperandIfConst(std::vector<SSAValue>& ops);
    bool replaceOperandIfConst(InstructionList& instructions);
    bool collapsePhiNodes(InstructionList& instructions);

    void propagateSideEffects(const SSAProgram& program);
    bool processBody(InstructionList& body);

    void analyzeCallGraph(const SSAProgram& program);
    bool inlineFunctionCall(SSAInstrCall* call, SSAFunction& func, InstructionList& instructions, size_t callIndex);
    bool inlineCallsToFunction(SSAProgram& program, SSAFunction& func);
    void removeUnusedFunctions(SSAProgram& program);

    // map from SSA value name -> constant value (string representation + type)
    std::unordered_map<std::string, SSAValue> mConstants;

    // usage counts for SSA named/temp values
    std::unordered_map<std::string, int> mUseCount;

    // set of function names that are considered to have side-effects (externals and those calling externals)
    std::unordered_set<std::string> mSideEffectFunctions;

    // call counts for functions
    std::unordered_map<std::string, int> mCallCounts;
};

} // namespace PExpr::ssa
