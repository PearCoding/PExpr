#include "BasicBlockAnalyzer.h"
#include "SSAMapper.h"

#include <algorithm>
#include <queue>

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

void BasicBlockAnalyzer::computeDominators()
{
    size_t numBlocks = mBasicBlocks.size();
    mDominators.resize(numBlocks);
    mImmediateDominator.resize(numBlocks, SIZE_MAX);

    initializeDominators();

    // Iterative dataflow analysis to compute dominators
    bool changed;
    do {
        changed = false;

        for (size_t i = 1; i < numBlocks; ++i) { // Skip entry block
            std::unordered_set<size_t> newDoms;

            // Start with all blocks
            for (size_t j = 0; j < numBlocks; ++j)
                newDoms.insert(j);

            // Intersect dominators of all predecessors
            for (size_t pred : mBasicBlocks[i].predecessors) {
                std::unordered_set<size_t> temp;
                for (size_t dom : newDoms) {
                    if (mDominators[pred].contains(dom))
                        temp.insert(dom);
                }
                newDoms = std::move(temp);
            }

            // Add self
            newDoms.insert(i);

            // Check for changes
            if (newDoms != mDominators[i]) {
                mDominators[i] = std::move(newDoms);
                changed        = true;
            }
        }
    } while (changed);

    computeImmediateDominators();
}

void BasicBlockAnalyzer::findNaturalLoops()
{
    mLoops.clear();

    // Simple loop detection: identify back edges in the CFG
    for (size_t i = 0; i < mBasicBlocks.size(); ++i) {
        for (size_t succ : mBasicBlocks[i].successors) {
            // Check if successor dominates predecessor (back edge)
            if (dominates(succ, i)) {
                // Found a back edge from i -> succ
                // The loop consists of succ and all blocks that can reach i without going through succ
                std::unordered_set<size_t> loop;
                loop.insert(succ);

                // Perform DFS to find all blocks in the loop
                std::queue<size_t> worklist;
                worklist.push(i);

                while (!worklist.empty()) {
                    size_t current = worklist.front();
                    worklist.pop();

                    if (loop.contains(current))
                        continue;
                    loop.insert(current);

                    // Add predecessors that aren't succ
                    for (size_t pred : mBasicBlocks[current].predecessors) {
                        if (pred != succ)
                            worklist.push(pred);
                    }
                }

                mLoops.push_back(loop);
            }
        }
    }
}

bool BasicBlockAnalyzer::dominates(size_t a, size_t b) const
{
    if (a >= mDominators.size() || b >= mDominators.size())
        return false;
    return mDominators[b].contains(a);
}

bool BasicBlockAnalyzer::strictlyDominates(size_t a, size_t b) const
{
    return dominates(a, b) && a != b;
}

size_t BasicBlockAnalyzer::findBlockContainingInstruction(size_t instructionIndex) const
{
    for (size_t i = 0; i < mBasicBlocks.size(); ++i) {
        if (mBasicBlocks[i].containsInstruction(instructionIndex))
            return i;
    }
    return SIZE_MAX;
}

size_t BasicBlockAnalyzer::splitBasicBlock(size_t blockIndex, size_t splitInstructionIndex)
{
    if (blockIndex >= mBasicBlocks.size())
        return SIZE_MAX;

    auto& block = mBasicBlocks[blockIndex];
    if (!block.containsInstruction(splitInstructionIndex)
        || splitInstructionIndex <= block.startIndex
        || splitInstructionIndex >= block.endIndex - 1)
        return SIZE_MAX;

    // Create new block for the split portion
    BasicBlock newBlock;
    newBlock.startIndex = splitInstructionIndex;
    newBlock.endIndex   = block.endIndex;

    // Update original block
    block.endIndex = splitInstructionIndex;

    // Insert new block after the original
    size_t newIndex = blockIndex + 1;
    mBasicBlocks.insert(mBasicBlocks.begin() + newIndex, newBlock);

    // Update control flow: new block inherits successors from original
    newBlock.successors   = block.successors;
    newBlock.predecessors = { blockIndex };

    // Original block now only goes to new block
    block.successors = { newIndex };

    // Update successors' predecessors
    for (size_t succ : newBlock.successors) {
        mBasicBlocks[succ].predecessors.erase(blockIndex);
        mBasicBlocks[succ].predecessors.insert(newIndex);
    }

    return newIndex;
}

