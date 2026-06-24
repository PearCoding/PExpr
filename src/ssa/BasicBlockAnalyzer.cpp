#include "BasicBlockAnalyzer.h"
#include "SSAMapper.h"

#include <algorithm>

namespace PExpr::ssa {

void BasicBlockAnalyzer::identifyBasicBlocks(const InstructionList& instructions)
{
    mBasicBlocks.clear();
    mLabelToBlock.clear();

    if (instructions.empty())
        return;

    BasicBlock currentBlock;
    currentBlock.startIndex = 0;

    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& instr = instructions[i];

        // Check if this is a leader (starts a new basic block)
        bool isLeader = false;

        if (i == 0) {
            // First instruction is always a leader
            isLeader = true;
        } else if (dynamic_cast<const SSAInstrLabel*>(instr.get())) {
            // Any label instruction starts a new block
            isLeader = true;
        } else if (dynamic_cast<const SSAInstrBranch*>(instr.get())
                   || dynamic_cast<const SSAInstrGoto*>(instr.get())
                   || dynamic_cast<const SSAInstrReturn*>(instr.get())) {
            // Any branch, goto, or return ends a block
            // End current block
            currentBlock.endIndex = i + 1;
            mBasicBlocks.push_back(currentBlock);

            // Start new block
            currentBlock            = BasicBlock();
            currentBlock.startIndex = i + 1;
            continue;
        }

        // If this is a leader and we have a current block, end it
        if (isLeader && i > currentBlock.startIndex) {
            currentBlock.endIndex = i;
            mBasicBlocks.push_back(currentBlock);

            currentBlock            = BasicBlock();
            currentBlock.startIndex = i;
        }

        // Store label name if present
        if (auto label = dynamic_cast<const SSAInstrLabel*>(instr.get())) {
            currentBlock.label         = label->Name;
            mLabelToBlock[label->Name] = mBasicBlocks.size(); // Will be the next block index
        }
    }

    // Add the last block if it has instructions
    if (currentBlock.startIndex < instructions.size()) {
        currentBlock.endIndex = instructions.size();
        mBasicBlocks.push_back(currentBlock);
    }

    // Update label map with actual indices
    mLabelToBlock.clear();
    for (size_t i = 0; i < mBasicBlocks.size(); ++i) {
        if (!mBasicBlocks[i].label.empty())
            mLabelToBlock[mBasicBlocks[i].label] = i;
    }
}

void BasicBlockAnalyzer::buildControlFlowGraph(const InstructionList& instructions)
{
    // Initialize predecessor/successor sets
    for (auto& block : mBasicBlocks) {
        block.predecessors.clear();
        block.successors.clear();
    }

    // Build control flow edges by analyzing terminators
    for (size_t i = 0; i < mBasicBlocks.size(); ++i) {
        auto& block = mBasicBlocks[i];

        // Get the last instruction in the block
        if (block.startIndex >= block.endIndex || block.endIndex > instructions.size())
            continue;

        size_t lastInstrIdx   = block.endIndex - 1;
        const auto& lastInstr = instructions[lastInstrIdx];

        // Check the last instruction for control flow
        if (auto branch = dynamic_cast<const SSAInstrBranch*>(lastInstr.get())) {
            // Conditional branch: successor is the target label, plus fall-through to next block
            if (auto it = mLabelToBlock.find(branch->TargetLabel); it != mLabelToBlock.end()) {
                size_t targetBlock = it->second;
                block.successors.insert(targetBlock);
                mBasicBlocks[targetBlock].predecessors.insert(i);
            }

            // Fall-through to next block (if exists)
            if (i + 1 < mBasicBlocks.size()) {
                block.successors.insert(i + 1);
                mBasicBlocks[i + 1].predecessors.insert(i);
            }
        } else if (auto gotoInstr = dynamic_cast<const SSAInstrGoto*>(lastInstr.get())) {
            // Unconditional goto: successor is only the target label
            if (auto it = mLabelToBlock.find(gotoInstr->TargetLabel); it != mLabelToBlock.end()) {
                size_t targetBlock = it->second;
                block.successors.insert(targetBlock);
                mBasicBlocks[targetBlock].predecessors.insert(i);
            }
        } else if (dynamic_cast<const SSAInstrReturn*>(lastInstr.get())) {
            // Return: no successors
            // Nothing to do
        } else {
            // Not a terminator: fall-through to next block
            if (i + 1 < mBasicBlocks.size()) {
                block.successors.insert(i + 1);
                mBasicBlocks[i + 1].predecessors.insert(i);
            }
        }
    }
}

} // namespace PExpr::ssa