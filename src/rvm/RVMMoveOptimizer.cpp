#include "RVMMoveOptimizer.h"
#include "RVMBasicBlockAnalyzer.h"
#include "RVMInstruction.h"
#include "RVMLiveAnalyzer.h"
#include "RVMValue.h"
#include "utils/Reporter.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PExpr::rvm {

bool RVMMoveOptimizer::optimize(const opt::OptimizerOptions& options, RVMProgram& program)
{
    // Early exit if no move optimizations are enabled
    if (!options.OptimizeIdentityMoves && !options.OptimizeMoveChains && !options.OptimizeRedundantMoves)
        return false;

    return optimizeProgram(options, program);
}

bool RVMMoveOptimizer::optimizeProgram(const opt::OptimizerOptions& options, RVMProgram& program)
{
    if (program.empty())
        return false;

    bool changed = false;

    // Step 1: Identity move elimination pass
    if (options.OptimizeIdentityMoves)
        changed |= runIdentityMovePass(program);

    // Step 2: Move chain collapsing pass
    if (options.OptimizeMoveChains)
        changed |= runMoveChainPass(program);

    // Step 3: Redundant move elimination pass
    if (options.OptimizeRedundantMoves)
        changed |= runRedundantMovePass(program);

    return changed;
}

bool RVMMoveOptimizer::runIdentityMovePass(RVMProgram& program)
{
    auto blocks = RVMBasicBlockAnalyzer::splitIntoBlocks(program);
    if (blocks.empty())
        return false;

    bool changed = false;
    for (size_t blockIdx = 0; blockIdx < blocks.size(); ++blockIdx) {
        bool blockChanged = false;
        removeIdentityMoves(blocks[blockIdx], blockChanged);
        changed |= blockChanged;
    }

    if (changed) {
        program.clear();
        for (const auto& block : blocks)
            program.insert(program.end(), block.begin(), block.end());
    }

    return changed;
}

