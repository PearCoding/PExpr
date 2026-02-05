#pragma once

#include "SSAContext.h"
#include "SSAStructs.h"

#include <unordered_map>
#include <vector>

namespace PExpr::ssa {

/// SSATupleDissolvePass dissolves all tuples into their corresponding SSA values for each element.
/// This pass transforms tuple operations into individual scalar/vector operations.
/// This pass also applies dead-code ellimination.
class SSATupleDissolvePass {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    /// Dissolve tuples in the given program and removes dead-code
    /// @return true if any tuples were dissolved, false otherwise
    bool dissolve(SSAProgram& program);

private:
    /// Dissolve tuples in a list of instructions
    /// @return true if any tuples were dissolved, false otherwise
    bool dissolveInstructions(InstructionList& instructions);

    /// Check if any value in the instruction list has tuple type
    bool hasTupleValues(const InstructionList& instructions);

    /// Map from tuple value names to their element values
    std::unordered_map<std::string, std::vector<SSAValue>> mTupleElements;

    /// SSA context for generating unique names
    SSAContext mContext;
};

} // namespace PExpr::ssa
