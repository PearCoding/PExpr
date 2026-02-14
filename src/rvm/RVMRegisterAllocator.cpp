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

    // Analyze live ranges using the live analyzer for the entire program at once
    auto intervals = RVMLiveAnalyzer::analyzeBlock(program);
    if (intervals.empty())
        return false;

    // Map to internal representation
    std::vector<InternalLiveInterval> internalIntervals;
    internalIntervals.reserve(intervals.size());
    for (const auto& interval : intervals)
        internalIntervals.push_back(InternalLiveInterval{ .Interval = interval, .AllocatedRegister = interval.reg });

    // Perform linear scan allocation
    linearScanAllocate(internalIntervals);

    // Create register mapping
    std::unordered_map<RegId, RegId> regMap = createRegisterMap(internalIntervals);

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

void RVMRegisterAllocator::linearScanAllocate(std::vector<InternalLiveInterval>& intervals)
{
    if (intervals.empty())
        return;

    // Active intervals (sorted by end position)
    std::vector<InternalLiveInterval*> active;

    // Next available register
    RegId nextReg = 0;

    for (auto& interval : intervals) {
        // Expire old intervals (end <= start means interval is no longer live)
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [&interval](InternalLiveInterval* activeInterval) {
                                        return activeInterval->Interval.end <= interval.Interval.start;
                                    }),
                     active.end());

        // Sort active by end position for spill selection
        std::sort(active.begin(), active.end(),
                  [](const InternalLiveInterval* a, const InternalLiveInterval* b) {
                      return a->Interval.end < b->Interval.end;
                  });

        // Try to find free register
        bool allocated = false;

        // Simple strategy: use next available register
        // Check if any active interval uses this register
        bool regInUse = false;
        for (const auto* activeInterval : active) {
            if (activeInterval->AllocatedRegister == nextReg) {
                regInUse = true;
                break;
            }
        }

        if (!regInUse) {
            interval.AllocatedRegister = nextReg;
            allocated                  = true;
        } else {
            // Register is in use, try to find another free one
            // Since we don't have a maximum, we can always allocate a new one
            // But we want to minimize, so try to reuse

            // Find max allocated register in active set
            RegId maxAllocated = 0;
            for (const auto* activeInterval : active) {
                if (activeInterval->AllocatedRegister > maxAllocated)
                    maxAllocated = activeInterval->AllocatedRegister;
            }

            // Try registers 0..maxAllocated+1
            for (RegId tryReg = 0; tryReg <= maxAllocated + 1; ++tryReg) {
                bool regFree = true;
                for (const auto* activeInterval : active) {
                    if (activeInterval->AllocatedRegister == tryReg) {
                        regFree = false;
                        break;
                    }
                }

                if (regFree) {
                    interval.AllocatedRegister = tryReg;
                    allocated                  = true;
                    nextReg                    = tryReg + 1; // Update next for future allocations
                    break;
                }
            }
        }

        if (allocated) {
            active.push_back(&interval);
        } else {
            // Should not happen since we have no register limit
            // Allocate new register
            interval.AllocatedRegister = nextReg++;
            active.push_back(&interval);
        }
    }
}

std::unordered_map<RegId, RegId> RVMRegisterAllocator::createRegisterMap(const std::vector<InternalLiveInterval>& intervals)
{
    std::unordered_map<RegId, RegId> regMap;

    for (const auto& interval : intervals)
        regMap[interval.Interval.reg] = interval.AllocatedRegister;

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