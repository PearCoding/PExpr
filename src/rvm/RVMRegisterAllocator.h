#pragma once

#include "RVMInstruction.h"
#include "RVMProgram.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PExpr::rvm {

/// Register allocator using linear scan algorithm
class RVMRegisterAllocator {
public:
    /// Apply register allocation to minimize register count
    /// Returns true if any changes were made
    static bool allocate(RVMProgram& program);

    /// Get maximum register count in program (for debugging)
    static size_t getMaxRegisterCount(const RVMProgram& program);

    /// Get maximum allocated register count after allocation (for debugging)
    static size_t getAllocatedRegisterCount(const RVMProgram& program);

private:
    /// Live interval for a register
    struct LiveInterval {
        RegId originalReg = 0;
        size_t start      = 0; // Instruction index where defined
        size_t end        = 0; // Instruction index of last use

        // For linear scan
        RegId allocatedReg = 0;

        LiveInterval() = default;

        LiveInterval(RegId reg, size_t s, size_t e)
            : originalReg(reg)
            , start(s)
            , end(e)
            , allocatedReg(reg)
        {
        }
    };

    /// Analyze live ranges of all registers in program
    static std::vector<LiveInterval> analyzeLiveRanges(const RVMProgram& program);

    /// Perform linear scan allocation on intervals
    static void linearScanAllocate(std::vector<LiveInterval>& intervals);

    /// Rewrite program with new register assignments
    static void rewriteProgram(RVMProgram& program, const std::unordered_map<RegId, RegId>& regMap);

    /// Collect all registers used in program
    static std::unordered_set<RegId> collectRegisters(const RVMProgram& program);

    /// Create register mapping from intervals
    static std::unordered_map<RegId, RegId> createRegisterMap(const std::vector<LiveInterval>& intervals);
};

} // namespace PExpr::rvm