#include "RVMRegisterAllocator.h"
#include "RVMInstruction.h"
#include "RVMValue.h"

#include <algorithm>
#include <iostream>

namespace PExpr::rvm {

bool RVMRegisterAllocator::allocate(RVMProgram& program)
{
    if (program.empty())
        return false;

    // Get original max register count
    size_t originalMax = getMaxRegisterCount(program);
    if (originalMax <= 1) // Already minimal or no registers
        return false;

    // Analyze live ranges
    std::vector<LiveInterval> intervals = analyzeLiveRanges(program);
    if (intervals.empty())
        return false;

    // Perform linear scan allocation
    linearScanAllocate(intervals);

    // Create register mapping
    std::unordered_map<RegId, RegId> regMap = createRegisterMap(intervals);

    // Check if any changes needed
    bool changesNeeded = false;
    for (const auto& [oldReg, newReg] : regMap) {
        if (oldReg != newReg) {
            changesNeeded = true;
            break;
        }
    }

    if (!changesNeeded)
        return false;

    // Rewrite program with new register assignments
    rewriteProgram(program, regMap);

    return true;
}

size_t RVMRegisterAllocator::getMaxRegisterCount(const RVMProgram& program)
{
    std::unordered_set<RegId> registers = collectRegisters(program);
    if (registers.empty())
        return 0;

    RegId maxReg = 0;
    for (RegId reg : registers) {
        if (reg > maxReg)
            maxReg = reg;
    }
    return static_cast<size_t>(maxReg + 1); // +1 because registers are 0-indexed
}

size_t RVMRegisterAllocator::getAllocatedRegisterCount(const RVMProgram& program)
{
    std::unordered_set<RegId> registers = collectRegisters(program);
    return registers.size();
}

std::unordered_set<RegId> RVMRegisterAllocator::collectRegisters(const RVMProgram& program)
{
    std::unordered_set<RegId> registers;

    for (const auto& instr : program) {
        instr->forEachValue([&](const RVMValue& val) {
            if (val.isRegister())
                registers.insert(val.regId());
        });
    }

    return registers;
}

std::vector<RVMRegisterAllocator::LiveInterval> RVMRegisterAllocator::analyzeLiveRanges(const RVMProgram& program)
{
    // First pass: collect all registers and initialize intervals
    std::unordered_map<RegId, LiveInterval> intervalMap;
    std::unordered_map<RegId, size_t> lastUsePos;

    auto useReg = [&](RegId reg, size_t index) {
        lastUsePos[reg] = index; // Update last use position
        // Ensure interval exists (register might be used before defined, e.g., parameters)
        if (!intervalMap.contains(reg))
            intervalMap[reg] = LiveInterval(reg, 0, index); // Start at 0, will be updated if defined later
    };

    auto modifyReg = [&](RegId reg, size_t index) {
        if (!intervalMap.contains(reg)) // First definition
            intervalMap[reg] = LiveInterval(reg, index, index);
        else if (index < intervalMap[reg].start) //< Update start if earlier (shouldn't happen in SSA but just in case)
            intervalMap[reg].start = index;
    };

    // Track definition positions
    for (size_t i = 0; i < program.size(); ++i) {
        const auto& instr = program[i];

        // Check destination register (definition)
        instr->forDestination([&](const RVMValue& dstVal) {
            if (dstVal.isRegister()) {
                RegId reg = dstVal.regId();
                if (!intervalMap.contains(reg)) // First definition
                    intervalMap[reg] = LiveInterval(reg, i, i);
                else if (i < intervalMap[reg].start) //< Update start if earlier (shouldn't happen in SSA but just in case)
                    intervalMap[reg].start = i;
            }
        });

        // Track last use positions for all source registers
        instr->forEachSource([&](const RVMValue& srcVal) {
            if (srcVal.isRegister())
                useReg(srcVal.regId(), i);
        });

        // Return instruction is a use-position for the n-count registers
        if (auto ret = dynamic_cast<RVMInstrReturn*>(instr.get())) {
            for (RegId reg = 0; reg < ret->returnCount(); ++reg)
                useReg(reg, i);
        }

        // External call instruction uses and modifies registers
        if (auto external_call = dynamic_cast<RVMInstrExternalCall*>(instr.get())) {
            for (RegId reg = 0; reg < external_call->srcs().size(); ++reg)
                useReg(reg, i);
            if (external_call->dst().has_value())
                modifyReg(0, i);
        }

        // Internal call instruction uses and modifies registers
        if (auto internal_call = dynamic_cast<RVMInstrInternalCall*>(instr.get())) {
            for (RegId reg = 0; reg < internal_call->parameterCount(); ++reg)
                useReg(reg, i);
            for (RegId reg = 0; reg < internal_call->returnCount(); ++reg)
                modifyReg(reg, i);
        }

        // Push frame 'uses' n registers
        if (auto push_frame = dynamic_cast<RVMInstrPushFrame*>(instr.get())) {
            for (RegId reg = 0; reg < push_frame->registerCount(); ++reg)
                useReg(reg, i);
        }

        // Pop frame 'modifies' n registers
        if (auto pop_frame = dynamic_cast<RVMInstrPopFrame*>(instr.get())) {
            for (RegId reg = 0; reg < pop_frame->registerCount(); ++reg)
                modifyReg(reg, i);
        }
    }

    // Second pass: update end positions from last uses
    for (auto& [reg, interval] : intervalMap) {
        if (auto it = lastUsePos.find(reg); it != lastUsePos.end() && it->second > interval.start)
            interval.end = it->second;

        // If no use found, end at definition (dead value)
    }

    // Convert to vector
    std::vector<LiveInterval> intervals;
    intervals.reserve(intervalMap.size());
    for (auto& [reg, interval] : intervalMap)
        intervals.push_back(interval);

    // Sort by start position
    std::sort(intervals.begin(), intervals.end(),
              [](const LiveInterval& a, const LiveInterval& b) {
                  return a.start < b.start;
              });

    return intervals;
}

void RVMRegisterAllocator::linearScanAllocate(std::vector<LiveInterval>& intervals)
{
    if (intervals.empty())
        return;

    // Active intervals (sorted by end position)
    std::vector<LiveInterval*> active;

    // Next available register
    RegId nextReg = 0;

    for (auto& interval : intervals) {
        // Expire old intervals (end <= start means interval is no longer live)
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [&interval](LiveInterval* activeInterval) {
                                        return activeInterval->end <= interval.start;
                                    }),
                     active.end());