bool RVMMoveOptimizer::runMoveChainPass(RVMProgram& program)
{
    // Reanalyze intervals for the current updated program
    auto workingIntervals = RVMLiveAnalyzer::analyzeProgram(program);

    // Split program into basic blocks
    auto mutableBlocks = RVMBasicBlockAnalyzer::splitIntoBlocks(program);
    if (mutableBlocks.empty())
        return false;

    // Calculate block start indices
    std::vector<size_t> blockStartIndices;
    size_t currentIndex = 0;
    for (const auto& block : mutableBlocks) {
        blockStartIndices.push_back(currentIndex);
        currentIndex += block.size();
    }

    // For each live interval that starts with a MOV instruction
    // we need to check if it can be collapsed into its source interval

    // First, build a map from global index to the instruction
    // and identify which intervals start with MOV instructions
    std::unordered_map<size_t, std::pair<size_t, size_t>> indexToBlockLocal; // global index -> (blockIdx, localIdx)
    std::unordered_map<size_t, const RVMLiveAnalyzer::LiveInterval*> intervalByStart;
    std::unordered_map<size_t, RegId> movSourceAtStart; // global index where MOV defines -> source register

    for (size_t i = 0; i < workingIntervals.size(); ++i) {
        const auto& interval            = workingIntervals[i];
        intervalByStart[interval.Start] = &interval;
    }

    // Collect MOV instructions and their sources
    for (size_t blockIdx = 0; blockIdx < mutableBlocks.size(); ++blockIdx) {
        const auto& block = mutableBlocks[blockIdx];
        size_t blockStart = blockStartIndices[blockIdx];

        for (size_t localIdx = 0; localIdx < block.size(); ++localIdx) {
            const auto& instr            = block[localIdx];
            size_t globalIdx             = blockStart + localIdx;
            indexToBlockLocal[globalIdx] = { blockIdx, localIdx };

            if (isMovInstruction(instr.get())) {
                auto* movInstr = static_cast<const RVMInstr2Op*>(instr.get());
                if (!isIdentityMov(movInstr)) {
                    const RVMValue& src = movInstr->source();
                    if (src.isRegister())
                        movSourceAtStart[globalIdx] = src.regId();
                }
            }
        }
    }

    // For each interval that starts with a MOV, check if it can be collapsed
    // We'll track which intervals can be collapsed and build a map for renaming
    struct CollapseInfo {
        size_t start;
        size_t end;
        RegId dstReg;
        RegId srcReg;
    };
    std::vector<CollapseInfo> intervalsToCollapse;

    for (const auto& interval : workingIntervals) {
        size_t startIdx = interval.Start;

        // Check if this interval starts with a MOV instruction
        auto movIt = movSourceAtStart.find(startIdx);
        if (movIt == movSourceAtStart.end())
            continue;

        // Already pinned intervals cannot be collapsed
        if (interval.isPinned())
            continue;

        // Skip redundant intervals (Start == End means no uses)
        // These should be handled by redundant move elimination instead
        if (interval.isRedundant())
            continue;

        RegId dstReg = interval.Register;

        // Get source value from instruction
        auto blockIt = indexToBlockLocal.find(startIdx);
        if (blockIt == indexToBlockLocal.end())
            continue;
        auto [bIdx, lIdx]        = blockIt->second;
        auto* movInstr           = static_cast<RVMInstr2Op*>(mutableBlocks[bIdx][lIdx].get());
        const RVMValue& srcValue = movInstr->source();

        if (srcValue.isRegister()) {
            RegId srcReg = srcValue.regId();

            // Find the source interval that contains this start index
            const RVMLiveAnalyzer::LiveInterval* srcInterval = nullptr;
            for (const auto& iv : workingIntervals) {
                if (iv.Register == srcReg && iv.Start <= startIdx && startIdx <= iv.End) {
                    srcInterval = &iv;
                    break;
                }
            }

            if (!srcInterval || srcInterval->isPinned())
                continue;

            // Replace lines 163-167 (the srcInterval->End check) with:
            if (srcValue.isRegister()) {
                RegId srcReg = srcValue.regId();

                // Explicitly check if srcReg is redefined in [startIdx+1, interval.End]
                bool srcRedefined = false;
                for (size_t checkIdx = startIdx + 1; checkIdx <= interval.End && !srcRedefined; ++checkIdx) {
                    auto checkIt = indexToBlockLocal.find(checkIdx);
                    if (checkIt != indexToBlockLocal.end()) {
                        auto [bi, li] = checkIt->second;
                        mutableBlocks[bi][li]->forEachDestination([&](const RVMValue& dst) {
                            if (dst.isRegister() && dst.regId() == srcReg)
                                srcRedefined = true;
                        });
                    }
                }
                if (srcRedefined)
                    continue; // Don't collapse - source is redefined before dest's last use
            }
        }

        // Check if all uses in this interval are in MOV instructions
        bool allUsesAreMovs = true;
        for (size_t useIdx = interval.Start; useIdx <= interval.End; ++useIdx) {
            // Skip the definition point (start)
            if (useIdx == interval.Start)
                continue;

            auto it = indexToBlockLocal.find(useIdx);
            if (it != indexToBlockLocal.end()) {
                auto [blockIdx, localIdx] = it->second;
                const auto& instr         = mutableBlocks[blockIdx][localIdx];

                // Check if this instruction uses dstReg
                bool usesDstReg = false;
                instr->forEachValue([&](const RVMValue& value) {
                    if (value.isRegister() && value.regId() == dstReg)
                        usesDstReg = true;
                });

                if (usesDstReg && !isMovInstruction(instr.get())) {
                    allUsesAreMovs = false;
                    break;
                }
            }
        }

        if (allUsesAreMovs) {
            // This interval can be collapsed
            // We need the srcValue again
            auto blockItInner        = indexToBlockLocal.find(startIdx);
            auto [bIdx, lIdx]        = blockItInner->second;
            auto* movInstrInner      = static_cast<RVMInstr2Op*>(mutableBlocks[bIdx][lIdx].get());
            const RVMValue& srcValue = movInstrInner->source();

            // For simplification, we'll store the source register if it is one,
            // or we'll need a different way to handle constants.
            // Actually, the current CollapseInfo only has RegId for srcReg.
            // Let's update it or handle it differently.

            // For now, only collapse register-to-register moves to match existing logic
            if (srcValue.isRegister())
                intervalsToCollapse.push_back({ interval.Start, interval.End, dstReg, srcValue.regId() });
        }
    }

    // After collecting intervalsToCollapse, filter out transitives:
    std::unordered_set<RegId> collapsedDsts;
    for (const auto& c : intervalsToCollapse)
        collapsedDsts.insert(c.dstReg);

    // Remove any collapse where srcReg is also a collapsed dst
    intervalsToCollapse.erase(
        std::remove_if(intervalsToCollapse.begin(), intervalsToCollapse.end(),
                       [&](const CollapseInfo& c) { return collapsedDsts.count(c.srcReg) > 0; }),
        intervalsToCollapse.end());

    bool changed = false;
    // Apply renaming for collapsed intervals
    // We need to rename uses within each collapsed interval
    for (const auto& collapse : intervalsToCollapse) {
        // For each instruction index in the interval [collapse.start, collapse.end]
        // (excluding the definition at collapse.start)
        for (size_t useIdx = collapse.start + 1; useIdx <= collapse.end; ++useIdx) {
            auto it = indexToBlockLocal.find(useIdx);
            if (it != indexToBlockLocal.end()) {
                auto [blockIdx, localIdx] = it->second;
                auto& instr               = mutableBlocks[blockIdx][localIdx];

                // Rename uses of dstReg to srcReg in this instruction
                instr->forEachValue([&](RVMValue& value) {
                    if (value.isRegister() && value.regId() == collapse.dstReg) {
                        value   = RVMValue::Register(collapse.srcReg, value.type());
                        changed = true;
                    }
                });
            }
        }
    }

    // Remove MOV instructions for collapsed intervals
    // Group removals by block to handle index shifting correctly
    std::unordered_map<size_t, std::vector<size_t>> removalsByBlock;
    for (const auto& collapse : intervalsToCollapse) {
        size_t globalIdx = collapse.start;
        auto it          = indexToBlockLocal.find(globalIdx);
        if (it != indexToBlockLocal.end()) {
            auto [blockIdx, localIdx] = it->second;
            removalsByBlock[blockIdx].push_back(localIdx);
        }
    }

    // Remove from each block in descending local index order
    // This ensures indices remain valid as we remove instructions
    for (auto& [blockIdx, localIndices] : removalsByBlock) {
        std::sort(localIndices.rbegin(), localIndices.rend());
        auto& block = mutableBlocks[blockIdx];

        for (size_t localIdx : localIndices) {
            if (localIdx < block.size()) {
                block.erase(block.begin() + localIdx);
                changed = true;
            }
        }
    }

    // Reconstruct program from blocks if changes were made
    if (changed) {
        program.clear();
        for (const auto& block : mutableBlocks)
            program.insert(program.end(), block.begin(), block.end());
    }

    return changed;
}

