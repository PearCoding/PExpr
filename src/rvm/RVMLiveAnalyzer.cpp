#include "RVMLiveAnalyzer.h"
#include "RVMBasicBlockAnalyzer.h"
#include "RVMInstruction.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

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

std::vector<RVMLiveAnalyzer::LiveInterval> RVMLiveAnalyzer::analyzeProgram(const RVMProgram& program)
{
    if (program.empty())
        return {};

    // Split program into basic blocks
    auto blocks = RVMBasicBlockAnalyzer::splitIntoBlocks(program);
    if (blocks.empty())
        return {};

    // Build control flow graph
    auto predecessorMap = RVMBasicBlockAnalyzer::buildPredecessorMap(blocks);

    // Map block index to its start instruction index in the global program
    std::vector<size_t> blockStartIndices;
    size_t currentIndex = 0;
    for (const auto& block : blocks) {
        blockStartIndices.push_back(currentIndex);
        currentIndex += block.size();
    }

    // Initialize live-in and live-out sets for each block
    std::vector<std::unordered_set<RegId>> liveIn(blocks.size());
    std::vector<std::unordered_set<RegId>> liveOut(blocks.size());
    std::vector<std::unordered_set<RegId>> defSets(blocks.size());
    std::vector<std::unordered_set<RegId>> useSets(blocks.size());

    // First pass: compute use and def sets for each block
    for (size_t blockIdx = 0; blockIdx < blocks.size(); ++blockIdx) {
        const auto& block = blocks[blockIdx];
        std::unordered_set<RegId> blockUses;
        std::unordered_set<RegId> blockDefs;

        // Analyze block to find uses and definitions
        for (size_t instrIdx = 0; instrIdx < block.size(); ++instrIdx) {
            const auto& instr = block[instrIdx];

            // Check for register uses (reads)
            instr->forEachSource([&](const RVMValue& srcVal) {
                if (srcVal.isRegister()) {
                    RegId reg = srcVal.regId();
                    // If not defined earlier in this block, it's a use
                    if (blockDefs.find(reg) == blockDefs.end())
                        blockUses.insert(reg);
                }
            });

            // Check for register definitions (writes)
            instr->forDestination([&](const RVMValue& dstVal) {
                if (dstVal.isRegister()) {
                    RegId reg = dstVal.regId();
                    blockDefs.insert(reg);
                }
            });

            // Handle call and return instructions specially
            if (auto ret = dynamic_cast<const RVMInstrReturn*>(instr.get())) {
                // Return uses registers r0..rN-1
                for (RegId reg = 0; reg < ret->returnCount(); ++reg) {
                    if (blockDefs.find(reg) == blockDefs.end())
                        blockUses.insert(reg);
                }
            }

            if (auto call = dynamic_cast<const RVMInstrCall*>(instr.get())) {
                // Call uses parameter registers r0..rN-1
                for (RegId reg = 0; reg < call->parameterCount(); ++reg) {
                    if (blockDefs.find(reg) == blockDefs.end())
                        blockUses.insert(reg);
                }
                // Call defines return registers r0..rN-1
                for (RegId reg = 0; reg < call->returnCount(); ++reg)
                    blockDefs.insert(reg);
            }
        }

        useSets[blockIdx] = blockUses;
        defSets[blockIdx] = blockDefs;
    }

    // Iterative dataflow analysis to compute live-in and live-out sets
    bool changed;
    do {
        changed = false;

        for (size_t blockIdx = 0; blockIdx < blocks.size(); ++blockIdx) {
            // liveOut[block] = union of liveIn[successor] for all successors
            std::unordered_set<RegId> newLiveOut;

            // Get successors of this block
            auto successors = RVMBasicBlockAnalyzer::getSuccessors(blocks[blockIdx]);

            // For each successor label, find the corresponding block index
            for (const auto& label : successors) {
                // Find block with this label
                for (size_t succIdx = 0; succIdx < blocks.size(); ++succIdx) {
                    if (!blocks[succIdx].empty()) {
                        if (auto* labelInstr = dynamic_cast<const RVMInstrLabel*>(blocks[succIdx][0].get())) {
                            if (labelInstr->labelName() == label) {
                                // Add all registers live at the start of successor block
                                for (RegId reg : liveIn[succIdx])
                                    newLiveOut.insert(reg);
                                break;
                            }
                        }
                    }
                }
            }

            // Also handle fall-through (if block doesn't end with control flow)
            if (!blocks[blockIdx].empty() && !RVMBasicBlockAnalyzer::endsBlock(blocks[blockIdx].back().get())) {
                if (blockIdx + 1 < blocks.size()) {
                    // Next block is a successor
                    for (RegId reg : liveIn[blockIdx + 1])
                        newLiveOut.insert(reg);
                }
            }

            // liveIn[block] = use[block] ∪ (liveOut[block] - def[block])
            std::unordered_set<RegId> newLiveIn = useSets[blockIdx];
            for (RegId reg : newLiveOut) {
                if (defSets[blockIdx].find(reg) == defSets[blockIdx].end())
                    newLiveIn.insert(reg);
            }

            // Check for changes
            if (newLiveIn != liveIn[blockIdx] || newLiveOut != liveOut[blockIdx]) {
                liveIn[blockIdx]  = std::move(newLiveIn);
                liveOut[blockIdx] = std::move(newLiveOut);
                changed           = true;
            }
        }
    } while (changed);

    // Now build global live intervals
    // We need to track intervals across blocks
    std::unordered_map<RegId, LiveInterval> activeIntervals;
    std::vector<LiveInterval> allIntervals;

    // Process all instructions in order with global indices
    size_t globalIndex = 0;
    for (size_t blockIdx = 0; blockIdx < blocks.size(); ++blockIdx) {
        const auto& block = blocks[blockIdx];

        // Registers live at block entry extend their intervals
        for (RegId reg : liveIn[blockIdx]) {
            if (auto it = activeIntervals.find(reg); it != activeIntervals.end()) //< Extend existing interval
                it->second.End = std::max(it->second.End, globalIndex);
            else //< Start new interval at block entry
                activeIntervals[reg] = LiveInterval(reg, globalIndex, globalIndex, false);
        }

        // Process instructions in this block
        for (size_t instrIdx = 0; instrIdx < block.size(); ++instrIdx, ++globalIndex) {
            const auto& instr = block[instrIdx];
            processInstruction(instr, globalIndex, activeIntervals, allIntervals);
        }

        // Registers live at block exit should have their intervals extended
        // to the start of successor blocks
        for (RegId reg : liveOut[blockIdx]) {
            if (auto it = activeIntervals.find(reg); it != activeIntervals.end()) {
                // Interval continues to next block
                // We'll extend it when we process the successor block
            } else {
                // Should not happen: register live out but not live in?
                PEXPR_ASSERT(false, "Register live out but not live in");
                activeIntervals[reg] = LiveInterval(reg, globalIndex, globalIndex, false);
            }
        }

        // End intervals for registers not live out
        std::vector<RegId> toRemove;
        for (const auto& [reg, interval] : activeIntervals) {
            if (liveOut[blockIdx].find(reg) == liveOut[blockIdx].end()) {
                // Register not live out of this block, end its interval
                allIntervals.push_back(interval);
                toRemove.push_back(reg);
            }
        }
        for (RegId reg : toRemove)
            activeIntervals.erase(reg);
    }

    // Add any remaining intervals
    for (const auto& [reg, interval] : activeIntervals)
        allIntervals.push_back(interval);

    // Sort by start position
    std::sort(allIntervals.begin(), allIntervals.end(),
              [](const LiveInterval& a, const LiveInterval& b) {
                  return a.Start < b.Start;
              });

    return allIntervals;
}

} // namespace PExpr::rvm