        // Sort active by end position for spill selection
        std::sort(active.begin(), active.end(),
                  [](const LiveInterval* a, const LiveInterval* b) {
                      return a->end < b->end;
                  });

        // Try to find free register
        bool allocated = false;

        // Simple strategy: use next available register
        // Check if any active interval uses this register
        bool regInUse = false;
        for (const auto* activeInterval : active) {
            if (activeInterval->allocatedReg == nextReg) {
                regInUse = true;
                break;
            }
        }

        if (!regInUse) {
            interval.allocatedReg = nextReg;
            allocated             = true;
        } else {
            // Register is in use, try to find another free one
            // Since we don't have a maximum, we can always allocate a new one
            // But we want to minimize, so try to reuse

            // Find max allocated register in active set
            RegId maxAllocated = 0;
            for (const auto* activeInterval : active) {
                if (activeInterval->allocatedReg > maxAllocated)
                    maxAllocated = activeInterval->allocatedReg;
            }

            // Try registers 0..maxAllocated+1
            for (RegId tryReg = 0; tryReg <= maxAllocated + 1; ++tryReg) {
                bool regFree = true;
                for (const auto* activeInterval : active) {
                    if (activeInterval->allocatedReg == tryReg) {
                        regFree = false;
                        break;
                    }
                }

                if (regFree) {
                    interval.allocatedReg = tryReg;
                    allocated             = true;
                    nextReg               = tryReg + 1; // Update next for future allocations
                    break;
                }
            }
        }

        if (allocated) {
            active.push_back(&interval);
        } else {
            // Should not happen since we have no register limit
            // Allocate new register
            interval.allocatedReg = nextReg++;
            active.push_back(&interval);
        }
    }
}

std::unordered_map<RegId, RegId> RVMRegisterAllocator::createRegisterMap(const std::vector<LiveInterval>& intervals)
{
    std::unordered_map<RegId, RegId> regMap;

    for (const auto& interval : intervals)
        regMap[interval.originalReg] = interval.allocatedReg;

    return regMap;
}

void RVMRegisterAllocator::rewriteProgram(RVMProgram& program, const std::unordered_map<RegId, RegId>& regMap)
{
    for (auto& instr : program) {
        // Use the instruction's visitor to rewrite values in-place
        instr->forEachValue([&regMap](RVMValue& value) {
            if (value.isRegister()) {
                RegId oldReg = value.regId();
                if (auto it = regMap.find(oldReg); it != regMap.end() && it->second != oldReg) //< Create new register value with same type
                    value = RVMValue::Register(it->second, value.type());
            }
        });
    }
}

} // namespace PExpr::rvm