bool RVMMoveOptimizer::runRedundantMovePass(RVMProgram& program)
{
    // Reanalyze intervals for the current updated program
    auto workingIntervals = RVMLiveAnalyzer::analyzeProgram(program);

    // Split program into basic blocks
    auto mutableBlocks = RVMBasicBlockAnalyzer::splitIntoBlocks(program);
    if (mutableBlocks.empty())
        return false;

    // Calculate block start indices
    std::vector<size_t> blockStartIndices;
    size_t currentIndex = 0;
    for (const auto& block : mutableBlocks) {
        blockStartIndices.push_back(currentIndex);
        currentIndex += block.size();
    }

    bool changed = false;
    for (size_t blockIdx = 0; blockIdx < mutableBlocks.size(); ++blockIdx) {
        bool blockChanged = false;
        removeRedundantMovesInBlock(mutableBlocks[blockIdx], blockStartIndices[blockIdx], workingIntervals, blockChanged);
        changed |= blockChanged;
    }

    // Reconstruct program from blocks if changes were made
    if (changed) {
        program.clear();
        for (const auto& block : mutableBlocks)
            program.insert(program.end(), block.begin(), block.end());
    }

    return changed;
}

bool RVMMoveOptimizer::isMovInstruction(const RVMInstr* instr)
{
    if (auto* instr2op = dynamic_cast<const RVMInstr2Op*>(instr))
        return instr2op->opcode() == Opcode::MOV;
    return false;
}

bool RVMMoveOptimizer::isIdentityMov(const RVMInstr2Op* movInstr)
{
    if (movInstr->opcode() != Opcode::MOV)
        return false;

    const RVMValue& dst = movInstr->destination();
    const RVMValue& src = movInstr->source();

    return dst == src;
}

size_t RVMMoveOptimizer::removeIdentityMoves(std::vector<std::shared_ptr<RVMInstr>>& block, bool& changed)
{
    size_t removed = 0;
    block.erase(std::remove_if(block.begin(), block.end(),
                               [&](const std::shared_ptr<RVMInstr>& instr) {
                                   if (isMovInstruction(instr.get())) {
                                       auto* movInstr = static_cast<RVMInstr2Op*>(instr.get());
                                       if (isIdentityMov(movInstr)) {
                                           removed++;
                                           changed = true;
                                           return true;
                                       }
                                   }
                                   return false;
                               }),
                block.end());
    return removed;
}

void RVMMoveOptimizer::removeRedundantMovesInBlock(
    std::vector<std::shared_ptr<RVMInstr>>& block,
    size_t blockStartIndex,
    const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals,
    bool& changed)
{
    if (block.empty() || intervals.empty())
        return;

    // Find redundant MOV instructions
    std::unordered_set<size_t> redundantIndices;

    for (size_t i = 0; i < block.size(); ++i) {
        const auto& instr = block[i];

        if (!isMovInstruction(instr.get()))
            continue;

        // Get the destination register
        RegId destReg = 0;
        bool hasDest  = false;
        instr->forEachDestination([&](const RVMValue& dstVal) {
            if (dstVal.isRegister()) {
                destReg = dstVal.regId();
                hasDest = true;
            }
        });

        if (!hasDest)
            continue;

        size_t globalIndex = blockStartIndex + i;

        // Find interval for this register definition
        const RVMLiveAnalyzer::LiveInterval* currentInterval = nullptr;
        for (const auto& interval : intervals) {
            if (interval.Register == destReg && interval.Start == globalIndex) {
                currentInterval = &interval;
                break;
            }
        }

        if (!currentInterval)
            continue;

        // A MOV is redundant if its destination register is not used
        // after this definition (interval is redundant) and not pinned
        if (currentInterval->isRedundant())
            redundantIndices.insert(i);
    }

    if (redundantIndices.empty())
        return;

    // Remove redundant instructions in reverse order
    std::vector<size_t> sortedIndices(redundantIndices.begin(), redundantIndices.end());
    std::sort(sortedIndices.rbegin(), sortedIndices.rend());

    for (size_t idx : sortedIndices) {
        if (idx < block.size()) {
            block.erase(block.begin() + idx);
            changed = true;
        }
    }
}

} // namespace PExpr::rvm