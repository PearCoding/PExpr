#include "RVMMoveOptimizer.h"
#include "RVMInstruction.h"
#include "RVMValue.h"
#include "RVMLiveAnalyzer.h"
#include "RVMBasicBlockAnalyzer.h"
#include "utils/Reporter.h"

#include <algorithm>
#include <unordered_set>
#include <unordered_map>
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

    // Get global live intervals for the entire program
    auto intervals = RVMLiveAnalyzer::analyzeProgram(program);
    
    // Split program into basic blocks
    auto blocks = RVMBasicBlockAnalyzer::splitIntoBlocks(program);
    if (blocks.empty())
        return false;

    return optimizeWithLiveIntervals(options, program, intervals, blocks);
}

bool RVMMoveOptimizer::optimizeWithLiveIntervals(const opt::OptimizerOptions& options, 
                                                 RVMProgram& program,
                                                 const std::vector<RVMLiveAnalyzer::LiveInterval>& intervals,
                                                 const std::vector<std::vector<std::shared_ptr<RVMInstr>>>& blocks)
{
    bool changed = false;
    
    // We need to work on a mutable copy of blocks
    auto mutableBlocks = blocks;
    
    // Calculate block start indices in global instruction list
    std::vector<size_t> blockStartIndices;
    size_t currentIndex = 0;
    for (const auto& block : mutableBlocks) {
        blockStartIndices.push_back(currentIndex);
        currentIndex += block.size();
    }
    
    // Build a map from global instruction index to (block index, local index)
    std::vector<std::pair<size_t, size_t>> globalToLocal;
    for (size_t blockIdx = 0; blockIdx < mutableBlocks.size(); ++blockIdx) {
        size_t blockSize = mutableBlocks[blockIdx].size();
        for (size_t localIdx = 0; localIdx < blockSize; ++localIdx) 
            globalToLocal.push_back({blockIdx, localIdx});
    }
    
    // Step 1: Identity move elimination
    if (options.OptimizeIdentityMoves) {
        for (size_t blockIdx = 0; blockIdx < mutableBlocks.size(); ++blockIdx) {
            bool blockChanged = false;
            removeIdentityMoves(mutableBlocks[blockIdx], blockChanged);
            changed |= blockChanged;
        }
    }
    
    // Step 2: Move chain collapsing (interval-based)
    if (options.OptimizeMoveChains) {
        // For each live interval that starts with a MOV instruction
        // we need to check if it can be collapsed into its source interval
        
        // First, build a map from global index to the instruction
        // and identify which intervals start with MOV instructions
        std::unordered_map<size_t, std::pair<size_t, size_t>> indexToBlockLocal; // global index -> (blockIdx, localIdx)
        std::unordered_map<size_t, const RVMLiveAnalyzer::LiveInterval*> intervalByStart;
        std::unordered_map<size_t, RegId> movSourceAtStart; // global index where MOV defines -> source register
        
        for (size_t i = 0; i < intervals.size(); ++i) {
            const auto& interval = intervals[i];
            intervalByStart[interval.Start] = &interval;
        }
        
        // Collect MOV instructions and their sources
        for (size_t blockIdx = 0; blockIdx < mutableBlocks.size(); ++blockIdx) {
            const auto& block = mutableBlocks[blockIdx];
            size_t blockStart = blockStartIndices[blockIdx];
            
            for (size_t localIdx = 0; localIdx < block.size(); ++localIdx) {
                const auto& instr = block[localIdx];
                size_t globalIdx = blockStart + localIdx;
                indexToBlockLocal[globalIdx] = {blockIdx, localIdx};
                
                if (isMovInstruction(instr.get())) {
                    auto* movInstr = static_cast<const RVMInstr2Op*>(instr.get());
                    if (!isIdentityMov(movInstr)) {
                        auto srcs = movInstr->srcs();
                        if (srcs.size() == 1 && srcs[0].isRegister()) {
                            RegId srcReg = srcs[0].regId();
                            movSourceAtStart[globalIdx] = srcReg;
                        }
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
        
        for (const auto& interval : intervals) {
            size_t startIdx = interval.Start;
            
            // Check if this interval starts with a MOV instruction
            auto movIt = movSourceAtStart.find(startIdx);
            if (movIt == movSourceAtStart.end())
                continue;
                
            // Already pinned intervals cannot be collapsed
            if (interval.HasPinned)
                continue;
                
            RegId dstReg = interval.Register;
            RegId srcReg = movIt->second;
            
            // Find the source interval that contains this start index
            const RVMLiveAnalyzer::LiveInterval* srcInterval = nullptr;
            for (const auto& iv : intervals) {
                if (iv.Register == srcReg && iv.Start <= startIdx && startIdx <= iv.End) {
                    srcInterval = &iv;
                    break;
                }
            }
            
            if (!srcInterval || srcInterval->HasPinned)
                continue;
                
            // Check if all uses in this interval are in MOV instructions
            bool allUsesAreMovs = true;
            for (size_t useIdx = interval.Start; useIdx <= interval.End; ++useIdx) {
                // Skip the definition point (start)
                if (useIdx == interval.Start)
                    continue;
                    
                auto it = indexToBlockLocal.find(useIdx);
                if (it != indexToBlockLocal.end()) {
                    auto [blockIdx, localIdx] = it->second;
                    const auto& instr = mutableBlocks[blockIdx][localIdx];
                    
                    // Check if this instruction uses dstReg
                    bool usesDstReg = false;
                    instr->forEachValue([&](const RVMValue& value) {
                        if (value.isRegister() && value.regId() == dstReg) {
                            usesDstReg = true;
                        }
                    });
                    
                    if (usesDstReg && !isMovInstruction(instr.get())) {
                        allUsesAreMovs = false;
                        break;
                    }
                }
            }
            
            if (allUsesAreMovs) {
                // This interval can be collapsed
                intervalsToCollapse.push_back({interval.Start, interval.End, dstReg, srcReg});
            }
        }
        
        // Apply renaming for collapsed intervals
        // We need to rename uses within each collapsed interval
        for (const auto& collapse : intervalsToCollapse) {
            // For each instruction index in the interval [collapse.start, collapse.end]
            // (excluding the definition at collapse.start)
            for (size_t useIdx = collapse.start + 1; useIdx <= collapse.end; ++useIdx) {
                auto it = indexToBlockLocal.find(useIdx);
                if (it != indexToBlockLocal.end()) {
                    auto [blockIdx, localIdx] = it->second;
                    auto& instr = mutableBlocks[blockIdx][localIdx];
                    
                    // Rename uses of dstReg to srcReg in this instruction
                    instr->forEachValue([&](RVMValue& value) {
                        if (value.isRegister() && value.regId() == collapse.dstReg) {
                            value = RVMValue::Register(collapse.srcReg, value.type());
                            changed = true;
                        }
                    });
                }
            }
        }
        
        // Remove MOV instructions for collapsed intervals
        std::vector<size_t> movsToRemove;
        for (const auto& collapse : intervalsToCollapse)
            movsToRemove.push_back(collapse.start);
        
        // Sort in descending order and remove
        std::sort(movsToRemove.rbegin(), movsToRemove.rend());
        for (size_t globalIdx : movsToRemove) {
            auto it = indexToBlockLocal.find(globalIdx);
            if (it != indexToBlockLocal.end()) {
                auto [blockIdx, localIdx] = it->second;
                auto& block = mutableBlocks[blockIdx];
                if (localIdx < block.size()) {
                    block.erase(block.begin() + localIdx);
                    changed = true;
                }
            }
        }
    }
    
    // Step 3: Redundant move elimination
    if (options.OptimizeRedundantMoves) {
        for (size_t blockIdx = 0; blockIdx < mutableBlocks.size(); ++blockIdx) {
            bool blockChanged = false;
            removeRedundantMovesInBlock(mutableBlocks[blockIdx], blockStartIndices[blockIdx], intervals, blockChanged);
            changed |= blockChanged;
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

    auto dstOpt = movInstr->dst();
    auto srcs   = movInstr->srcs();

    PEXPR_ASSERT(dstOpt.has_value(), "'mov' instruction must have a target destination");
    PEXPR_ASSERT(srcs.size() == 1, "'mov' instruction must have a single source");

    const RVMValue& dst = dstOpt.value();
    const RVMValue& src = srcs[0];

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
        bool hasDest = false;
        instr->forDestination([&](const RVMValue& dstVal) {
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