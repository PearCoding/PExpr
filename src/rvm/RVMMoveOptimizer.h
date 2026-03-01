#pragma once

#include "RVMInstruction.h"
#include "RVMProgram.h"
#include "RVMLiveAnalyzer.h"
#include "RVMBasicBlockAnalyzer.h"
#include "opt/OptimizerOptions.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PExpr::rvm {

/// Move optimizer for RVM IR which performs:
/// 1. Identity move elimination (MOV where src == dst)
/// 2. Move chain simplification (coalescing MOV chains)
/// 3. Redundant move elimination (MOV overwritten before being read)
/// 
/// Uses global live interval analysis across the entire program,
/// respecting basic block boundaries and control flow.
class RVMMoveOptimizer {
public:
    /// Apply move optimizations to an RVM program based on options.
    /// Returns true if any changes were made.
    [[nodiscard]] static bool optimize(const opt::OptimizerOptions& options, RVMProgram& program);

private:
    /// Analyze and optimize a single program with given options
    [[nodiscard]] static bool optimizeProgram(const opt::OptimizerOptions& options, RVMProgram& program);

    /// Check if an instruction is a MOV instruction
    [[nodiscard]] static bool isMovInstruction(const RVMInstr* instr);

    /// Check if a MOV instruction is identity (src and dst are same register)
    [[nodiscard]] static bool isIdentityMov(const RVMInstr2Op* movInstr);

    /// Remove identity moves from a block
    static size_t removeIdentityMoves(std::vector<std::shared_ptr<RVMInstr>>& block, bool& changed);

    /// Run independent optimization passes
    [[nodiscard]] static bool runIdentityMovePass(RVMProgram& program);
    [[nodiscard]] static bool runMoveChainPass(RVMProgram& program);
    [[nodiscard]] static bool runRedundantMovePass(RVMProgram& program);

    /// Remove redundant moves in a block using global intervals
    static void removeRedundantMovesInBlock(
        std::vector<std::shared_ptr<RVMInstr>>& block,
        size_t blockStartIndex,
        const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals,
        bool& changed);
};

} // namespace PExpr::rvm