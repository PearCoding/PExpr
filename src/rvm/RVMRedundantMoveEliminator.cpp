#include "RVMRedundantMoveEliminator.h"
#include "RVMBasicBlockAnalyzer.h"
#include "RVMLiveAnalyzer.h"

#include <algorithm>
#include <unordered_map>

namespace PExpr::rvm {

bool RVMRedundantMoveEliminator::eliminate(RVMProgram& program)
{
    bool changed = false;

    // Split program into basic blocks using the analyzer
    auto blocks = RVMBasicBlockAnalyzer::splitIntoBlocks(program);

    // Process each block
    for (auto& block : blocks)
        changed |= eliminateInBlock(block);

    // Reconstruct program from blocks
    if (changed) {
        program.clear();
        for (auto& block : blocks)
            program.insert(program.end(), block.begin(), block.end());
    }

    return changed;
}

bool RVMRedundantMoveEliminator::eliminateInBlock(std::vector<std::shared_ptr<RVMInstr>>& block)
{
    if (block.empty())
        return false;

    bool changed = false;
    bool anyChange;

    // Run multiple passes to handle cascading redundancy
    // (e.g., when removing a MOV makes another MOV redundant)
    do {
        anyChange = false;

        // Find indices of redundant MOV instructions using live interval analysis
        std::unordered_set<size_t> redundantIndices;
        analyzeBlock(block, redundantIndices);

        if (redundantIndices.empty())
            break;

        // Remove redundant instructions (in reverse order to preserve indices)
        // Convert to vector and sort in descending order
        std::vector<size_t> sortedIndices(redundantIndices.begin(), redundantIndices.end());
        std::sort(sortedIndices.rbegin(), sortedIndices.rend());

        for (size_t idx : sortedIndices) {
            if (idx < block.size()) {
                block.erase(block.begin() + idx);
                anyChange = true;
                changed   = true;
            }
        }

        // Continue if we made changes (cascading redundancy might have been created)
    } while (anyChange);

    return changed;
}

void RVMRedundantMoveEliminator::analyzeBlock(const std::vector<std::shared_ptr<RVMInstr>>& block, std::unordered_set<size_t>& redundantIndices)
{
    redundantIndices.clear();

    // Get live intervals for this block from RVMLiveAnalyzer
    auto intervals = RVMLiveAnalyzer::analyzeBlock(block);

    // For each instruction that is a MOV
    for (size_t i = 0; i < block.size(); ++i) {
        const auto& instr = block[i];

        // Check if it's a MOV instruction
        if (auto* instr2op = dynamic_cast<const RVMInstr2Op*>(instr.get())) {
            if (instr2op->opcode() != Opcode::MOV)
                continue;
        } else {
            continue; // Not a 2-operand instruction
        }

        // Get the destination register
        RegId destReg = 0;
        bool hasDest  = false;
        instr->forDestination([&](const RVMValue& dstVal) {
            if (dstVal.isRegister()) {
                destReg = dstVal.regId();
                hasDest = true;
            }
        });

        if (!hasDest)
            continue;

        // Find the interval that starts at this instruction
        const RVMLiveAnalyzer::LiveInterval* currentInterval = nullptr;
        for (const auto& interval : intervals) {
            if (interval.reg == destReg && interval.start == i) {
                currentInterval = &interval;
                break;
            }
        }

        if (!currentInterval)
            continue;

        // A MOV is redundant if:
        // 1. The register is defined here (start == i)
        // 2. The register is not used after this definition (interval.isRedundant() means start == end)
        // 3. AND there is a later definition of the same register

        if (currentInterval->isRedundant()) {
            // Check if there's any later definition of this register
            // Look through intervals for the same register with later start
            bool hasLaterDefinition = false;
            for (const auto& interval : intervals) {
                if (interval.reg == destReg && interval.start > i) {
                    hasLaterDefinition = true;
                    break;
                }
            }

            // Only mark as redundant if there's definitely a later definition
            // This handles the case where a MOV result is dead (never used)
            // but not overwritten - we leave those for dead code elimination
            if (hasLaterDefinition)
                redundantIndices.insert(i);
        }
    }
}

} // namespace PExpr::rvm