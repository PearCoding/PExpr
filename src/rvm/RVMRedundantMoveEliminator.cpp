#include "RVMRedundantMoveEliminator.h"
#include "RVMBasicBlockAnalyzer.h"

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

        // Find indices of redundant MOV instructions
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

bool RVMRedundantMoveEliminator::isMovInstruction(const RVMInstr* instr)
{
    if (auto* instr2op = dynamic_cast<const RVMInstr2Op*>(instr))
        return instr2op->opcode() == Opcode::MOV;
    return false;
}

void RVMRedundantMoveEliminator::analyzeBlock(
    const std::vector<std::shared_ptr<RVMInstr>>& block,
    std::unordered_set<size_t>& redundantIndices)
{
    redundantIndices.clear();

    // For each register, track:
    // - Last definition index (where it was written)
    // - Whether it has been read since last definition
    std::unordered_map<RegId, size_t> lastDefIndex;
    std::unordered_map<RegId, bool> readSinceLastDef;

    for (size_t i = 0; i < block.size(); ++i) {
        const auto& instr = block[i];

        // Process source reads first
        instr->forEachSource([&](const RVMValue& srcVal) {
            if (srcVal.isRegister()) //< Mark this register as read since its last definition
                readSinceLastDef[srcVal.regId()] = true;
        });

        // Process destination write
        instr->forDestination([&](const RVMValue& dstVal) {
            if (!dstVal.isRegister())
                return;

            RegId reg = dstVal.regId();

            // Check if this register was previously defined
            if (auto it = lastDefIndex.find(reg); it != lastDefIndex.end()) {
                size_t prevDefIndex = it->second;

                // Check if the previous definition was a MOV instruction
                if (prevDefIndex < i && isMovInstruction(block[prevDefIndex].get())) {
                    // Check if the register was read between the previous definition and now
                    if (!readSinceLastDef[reg]) //< The previous MOV is redundant - its result is overwritten before being read
                        redundantIndices.insert(prevDefIndex);
                }
            }

            // Update last definition index and reset read flag
            lastDefIndex[reg]     = i;
            readSinceLastDef[reg] = false;
        });
    }
}

} // namespace PExpr::rvm