#pragma once

#include "SSAContext.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PExpr::ssa {

/// Basic block structure for control flow analysis
struct BasicBlock {
    std::string label;                       // Optional label name
    size_t startIndex;                       // Start index in instruction list
    size_t endIndex;                         // End index (exclusive)
    std::unordered_set<size_t> predecessors; // Indices of predecessor blocks (populated by buildControlFlowGraph)
    std::unordered_set<size_t> successors;   // Indices of successor blocks  (populated by buildControlFlowGraph)
};

/// Basic block analyzer utility class
/// Handles identification of basic blocks and control flow graph construction.
class BasicBlockAnalyzer {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    BasicBlockAnalyzer() = default;

    /// Identify basic blocks in the instruction list
    void identifyBasicBlocks(const InstructionList& instructions);

    /// Build control flow graph between basic blocks
    void buildControlFlowGraph(const InstructionList& instructions);

    /// Get the basic blocks
    [[nodiscard]] const std::vector<BasicBlock>& getBasicBlocks() const { return mBasicBlocks; }

private:
    // Basic blocks identified in the function
    std::vector<BasicBlock> mBasicBlocks;

    // Map from label name to block index
    std::unordered_map<std::string, size_t> mLabelToBlock;
};

} // namespace PExpr::ssa