#include "RVMLiveAnalyzer.h"
#include "RVMInstruction.h"

#include <algorithm>

namespace PExpr::rvm {

std::vector<RVMLiveAnalyzer::LiveInterval> RVMLiveAnalyzer::analyzeBlock(const std::vector<std::shared_ptr<RVMInstr>>& block)
{
    if (block.empty())
        return {};

    // First pass: collect all registers and initialize intervals
    std::unordered_map<RegId, LiveInterval> activeIntervals;
    std::vector<LiveInterval> allIntervals;

    // Process each instruction
    for (size_t i = 0; i < block.size(); ++i)
        processInstruction(block[i], i, activeIntervals, allIntervals);

    // Add all dangling intervals to the list
    for (auto p : activeIntervals)
        allIntervals.push_back(p.second);

    // Sort by start position
    std::sort(allIntervals.begin(), allIntervals.end(),
              [](const LiveInterval& a, const LiveInterval& b) {
                  return a.Start < b.Start;
              });

    return allIntervals;
}

void RVMLiveAnalyzer::processInstruction(
    const std::shared_ptr<RVMInstr>& instr,
    size_t index,
    std::unordered_map<RegId, LiveInterval>& activeIntervals,
    std::vector<LiveInterval>& allIntervals)
{
    // Check destination register (definition)
    instr->forDestination([&](const RVMValue& dstVal) {
        if (dstVal.isRegister()) {
            RegId reg = dstVal.regId();
            processRegDef(reg, index, activeIntervals, allIntervals, false);
        }
    });

    // Track last use positions for all source registers
    instr->forEachSource([&](const RVMValue& srcVal) {
        if (srcVal.isRegister()) {
            RegId reg = srcVal.regId();
            processRegUse(reg, index, activeIntervals, false);
        }
    });

    // Return instruction is a use-position for the n-count registers
    if (auto ret = dynamic_cast<RVMInstrReturn*>(instr.get())) {
        for (RegId reg = 0; reg < ret->returnCount(); ++reg)
            processRegUse(reg, index, activeIntervals, true);
    }

    // Call instruction uses and modifies registers
    if (auto call = dynamic_cast<RVMInstrCall*>(instr.get())) {
        for (RegId reg = 0; reg < call->parameterCount(); ++reg)
            processRegUse(reg, index, activeIntervals, true);
        for (RegId reg = 0; reg < call->returnCount(); ++reg)
            processRegDef(reg, index, activeIntervals, allIntervals, true);
    }
}

void RVMLiveAnalyzer::processRegUse(RegId reg, size_t index,
                                    std::unordered_map<RegId, LiveInterval>& activeIntervals,
                                    bool pin)
{
    if (auto it = activeIntervals.find(reg); it != activeIntervals.end()) {
        it->second.End = std::max(it->second.End, index); //< Update end of the interval
        it->second.HasPinned |= pin;
    } else {
        // Register used before definition (e.g., function parameter)
        // Create an interval that starts at 0 (block beginning)
        // When a definition comes later, it will create a new interval
        activeIntervals[reg] = LiveInterval(reg, 0, index, pin);
    }
}

void RVMLiveAnalyzer::processRegDef(RegId reg, size_t index,
                                    std::unordered_map<RegId, LiveInterval>& activeIntervals,
                                    std::vector<LiveInterval>& allIntervals,
                                    bool pin)
{
    if (auto it = activeIntervals.find(reg); it != activeIntervals.end())
        allIntervals.push_back(it->second);

    activeIntervals[reg] = LiveInterval(reg, index, index, pin);
}

} // namespace PExpr::rvm