#pragma once

#include "RVMInstruction.h"
#include "RVMProgram.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PExpr::rvm {

/// Register linearizer for RVM IR which remaps registers to a linear sequence
/// while preserving their relative order. This is a syntax-only pass that
/// does not change program semantics, only makes register numbering sequential.
class RVMRegisterLinearizer {
public:
    /// Apply register linearization to an RVM program.
    /// Returns true if any changes were made.
    [[nodiscard]] static bool linearize(RVMProgram& program);

private:
    /// Collect all registers used in program
    static std::unordered_set<RegId> collectRegisters(const RVMProgram& program);

    /// Create register mapping to linear sequence preserving order
    static std::unordered_map<RegId, RegId> createLinearMapping(const std::unordered_set<RegId>& registers);

    /// Rewrite program with new register assignments
    static void rewriteProgram(RVMProgram& program, const std::unordered_map<RegId, RegId>& regMap);
};

} // namespace PExpr::rvm