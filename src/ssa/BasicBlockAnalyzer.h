#pragma once

#include "SSAMapper.h"
#include "SSAOptions.h"

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

    // Extract instructions from the main instruction list
    void extractInstructions(const std::vector<std::shared_ptr<SSAInstr>>& allInstructions,
                             std::vector<std::shared_ptr<SSAInstr>>& output) const
    {
        if (startIndex >= allInstructions.size() || startIndex >= endIndex)
            return;
        for (size_t i = startIndex; i < endIndex && i < allInstructions.size(); ++i)
            output.push_back(allInstructions[i]);
    }

    /// Check if this block contains a given instruction index
    [[nodiscard]] inline bool containsInstruction(size_t index) const { return index >= startIndex && index < endIndex; }

    /// Get the number of instructions in this block
    [[nodiscard]] inline size_t size() const { return endIndex > startIndex ? endIndex - startIndex : 0; }
};

/// Basic block analyzer utility class
/// Handles identification of basic blocks, control flow graph construction,
/// and dominator computation.
class BasicBlockAnalyzer {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    BasicBlockAnalyzer() = default;

    /// Identify basic blocks in the instruction list
    void identifyBasicBlocks(const InstructionList& instructions);

    /// Build control flow graph between basic blocks
    void buildControlFlowGraph(const InstructionList& instructions);

    /// Compute dominator tree
    void computeDominators();

    /// Find natural loops in the CFG
    void findNaturalLoops();

    /// Get the basic blocks
    [[nodiscard]] const std::vector<BasicBlock>& getBasicBlocks() const { return mBasicBlocks; }

    /// Get dominator sets
    [[nodiscard]] const std::vector<std::unordered_set<size_t>>& getDominators() const { return mDominators; }

    /// Get immediate dominators
    [[nodiscard]] const std::vector<size_t>& getImmediateDominators() const { return mImmediateDominator; }

    /// Get natural loops
    [[nodiscard]] const std::vector<std::unordered_set<size_t>>& getNaturalLoops() const { return mLoops; }

    /// Check if block A dominates block B
    [[nodiscard]] bool dominates(size_t a, size_t b) const;

    /// Check if block A strictly dominates block B
    [[nodiscard]] bool strictlyDominates(size_t a, size_t b) const;

    /// Find the basic block containing a given instruction index
    [[nodiscard]] size_t findBlockContainingInstruction(size_t instructionIndex) const;

    /// Split a basic block at the given instruction index (creates a new block)
    /// Returns the index of the new block, or SIZE_MAX if splitting failed
    size_t splitBasicBlock(size_t blockIndex, size_t splitInstructionIndex);

    /// Merge consecutive basic blocks if there's no control flow between them
    bool mergeConsecutiveBlocks(size_t firstBlockIndex, size_t secondBlockIndex);

    /// Get instructions for a specific basic block
    void getBlockInstructions(size_t blockIndex, const InstructionList& allInstructions,
                              InstructionList& blockInstructions) const;

private:
    // Basic blocks identified in the function
    std::vector<BasicBlock> mBasicBlocks;

    // Dominator sets: dominators[block] = set of blocks that dominate this block
    std::vector<std::unordered_set<size_t>> mDominators;

    // Immediate dominator for each block
    std::vector<size_t> mImmediateDominator;

    // Natural loops in the CFG
    std::vector<std::unordered_set<size_t>> mLoops;

    // Map from label name to block index
    std::unordered_map<std::string, size_t> mLabelToBlock;

    /// Initialize dominator sets for iterative computation
    void initializeDominators();

    /// Compute immediate dominators from dominator sets
    void computeImmediateDominators();

    /// Check if there's a back edge from block A to block B
    [[nodiscard]] bool isBackEdge(size_t from, size_t to) const;

    /// Get all blocks that can reach 'start' without going through 'exclude'
    void collectReachableBlocks(size_t start, size_t exclude, std::unordered_set<size_t>& result) const;
};

} // namespace PExpr::ssa