#pragma once

#include "RVMBasicBlockAnalyzer.h"
#include "RVMInstruction.h"
#include "RVMLiveAnalyzer.h"
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
    [[nodiscard]] static size_t getMaxRegisterCount(const RVMProgram& program);

    /// Get maximum allocated register count after allocation (for debugging)
    [[nodiscard]] static size_t getAllocatedRegisterCount(const RVMProgram& program);

private:
    struct InternalLiveInterval {
        RVMLiveAnalyzer::LiveInterval Interval;
        RegId AllocatedRegister;
    };

    /// Perform linear scan allocation on intervals
    static void linearScanAllocate(std::vector<InternalLiveInterval>& intervals);

    /// Rewrite program with new register assignments
    static void rewriteProgram(RVMProgram& program, const std::unordered_map<RegId, RegId>& regMap);

    /// Collect all registers used in program
    static std::unordered_set<RegId> collectRegisters(const RVMProgram& program);

    /// Create register mapping from intervals
    static std::unordered_map<RegId, RegId> createRegisterMap(const std::vector<InternalLiveInterval>& intervals);
};

} // namespace PExpr::rvm