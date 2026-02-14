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
                  return a.start < b.start;
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
            processRegDef(reg, index, activeIntervals, allIntervals);
        }
    });

    // Track last use positions for all source registers
    instr->forEachSource([&](const RVMValue& srcVal) {
        if (srcVal.isRegister()) {
            RegId reg = srcVal.regId();
            processRegUse(reg, index, activeIntervals);
        }
    });

    // Return instruction is a use-position for the n-count registers
    if (auto ret = dynamic_cast<RVMInstrReturn*>(instr.get())) {
        for (RegId reg = 0; reg < ret->returnCount(); ++reg)
            processRegUse(reg, index, activeIntervals);
    }

    // External call instruction uses and modifies registers
    if (auto external_call = dynamic_cast<RVMInstrExternalCall*>(instr.get())) {
        for (RegId reg = 0; reg < external_call->srcs().size(); ++reg)
            processRegUse(reg, index, activeIntervals);
        if (external_call->dst().has_value())
            processRegDef(0, index, activeIntervals, allIntervals);
    }

    // Internal call instruction uses and modifies registers
    if (auto internal_call = dynamic_cast<RVMInstrInternalCall*>(instr.get())) {
        for (RegId reg = 0; reg < internal_call->parameterCount(); ++reg)
            processRegUse(reg, index, activeIntervals);
        for (RegId reg = 0; reg < internal_call->returnCount(); ++reg)
            processRegDef(reg, index, activeIntervals, allIntervals);
    }

    // Push frame 'uses' n registers
    if (auto push_frame = dynamic_cast<RVMInstrPushFrame*>(instr.get())) {
        for (RegId reg = 0; reg < push_frame->registerCount(); ++reg)
            processRegUse(reg, index, activeIntervals);
    }

    // Pop frame 'modifies' n registers
    if (auto pop_frame = dynamic_cast<RVMInstrPopFrame*>(instr.get())) {
        for (RegId reg = 0; reg < pop_frame->registerCount(); ++reg)
            processRegDef(reg, index, activeIntervals, allIntervals);
    }
}

void RVMLiveAnalyzer::processRegUse(RegId reg, size_t index,
                                    std::unordered_map<RegId, LiveInterval>& activeIntervals)
{
    if (auto it = activeIntervals.find(reg); it != activeIntervals.end()) {
        it->second.end = std::max(it->second.end, index);   //< Update end of the interval
    } else {
        // Register used before definition (e.g., function parameter)
        // Create an interval that starts at 0 (block beginning)
        // When a definition comes later, it will create a new interval
        activeIntervals[reg] = LiveInterval(reg, 0, index);
    }
}

void RVMLiveAnalyzer::processRegDef(RegId reg, size_t index,
                                    std::unordered_map<RegId, LiveInterval>& activeIntervals,
                                    std::vector<LiveInterval>& allIntervals)
{
    if (auto it = activeIntervals.find(reg); it != activeIntervals.end())
        allIntervals.push_back(it->second);

    activeIntervals[reg] = LiveInterval(reg, index, index);
}

} // namespace PExpr::rvm