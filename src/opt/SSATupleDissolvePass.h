#pragma once

#include "ssa/SSAContext.h"
#include "ssa/SSAStructs.h"

#include <unordered_map>
#include <vector>

namespace PExpr::opt {

/// SSATupleDissolvePass dissolves all tuples into their corresponding SSA values for each element.
/// This pass transforms tuple operations into individual scalar/vector operations.
/// This pass also applies dead-code ellimination.
class SSATupleDissolvePass {
public:
    using InstructionList = std::vector<std::shared_ptr<ssa::SSAInstr>>;

    /// Dissolve tuples in the given program
    /// @return true if any tuples were dissolved, false otherwise
    bool dissolve(ssa::SSAContext* context, ssa::SSAProgram& program);

    inline void clear()
    {
        mTupleElements.clear();
    }

private:
    /// Dissolve tuples in a list of instructions
    /// @return true if any tuples were dissolved, false otherwise
    bool dissolveInstructions(ssa::SSAContext* context, InstructionList& instructions);

    /// Map from tuple value hash to their element values
    std::unordered_map<size_t, std::vector<ssa::SSAValue>> mTupleElements;
};

} // namespace PExpr::opt
