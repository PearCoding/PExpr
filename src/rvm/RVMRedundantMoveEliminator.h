#pragma once

#include "RVMInstruction.h"
#include "RVMProgram.h"

#include <memory>
#include <unordered_set>
#include <vector>

namespace PExpr::rvm {

/// Eliminates redundant MOV instructions within basic blocks.
/// A MOV instruction is redundant if its destination register is overwritten
/// later in the same basic block before being read.
class RVMRedundantMoveEliminator {
public:
    /// Apply redundant MOV elimination to an RVM program.
    /// Returns true if any changes were made.
    [[nodiscard]] static bool eliminate(RVMProgram& program);

private:
    /// Process a single basic block
    [[nodiscard]] static bool eliminateInBlock(std::vector<std::shared_ptr<RVMInstr>>& block);

    /// Analyze a block to find redundant MOVs
    static void analyzeBlock(const std::vector<std::shared_ptr<RVMInstr>>& block, std::unordered_set<size_t>& redundantIndices);
};

} // namespace PExpr::rvm