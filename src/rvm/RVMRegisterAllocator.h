#pragma once

#include "RVMLiveAnalyzer.h"
#include "RVMProgram.h"

#include <map>
#include <set>
#include <vector>

namespace PExpr::rvm {

/// Register allocator with integrated MOV optimization.
/// Performs:
/// - Interference graph construction
/// - Aggressive MOV coalescing (merges non-interfering intervals)
/// - Identity MOV removal (MOV rx, rx)
/// - Redundant MOV removal (destination never used)
/// - Graph coloring for register minimization
class RVMRegisterAllocator {
public:
    /// Result of register allocation
    struct AllocationResult {
        bool Changed;            // True if any changes were made
        size_t OriginalRegCount; // Unique registers before allocation
        size_t FinalRegCount;    // Unique registers after allocation
        size_t MovsRemoved;      // Total MOVs removed (coalesced + identity + redundant)
    };

    /// Perform register allocation with integrated MOV optimization
    /// @param program The RVM program to optimize (modified in place)
    /// @return AllocationResult with statistics
    static AllocationResult allocate(RVMProgram& program);

    /// Get number of unique registers used in program
    static size_t getRegisterCount(const RVMProgram& program);

private:
    //=== Data Structures ===

    /// Information about a MOV instruction
    struct MovInfo {
        size_t InstructionIndex; // Index in program
        RegId SrcReg;            // Source register
        RegId DstReg;            // Destination register
        bool IsIdentity;         // true if SrcReg == DstReg
        bool CanCoalesce;        // true if src/dst don't interfere and neither is pinned
        bool ShouldMerge;        // true if registers should be merged (false for dead destination MOVs)
    };

    /// Union-Find for coalescing intervals
    struct UnionFind {
        std::map<RegId, RegId> parent;

        RegId find(RegId x);
        void unite(RegId x, RegId y);
        bool connected(RegId x, RegId y);
    };

    //=== Helper Functions ===

    /// Build list of all MOV instructions with their properties
    static std::vector<MovInfo> collectMovInstructions(
        const RVMProgram& program,
        const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals);

    /// Check if two registers interfere (their live ranges overlap)
    static bool interferes(
        RegId r1, RegId r2,
        const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals,
        size_t movIdx);

    /// Check if a register is pinned (used in call/return)
    static bool isPinned(
        RegId reg,
        const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals);

    /// Check if a MOV destination is dead (never used after the MOV)
    static bool isDestinationDead(
        RegId dstReg,
        const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals,
        size_t movIdx);

    /// Build interference graph: set of (r1, r2) pairs that interfere
    static std::set<std::pair<RegId, RegId>> buildInterferenceGraph(
        const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals);

    /// Perform coalescing: merge non-interfering MOV src/dst
    /// Returns UnionFind structure and set of MOV indices to remove
    static std::pair<UnionFind, std::set<size_t>> performCoalescing(
        const std::vector<MovInfo>& movs,
        const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals);

    /// Assign registers using graph coloring (greedy)
    /// Returns map from original register to allocated register
    static std::map<RegId, RegId> assignRegisters(
        const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals,
        const std::set<std::pair<RegId, RegId>>& interference,
        const UnionFind& coalesced);

    /// Rewrite program with new register assignments and remove MOVs
    static void rewriteProgram(
        RVMProgram& program,
        const std::map<RegId, RegId>& registerMap,
        const std::set<size_t>& movsToRemove);
};

} // namespace PExpr::rvm