bool BasicBlockAnalyzer::mergeConsecutiveBlocks(size_t firstBlockIndex, size_t secondBlockIndex)
{
    if (firstBlockIndex >= mBasicBlocks.size()
        || secondBlockIndex >= mBasicBlocks.size()
        || secondBlockIndex != firstBlockIndex + 1)
        return false;

    auto& first  = mBasicBlocks[firstBlockIndex];
    auto& second = mBasicBlocks[secondBlockIndex];

    // Can only merge if first block has exactly one successor (the second block)
    // and second block has exactly one predecessor (the first block)
    if (first.successors.size() != 1
        || *first.successors.begin() != secondBlockIndex
        || second.predecessors.size() != 1
        || *second.predecessors.begin() != firstBlockIndex)
        return false;

    // Merge blocks
    first.endIndex   = second.endIndex;
    first.successors = second.successors;

    // Update successors' predecessors
    for (size_t succ : first.successors) {
        mBasicBlocks[succ].predecessors.erase(secondBlockIndex);
        mBasicBlocks[succ].predecessors.insert(firstBlockIndex);
    }

    // Remove the second block
    mBasicBlocks.erase(mBasicBlocks.begin() + secondBlockIndex);

    return true;
}

void BasicBlockAnalyzer::getBlockInstructions(size_t blockIndex, const InstructionList& allInstructions,
                                              InstructionList& blockInstructions) const
{
    blockInstructions.clear();

    if (blockIndex >= mBasicBlocks.size())
        return;

    const auto& block = mBasicBlocks[blockIndex];
    for (size_t i = block.startIndex; i < block.endIndex && i < allInstructions.size(); ++i)
        blockInstructions.push_back(allInstructions[i]);
}

void BasicBlockAnalyzer::initializeDominators()
{
    size_t numBlocks = mBasicBlocks.size();

    for (size_t i = 0; i < numBlocks; ++i) {
        if (i == 0) {
            // Entry block dominates itself
            mDominators[i].insert(i);
        } else {
            // All blocks dominate all initially (will be refined)
            for (size_t j = 0; j < numBlocks; ++j)
                mDominators[i].insert(j);
        }
    }
}

void BasicBlockAnalyzer::computeImmediateDominators()
{
    size_t numBlocks = mBasicBlocks.size();

    for (size_t i = 0; i < numBlocks; ++i) {
        // Find the unique dominator that is not i and dominates all other dominators of i
        for (size_t dom : mDominators[i]) {
            if (dom == i)
                continue;

            bool isImmediate = true;
            for (size_t otherDom : mDominators[i]) {
                if (otherDom == i || otherDom == dom)
                    continue;

                // Check if otherDom dominates dom
                if (mDominators[dom].contains(otherDom)) {
                    isImmediate = false;
                    break;
                }
            }

            if (isImmediate) {
                mImmediateDominator[i] = dom;
                break;
            }
        }
    }
}

bool BasicBlockAnalyzer::isBackEdge(size_t from, size_t to) const
{
    // A back edge is when 'to' dominates 'from'
    return dominates(to, from);
}

void BasicBlockAnalyzer::collectReachableBlocks(size_t start, size_t exclude,
                                                std::unordered_set<size_t>& result) const
{
    std::queue<size_t> worklist;
    worklist.push(start);

    while (!worklist.empty()) {
        size_t current = worklist.front();
        worklist.pop();

        if (result.contains(current))
            continue;
        result.insert(current);

        // Add predecessors that aren't the excluded block
        for (size_t pred : mBasicBlocks[current].predecessors) {
            if (pred != exclude)
                worklist.push(pred);
        }
    }
}

} // namespace PExpr::ssa