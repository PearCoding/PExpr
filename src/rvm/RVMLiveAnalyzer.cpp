#include "RVMLiveAnalyzer.h"
#include "RVMBasicBlockAnalyzer.h"
#include "RVMInstruction.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <queue>
#include <unordered_set>

namespace PExpr::rvm {

/// Internal structure to track a liveness segment within a basic block
struct LivenessSegment : public RVMLiveAnalyzer::LiveInterval {
    size_t BlockIndex       = 0;
    bool StartsAtBlockEntry = false;
    bool EndsAtBlockExit    = false;

    LivenessSegment() = default;
    LivenessSegment(RegId r, size_t s, size_t e, bool pStart, bool pEnd, bool nm, size_t bIdx, bool entries, bool exits)
        : LiveInterval(r, s, e, pStart, pEnd, nm)
        , BlockIndex(bIdx)
        , StartsAtBlockEntry(entries)
        , EndsAtBlockExit(exits)
    {
    }
};

/// Simple Disjoint Set Union for merging segments
struct DSU {
    std::vector<size_t> parent;
    DSU(size_t n)
    {
        parent.resize(n);
        std::iota(parent.begin(), parent.end(), 0);
    }
    size_t find(size_t i)
    {
        if (parent[i] == i)
            return i;
        return parent[i] = find(parent[i]);
    }
    void unite(size_t i, size_t j)
    {
        size_t root_i = find(i);
        size_t root_j = find(j);
        if (root_i != root_j)
            parent[root_i] = root_j;
    }
};

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

    // Add all dangling intervals to the list (they go until the end of the block)
    for (auto p : activeIntervals) {
        p.second.End = block.size() - 1;
        allIntervals.push_back(p.second);
    }

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
    bool isNonMove = instr->opcode() != Opcode::MOV;

    // A call or return pins its registers according to the calling convention
    bool pins = (instr->opcode() == Opcode::CALL_EXTERNAL || instr->opcode() == Opcode::CALL_INTERNAL || instr->opcode() == Opcode::RET);

    // Check destination register (definition)
    instr->forEachDestination([&](const RVMValue& dstVal) {
        if (dstVal.isRegister()) {
            RegId reg = dstVal.regId();
            processRegDef(reg, index, activeIntervals, allIntervals, pins);
            if (isNonMove)
                activeIntervals[reg].HasNonMoveUsage = true;
        }
    });

    // Track last use positions for all source registers
    instr->forEachSource([&](const RVMValue& srcVal) {
        if (srcVal.isRegister()) {
            RegId reg = srcVal.regId();
            processRegUse(reg, index, activeIntervals, pins);
            if (isNonMove) {
                if (auto it = activeIntervals.find(reg); it != activeIntervals.end())
                    it->second.HasNonMoveUsage = true;
            }
        }
    });
}

void RVMLiveAnalyzer::processRegUse(RegId reg, size_t index,
                                    std::unordered_map<RegId, LiveInterval>& activeIntervals,
                                    bool pin)
{
    if (auto it = activeIntervals.find(reg); it != activeIntervals.end()) {
        it->second.End = std::max(it->second.End, index); //< Update end of the interval
        if (pin)
            it->second.PinnedEnd = true;
    } else {
        // Register used before definition (e.g., function parameter or live-in)
        // Create an interval that starts at 0 (block beginning)
        activeIntervals[reg] = LiveInterval(reg, 0, index, false, pin, false);
    }
}

void RVMLiveAnalyzer::processRegDef(RegId reg, size_t index,
                                    std::unordered_map<RegId, LiveInterval>& activeIntervals,
                                    std::vector<LiveInterval>& allIntervals,
                                    bool pin)
{
    if (auto it = activeIntervals.find(reg); it != activeIntervals.end())
        allIntervals.push_back(it->second);

    activeIntervals[reg] = LiveInterval(reg, index, index, pin, false, false);
}

