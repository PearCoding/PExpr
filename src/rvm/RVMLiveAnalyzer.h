#pragma once

#include "RVMInstruction.h"
#include "RVMProgram.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PExpr::rvm {

class RVMBasicBlockAnalyzer;

/// Analyzes live intervals within a list of instructions
class RVMLiveAnalyzer {
public:
    /// Live interval of a register
    struct LiveInterval {
        RegId Register            = 0;
        size_t Start              = 0;     // Instruction index where defined (or block start if live-in)
        size_t End                = 0;     // Instruction index of last use
        bool PinnedStart          = false; // Is the start of the interval pinned (e.g., return value of a call)?
        bool PinnedEnd            = false; // Is the end of the interval pinned (e.g., parameter of a call)?
        bool HasNonMoveUsage      = false; // Does the interval contain any non-move instruction usage?

        LiveInterval() = default;
        LiveInterval(RegId r, size_t s, size_t e, bool pStart, bool pEnd, bool nonMove)
            : Register(r)
            , Start(s)
            , End(e)
            , PinnedStart(pStart)
            , PinnedEnd(pEnd)
            , HasNonMoveUsage(nonMove)
        {
        }

        /// Returns true when the interval is not used and is not associated with a pinned register
        [[nodiscard]] inline bool isRedundant() const { return Start == End && !PinnedStart && !PinnedEnd; }
        /// Returns true when the interval is pinned (either start or end)
        [[nodiscard]] inline bool isPinned() const { return PinnedStart || PinnedEnd; }
    };

    /// Analyze live intervals for registers in a block.
    /// A live interval is defined by its first definition (or live-in) and it's last use.
    /// There might be multiple intervals for a single register as each new definition starts a new interval.
    /// Returns a vector of intervals sorted by start position
    static std::vector<LiveInterval> analyzeBlock(const std::vector<std::shared_ptr<RVMInstr>>& block);

    /// Analyze live intervals for registers in an entire program with control flow.
    /// This performs global liveness analysis across basic blocks, respecting jumps,
    /// branches, calls, and returns.
    /// Returns a vector of intervals sorted by start position (global instruction index)
    static std::vector<LiveInterval> analyzeProgram(const RVMProgram& program);

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