#pragma once

#include "RVMInstruction.h"
#include "RVMProgram.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace PExpr::rvm {

/// Analyzes basic blocks in RVM programs
class RVMBasicBlockAnalyzer {
public:
    using Block = std::vector<std::shared_ptr<RVMInstr>>;
    using BlockList = std::vector<Block>;

    /// Split an RVM program into basic blocks
    /// A basic block is a sequence of instructions with:
    /// - No internal labels, branches, jumps, or returns
    /// - Only the first instruction can be a label
    /// - Only the last instruction can be a branch, jump, or return
    [[nodiscard]] static BlockList splitIntoBlocks(const RVMProgram& program);

    /// Check if an instruction is a control flow instruction
    [[nodiscard]] static bool isControlFlow(const RVMInstr* instr);

    /// Check if an instruction starts a new basic block
    [[nodiscard]] static bool startsNewBlock(const RVMInstr* instr);

    /// Check if an instruction ends a basic block
    [[nodiscard]] static bool endsBlock(const RVMInstr* instr);

    /// Get successors of a basic block (labels that can be jumped to from this block)
    [[nodiscard]] static std::vector<std::string> getSuccessors(const Block& block);

    /// Get predecessor information for building control flow graph
    [[nodiscard]] static std::unordered_map<std::string, std::vector<size_t>> 
        buildPredecessorMap(const BlockList& blocks);
};

} // namespace PExpr::rvm