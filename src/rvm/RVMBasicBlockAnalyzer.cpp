#include "RVMBasicBlockAnalyzer.h"

namespace PExpr::rvm {

RVMBasicBlockAnalyzer::BlockList RVMBasicBlockAnalyzer::splitIntoBlocks(const RVMProgram& program)
{
    BlockList blocks;
    Block currentBlock;

    for (const auto& instr : program) {
        // Check if this instruction starts a new block
        if (startsNewBlock(instr.get()) && !currentBlock.empty()) {
            blocks.push_back(std::move(currentBlock));
            currentBlock.clear();
        }

        currentBlock.push_back(instr);

        // Check if this instruction ends the current block
        if (endsBlock(instr.get())) {
            blocks.push_back(std::move(currentBlock));
            currentBlock.clear();
        }
    }

    // Don't forget the last block
    if (!currentBlock.empty())
        blocks.push_back(std::move(currentBlock));

    return blocks;
}

bool RVMBasicBlockAnalyzer::isControlFlow(const RVMInstr* instr)
{
    return dynamic_cast<const RVMInstrBranch*>(instr) != nullptr || dynamic_cast<const RVMInstrJump*>(instr) != nullptr || dynamic_cast<const RVMInstrReturn*>(instr) != nullptr;
}

bool RVMBasicBlockAnalyzer::startsNewBlock(const RVMInstr* instr)
{
    // Labels always start a new block
    if (dynamic_cast<const RVMInstrLabel*>(instr) != nullptr)
        return true;

    return false;
}

bool RVMBasicBlockAnalyzer::endsBlock(const RVMInstr* instr)
{
    return isControlFlow(instr);
}

std::vector<std::string> RVMBasicBlockAnalyzer::getSuccessors(const Block& block)
{
    std::vector<std::string> successors;

    if (block.empty())
        return successors;

    // Check the last instruction in the block
    const auto& lastInstr = block.back();

    if (auto* branch = dynamic_cast<const RVMInstrBranch*>(lastInstr.get())) {
        // Conditional branch: has one explicit target
        successors.push_back(branch->targetLabel());

        // And implicit fall-through to next block (if any)
        // This will be handled by block ordering
    } else if (auto* jump = dynamic_cast<const RVMInstrJump*>(lastInstr.get())) {
        // Unconditional jump: has one target
        successors.push_back(jump->targetLabel());
    } else if (dynamic_cast<const RVMInstrReturn*>(lastInstr.get()) != nullptr) {
        // Return: no successors
    } else {
        // Normal instruction: fall through to next block
        // This will be handled by block ordering
    }

    return successors;
}

std::unordered_map<std::string, std::vector<size_t>> RVMBasicBlockAnalyzer::buildPredecessorMap(const BlockList& blocks)
{
    std::unordered_map<std::string, std::vector<size_t>> predecessorMap;

    // First pass: collect all label names and their block indices
    for (size_t i = 0; i < blocks.size(); ++i) {
        const auto& block = blocks[i];
        if (!block.empty()) {
            if (auto* label = dynamic_cast<const RVMInstrLabel*>(block[0].get()))
                predecessorMap[label->labelName()] = {};
        }
    }

    // Second pass: find predecessors for each block
    for (size_t i = 0; i < blocks.size(); ++i) {
        const auto& block = blocks[i];
        auto successors   = getSuccessors(block);

        for (const auto& targetLabel : successors) {
            if (auto it = predecessorMap.find(targetLabel); it != predecessorMap.end())
                it->second.push_back(i);
        }

        // Also handle fall-through: if this block doesn't end with control flow,
        // the next block is a successor
        if (!block.empty() && !endsBlock(block.back().get())) {
            if (i + 1 < blocks.size()) {
                const auto& nextBlock = blocks[i + 1];
                if (!nextBlock.empty()) {
                    if (auto* label = dynamic_cast<const RVMInstrLabel*>(nextBlock[0].get()))
                        predecessorMap[label->labelName()].push_back(i);
                }
            }
        }
    }

    return predecessorMap;
}

} // namespace PExpr::rvm