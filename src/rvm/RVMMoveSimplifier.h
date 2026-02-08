#pragma once

#include "RVMInstruction.h"
#include "RVMProgram.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PExpr::rvm {

/// Move simplifier for RVM IR which simplifies all mov instructions when they are identity.
/// Note that the final target register of a move chain should be kept alive to ensure
/// the calling convention (parameters passed by %r0, ... and values returned by %r0, ...)
/// is not invalidated.
class RVMMoveSimplifier {
public:
    /// Apply move simplification to an RVM program.
    /// Returns true if any changes were made.
    [[nodiscard]] static bool simplify(RVMProgram& program);

private:
    /// Process a single basic block (sequence of instructions between labels/branches)
    [[nodiscard]] static bool simplifyBlock(std::vector<std::shared_ptr<RVMInstr>>& block);

    /// Check if an instruction is a MOV instruction
    [[nodiscard]] static bool isMovInstruction(const RVMInstr* instr);

    /// Check if a MOV instruction is identity (src and dst are same register)
    [[nodiscard]] static bool isIdentityMov(const RVMInstr2Op* movInstr);

    /// Build register renaming map for a block, considering pinned registers
    static void buildRenameMap(
        const std::vector<std::shared_ptr<RVMInstr>>& block,
        std::unordered_map<RegId, RegId>& renameMap,
        std::unordered_set<RegId>& pinnedRegs);

    /// Collect registers that cannot be renamed (used in non-MOV instructions)
    static void collectPinnedRegisters(
        const std::vector<std::shared_ptr<RVMInstr>>& block,
        std::unordered_set<RegId>& pinnedRegs);

    /// Apply register renaming to a value
    static RVMValue renameValue(const RVMValue& value, const std::unordered_map<RegId, RegId>& renameMap);
};

} // namespace PExpr::rvm