std::vector<RVMLiveAnalyzer::LiveInterval> RVMLiveAnalyzer::analyzeProgram(const RVMProgram& program)
{
    if (program.empty())
        return {};

    // Split program into basic blocks
    auto blocks = RVMBasicBlockAnalyzer::splitIntoBlocks(program);
    if (blocks.empty())
        return {};

    // Map block index to its start instruction index in the global program
    std::vector<size_t> blockStartIndices;
    size_t currentIndex = 0;
    for (const auto& block : blocks) {
        blockStartIndices.push_back(currentIndex);
        currentIndex += block.size();
    }

    // Initialize live-in and live-out sets for each block using dataflow analysis
    std::vector<std::unordered_set<RegId>> liveIn(blocks.size());
    std::vector<std::unordered_set<RegId>> liveOut(blocks.size());
    std::vector<std::unordered_set<RegId>> defSets(blocks.size());
    std::vector<std::unordered_set<RegId>> useSets(blocks.size());

    // First pass: compute use and def sets for each block
    for (size_t blockIdx = 0; blockIdx < blocks.size(); ++blockIdx) {
        const auto& block = blocks[blockIdx];
        std::unordered_set<RegId> blockUses;
        std::unordered_set<RegId> blockDefs;

        for (const auto& instr : block) {
            instr->forEachSource([&](const RVMValue& srcVal) {
                if (srcVal.isRegister()) {
                    RegId reg = srcVal.regId();
                    if (blockDefs.find(reg) == blockDefs.end())
                        blockUses.insert(reg);
                }
            });
            instr->forEachDestination([&](const RVMValue& dstVal) {
                if (dstVal.isRegister())
                    blockDefs.insert(dstVal.regId());
            });
        }
        useSets[blockIdx] = blockUses;
        defSets[blockIdx] = blockDefs;
    }

    // Iterative dataflow analysis
    bool changed;
    do {
        changed = false;
        for (int blockIdx = static_cast<int>(blocks.size()) - 1; blockIdx >= 0; --blockIdx) {
            std::unordered_set<RegId> newLiveOut;
            auto successors = RVMBasicBlockAnalyzer::getSuccessors(blocks[blockIdx]);
            for (const auto& label : successors) {
                for (size_t succIdx = 0; succIdx < blocks.size(); ++succIdx) {
                    if (!blocks[succIdx].empty()) {
                        if (auto* labelInstr = dynamic_cast<const RVMInstrLabel*>(blocks[succIdx][0].get())) {
                            if (labelInstr->labelName() == label) {
                                for (RegId reg : liveIn[succIdx])
                                    newLiveOut.insert(reg);
                                break;
                            }
                        }
                    }
                }
            }
            bool canFallThrough = true;
            if (!blocks[blockIdx].empty()) {
                auto* last = blocks[blockIdx].back().get();
                if (dynamic_cast<const RVMInstrJump*>(last) || dynamic_cast<const RVMInstrReturn*>(last))
                    canFallThrough = false;
            }
            if (canFallThrough && static_cast<size_t>(blockIdx) + 1 < blocks.size()) {
                for (RegId reg : liveIn[blockIdx + 1])
                    newLiveOut.insert(reg);
            }

            std::unordered_set<RegId> newLiveIn = useSets[blockIdx];
            for (RegId reg : newLiveOut) {
                if (defSets[blockIdx].find(reg) == defSets[blockIdx].end())
                    newLiveIn.insert(reg);
            }

            if (newLiveIn != liveIn[blockIdx] || newLiveOut != liveOut[blockIdx]) {
                liveIn[blockIdx]  = std::move(newLiveIn);
                liveOut[blockIdx] = std::move(newLiveOut);
                changed           = true;
            }
        }
    } while (changed);

    // Build segments for each block
    std::vector<std::vector<LivenessSegment>> blockSegments(blocks.size());
    size_t totalSegments = 0;

    for (size_t blockIdx = 0; blockIdx < blocks.size(); ++blockIdx) {
        const auto& block = blocks[blockIdx];
        size_t blockStart = blockStartIndices[blockIdx];
        std::unordered_map<RegId, LiveInterval> activeIntervals;
        std::vector<LiveInterval> completedIntervals;

        // If register is live-in, it starts at block entry
        for (RegId reg : liveIn[blockIdx])
            activeIntervals[reg] = LiveInterval(reg, 0, 0, false, false, false);

        // Process instructions
        for (size_t i = 0; i < block.size(); ++i)
            processInstruction(block[i], i, activeIntervals, completedIntervals);

        // Handle dangling intervals
        for (auto& [reg, interval] : activeIntervals) {
            bool isLiveOut     = liveOut[blockIdx].find(reg) != liveOut[blockIdx].end();
            size_t endIdx      = isLiveOut ? (block.size() - 1) : interval.End;
            bool startsAtEntry = (interval.Start == 0 && liveIn[blockIdx].find(reg) != liveIn[blockIdx].end());

            blockSegments[blockIdx].emplace_back(
                reg,
                interval.Start + blockStart,
                endIdx + blockStart,
                interval.PinnedStart,
                interval.PinnedEnd,
                interval.HasNonMoveUsage,
                blockIdx,
                startsAtEntry,
                isLiveOut);
        }

        // Handle completed intervals (those that were redefined)
        for (auto& interval : completedIntervals) {
            bool startsAtEntry = (interval.Start == 0 && liveIn[blockIdx].find(interval.Register) != liveIn[blockIdx].end());
            blockSegments[blockIdx].emplace_back(
                interval.Register,
                interval.Start + blockStart,
                interval.End + blockStart,
                interval.PinnedStart,
                interval.PinnedEnd,
                interval.HasNonMoveUsage,
                blockIdx,
                startsAtEntry,
                false // Completed segments within a block never reach the exit
            );
        }
        totalSegments += blockSegments[blockIdx].size();
    }

    // Merge segments using DSU
    DSU dsu(totalSegments);
    std::vector<size_t> segmentOffsets(blocks.size() + 1, 0);
    for (size_t i = 0; i < blocks.size(); ++i)
        segmentOffsets[i + 1] = segmentOffsets[i] + blockSegments[i].size();

    auto getSegmentIdx = [&](size_t blockIdx, size_t localSegIdx) { return segmentOffsets[blockIdx] + localSegIdx; };

    for (size_t blockIdx = 0; blockIdx < blocks.size(); ++blockIdx) {
        auto successors = RVMBasicBlockAnalyzer::getSuccessors(blocks[blockIdx]);

        // Handle explicit successors (jumps/branches)
        std::unordered_set<size_t> succIndices;
        for (const auto& label : successors) {
            for (size_t s = 0; s < blocks.size(); ++s) {
                if (!blocks[s].empty()) {
                    if (auto* lb = dynamic_cast<const RVMInstrLabel*>(blocks[s][0].get())) {
                        if (lb->labelName() == label) {
                            succIndices.insert(s);
                            break;
                        }
                    }
                }
            }
        }
        // Handle fall-through
        bool canFallThrough = true;
        if (!blocks[blockIdx].empty()) {
            auto* last = blocks[blockIdx].back().get();
            if (dynamic_cast<const RVMInstrJump*>(last) || dynamic_cast<const RVMInstrReturn*>(last))
                canFallThrough = false;
        }
        if (canFallThrough && blockIdx + 1 < blocks.size())
            succIndices.insert(blockIdx + 1);

        for (size_t succIdx : succIndices) {
            for (size_t i = 0; i < blockSegments[blockIdx].size(); ++i) {
                const auto& segA = blockSegments[blockIdx][i];
                if (!segA.EndsAtBlockExit)
                    continue;

                for (size_t j = 0; j < blockSegments[succIdx].size(); ++j) {
                    const auto& segB = blockSegments[succIdx][j];
                    if (segB.Register == segA.Register && segB.StartsAtBlockEntry)
                        dsu.unite(getSegmentIdx(blockIdx, i), getSegmentIdx(succIdx, j));
                }
            }
        }
    }

    // Build final intervals from DSU components
    std::map<size_t, LiveInterval> mergedMap;
    std::vector<LivenessSegment> allFlatSegments;
    for (const auto& vec : blockSegments)
        allFlatSegments.insert(allFlatSegments.end(), vec.begin(), vec.end());

    for (size_t i = 0; i < totalSegments; ++i) {
        size_t root     = dsu.find(i);
        const auto& seg = allFlatSegments[i];
        if (mergedMap.find(root) == mergedMap.end()) {
            mergedMap[root] = seg;
        } else {
            auto& merged = mergedMap[root];
            merged.Start = std::min(merged.Start, seg.Start);
            merged.End   = std::max(merged.End, seg.End);
            merged.PinnedStart |= seg.PinnedStart;
            merged.PinnedEnd |= seg.PinnedEnd;
            merged.HasNonMoveUsage |= seg.HasNonMoveUsage;
        }
    }

    std::vector<LiveInterval> result;
    for (auto const& [idx, interval] : mergedMap)
        result.push_back(interval);

    std::sort(result.begin(), result.end(), [](const LiveInterval& a, const LiveInterval& b) {
        if (a.Start != b.Start)
            return a.Start < b.Start;
        return a.Register < b.Register;
    });

    return result;
}

} // namespace PExpr::rvm
