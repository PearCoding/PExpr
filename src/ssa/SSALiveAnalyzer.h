#pragma once

#include "SSAContext.h"

#include <memory>
#include <unordered_set>
#include <vector>

namespace PExpr::ssa {

/// Utility class to analyze which variables in a basic block (InstructionList)
/// are alive inside block. Given an InstructionList (basic block), it
/// outputs a list containing the most recent version of each variable which
/// was targeted inside the block.
class SSALiveAnalyzer {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    SSALiveAnalyzer() = default;

    /// Analyze an instruction list (basic block) and return a list of SSAValues
    /// representing the most recent version of each variable that was targeted.
    ///
    /// @param instructions The instruction list (basic block) to analyze
    std::vector<SSAValue> analyze(const InstructionList& instructions);

private:
    /// Reset the analyzer state (clear all internal data structures).
    void reset();

    /// Process a single SSA value (used as an operand in an instruction)
    /// @param val The value to process
    void processOperandValue(const SSAValue& val);

    /// Process a single SSA value (defined as a target in an instruction)
    /// @param val The value to process
    void processTargetValue(const SSAValue& val);

    std::unordered_set<SSAValue> mUsedValues;
    std::unordered_set<SSAValue> mTargetedValues;
};

} // namespace PExpr::ssa