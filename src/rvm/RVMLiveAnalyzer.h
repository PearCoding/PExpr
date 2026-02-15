#pragma once

#include "RVMInstruction.h"

#include <unordered_map>
#include <vector>

namespace PExpr::rvm {

/// Analyzes live intervals within a list of instructions
class RVMLiveAnalyzer {
public:
    /// Live interval of a register
    struct LiveInterval {
        RegId Register = 0;
        size_t Start   = 0;     // Instruction index where defined (or block start if live-in)
        size_t End     = 0;     // Instruction index of last use
        bool HasPinned = false; // Is the register inside this interval pinned?

        LiveInterval() = default;
        LiveInterval(RegId r, size_t s, size_t e, bool pinned)
            : Register(r)
            , Start(s)
            , End(e)
            , HasPinned(pinned)
        {
        }

        /// Returns true when the interval is not used and is not associated with a pinned register
        [[nodiscard]] inline bool isRedundant() const { return Start == End && !HasPinned; }
    };

    /// Analyze live intervals for registers in a block.
    /// A live interval is defined by its first definition (or live-in) and it's last use.
    /// There might be multiple intervals for a single register as each new definition starts a new interval.
    /// Returns a vector of intervals sorted by start position
    static std::vector<LiveInterval> analyzeBlock(const std::vector<std::shared_ptr<RVMInstr>>& block);

private:
    /// Process a single instruction for live interval analysis
    /// Updates intervalMap and lastUsePos based on instruction's register usage
    static void processInstruction(const std::shared_ptr<RVMInstr>& instr, size_t index,
                                   std::unordered_map<RegId, LiveInterval>& activeIntervals,
                                   std::vector<LiveInterval>& allIntervals);

    /// Process register use (read)
    static void processRegUse(RegId reg, size_t index,
                              std::unordered_map<RegId, LiveInterval>& activeIntervals,
                              bool pin);

    /// Process register definition (write)
    static void processRegDef(RegId reg, size_t index,
                              std::unordered_map<RegId, LiveInterval>& activeIntervals,
                              std::vector<LiveInterval>& allIntervals,
                              bool pin);
};

} // namespace PExpr::rvm