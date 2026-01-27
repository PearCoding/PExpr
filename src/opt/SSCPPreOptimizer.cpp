#include "SSCPPreOptimizer.h"
#include "SSCPSideEffectAnalyzer.h"

#include <algorithm>
#include <queue>
#include <ranges>

namespace PExpr::opt {
using namespace ssa;

bool SSCPPreOptimizer::applyPRE(SSAContext* ctx, InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions)
{
    mBlockAnalyzer.identifyBasicBlocks(instructions);
    mBlockAnalyzer.buildControlFlowGraph(instructions);

    PEXPR_UNUSED(ctx);

    // Clear previous state
    mExpressionInfo.clear();
    mInstructionToExpression.clear();

    // Get basic blocks from the analyzer
    if (mBlockAnalyzer.getBasicBlocks().empty())
        return false;

    // Step 1: Identify expressions
    identifyExpressions(instructions, sideEffectedFunctions);

    // Step 2: Perform PRE analysis
    computeAvailability();
    computeAnticipability();
    computeEarliestPlacement();
    computeLatestPlacement();
    computeInsertionDeletionPoints();

    // Step 3: Apply code motion
    return applyCodeMotion(ctx, instructions, sideEffectedFunctions);
}

void SSCPPreOptimizer::identifyExpressions(const InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions)
{
    mExpressionInfo.clear();
    mInstructionToExpression.clear();

    size_t nextExpressionId = 0;

    // TODO: Use the existing CSE hash logic to identify equivalent expressions

    // This is a simplified implementation
    // In practice, we would need to track expressions across basic blocks

    for (const auto& instr : instructions) {
        if (!instr)
            continue;

        // Only consider assignments and calls that produce values
        if (auto asg = dynamic_cast<const SSAInstrAssign*>(instr.get())) {
            // Skip assignments that are just copies
            if (asg->Operator != SSAInstrAssign::OpKind::Assign) {
                // Create expression info
                ExpressionInfo info;
                info.expressionId = nextExpressionId++;
                info.value        = asg->Target;

                mExpressionInfo[info.expressionId]    = info;
                mInstructionToExpression[instr.get()] = info.expressionId;
            }
        } else if (auto call = dynamic_cast<const SSAInstrCall*>(instr.get())) {
            // Skip calls with side effects
            if (sideEffectedFunctions.contains(call->FunctionName))
                continue;

            // Create expression info
            ExpressionInfo info;
            info.expressionId = nextExpressionId++;
            info.value        = call->Target;

            mExpressionInfo[info.expressionId]    = info;
            mInstructionToExpression[instr.get()] = info.expressionId;
        }
    }
}

void SSCPPreOptimizer::computeAvailability()
{
    const auto& basicBlocks = mBlockAnalyzer.getBasicBlocks();
    size_t numBlocks        = basicBlocks.size();

    // Initialize availability sets - all empty initially
    for (auto& [id, info] : mExpressionInfo) {
        info.availableAtEntry.clear();
        info.availableAtExit.clear();
    }

    // Iterative dataflow analysis for availability
    bool changed;
    do {
        changed = false;

        for (size_t blockIdx = 0; blockIdx < numBlocks; ++blockIdx) {
            for (auto& [exprId, info] : mExpressionInfo) {
                // Compute available at entry: intersection of available at exit of predecessors
                std::unordered_set<size_t> newEntry;

                if (basicBlocks[blockIdx].predecessors.empty()) {
                    // Entry block: nothing is available initially
                    newEntry.clear();
                } else {
                    // Start with universal set (all blocks)
                    for (size_t i = 0; i < numBlocks; ++i)
                        newEntry.insert(i);

                    // Intersect over all predecessors
                    for (size_t pred : basicBlocks[blockIdx].predecessors) {
                        std::unordered_set<size_t> temp;
                        for (size_t block : newEntry) {
                            if (info.availableAtExit.contains(block))
                                temp.insert(block);
                        }
                        newEntry = std::move(temp);
                    }
                }

                // Compute available at exit: available at entry ∪ generated in this block
                std::unordered_set<size_t> newExit = newEntry;

                // Simplified: if expression appears in this block, it's generated
                // Actual implementation would need to check block instructions

                // Check for changes
                if (newEntry != info.availableAtEntry || newExit != info.availableAtExit) {
                    info.availableAtEntry = std::move(newEntry);
                    info.availableAtExit  = std::move(newExit);
                    changed               = true;
                }
            }
        }
    } while (changed);
}

void SSCPPreOptimizer::computeAnticipability()
{
    const auto& basicBlocks = mBlockAnalyzer.getBasicBlocks();
    size_t numBlocks        = basicBlocks.size();

    // Initialize anticipability sets
    for (auto& [id, info] : mExpressionInfo) {
        info.anticipatedAtEntry.clear();
        info.anticipatedAtExit.clear();

        // Initially, nothing is anticipated
    }

    // Iterative backward dataflow analysis for anticipability
    bool changed;
    do {
        changed = false;

        for (size_t blockIdx = numBlocks; blockIdx > 0; --blockIdx) {
            size_t i = blockIdx - 1; // Process in reverse order

            for (auto& [exprId, info] : mExpressionInfo) {
                // Compute anticipated at exit: union of anticipated at entry of successors
                std::unordered_set<size_t> newExit;
                for (size_t succ : basicBlocks[i].successors)
                    newExit.insert(succ);

                // Compute anticipated at entry: anticipated at exit ∪ generated in block
                std::unordered_set<size_t> newEntry = newExit;
                // Add block if expression is used here (simplified)

                // Check for changes
                if (newEntry != info.anticipatedAtEntry || newExit != info.anticipatedAtExit) {
                    info.anticipatedAtEntry = std::move(newEntry);
                    info.anticipatedAtExit  = std::move(newExit);
                    changed                 = true;
                }
            }
        }
    } while (changed);
}

void SSCPPreOptimizer::computeEarliestPlacement()
{
    const auto& basicBlocks = mBlockAnalyzer.getBasicBlocks();
    size_t numBlocks        = basicBlocks.size();

    for (auto& [id, info] : mExpressionInfo) {
        info.earliest.clear();

        // Earliest[i] = ¬AvailableIn[i] ∩ AnticipatedIn[i]
        for (size_t i = 0; i < numBlocks; ++i) {
            bool isAvailable   = info.availableAtEntry.contains(i);
            bool isAnticipated = info.anticipatedAtEntry.contains(i);

            if (!isAvailable && isAnticipated)
                info.earliest.insert(i);
        }
    }
}

void SSCPPreOptimizer::computeLatestPlacement()
{
    const auto& basicBlocks = mBlockAnalyzer.getBasicBlocks();
    size_t numBlocks        = basicBlocks.size();

    for (auto& [id, info] : mExpressionInfo) {
        info.latest.clear();

        // Latest[i] = Earliest[i] ∪
        //             {i | i ∈ AnticipatedIn[i] ∧ ¬∃ successor j where Earliest[j] ∨ Latest[j]}
        for (size_t i = 0; i < numBlocks; ++i) {
            if (info.earliest.contains(i)) {
                info.latest.insert(i);
                continue;
            }

            if (info.anticipatedAtEntry.contains(i)) {
                bool hasSuccessorPlacement = false;
                for (size_t succ : basicBlocks[i].successors) {
                    if (info.earliest.contains(succ) || info.latest.contains(succ)) {
                        hasSuccessorPlacement = true;
                        break;
                    }
                }

                if (!hasSuccessorPlacement)
                    info.latest.insert(i);
            }
        }
    }
}

void SSCPPreOptimizer::computeInsertionDeletionPoints()
{
    const auto& basicBlocks = mBlockAnalyzer.getBasicBlocks();
    size_t numBlocks        = basicBlocks.size();

    for (auto& [id, info] : mExpressionInfo) {
        info.insert.clear();
        info.delete_.clear();

        // Insert at latest placement points
        info.insert = info.latest;

        // Delete where expression is redundant
        // For each block where expression is available at entry and anticipated at entry
        for (size_t i = 0; i < numBlocks; ++i) {
            if (info.availableAtEntry.contains(i) && info.anticipatedAtEntry.contains(i))
                info.delete_.insert(i);
        }
    }
}

bool SSCPPreOptimizer::applyCodeMotion(SSAContext* ctx, InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions)
{
    PEXPR_UNUSED(ctx);

    bool changed = false;

    // Get natural loops from the analyzer
    mBlockAnalyzer.findNaturalLoops();
    const auto& loops       = mBlockAnalyzer.getNaturalLoops();
    const auto& basicBlocks = mBlockAnalyzer.getBasicBlocks();

    // For each loop, identify loop-invariant computations
    for (const auto& loop : loops) {
        // Analyze each block in the loop
        for (size_t blockIdx : loop) {
            const auto& block = basicBlocks[blockIdx];

            // Check instructions in this block
            for (size_t i = block.startIndex; i < block.endIndex; ++i) {
                if (i >= instructions.size())
                    break;

                auto& instr = instructions[i];
                if (!instr)
                    continue;

                // Check if this instruction is loop-invariant
                if (isSafeToMove(instr.get(), sideEffectedFunctions)) {
                    // Check if all operands are defined outside the loop
                    bool allOperandsOutsideLoop                 = true;
                    std::function<void(SSAValue&)> checkOperand = [&](SSAValue& val) {
                        if (!val.isConstant()) {
                            // Need to track where variables are defined
                            // Simplified: assume variables defined in loop are not invariant
                            allOperandsOutsideLoop = false;
                        }
                    };

                    instr->forEachOperand(checkOperand);

                    if (allOperandsOutsideLoop) {
                        // This is loop-invariant code that could be moved
                        // In actual implementation, we would move it to a preheader block
                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}

bool SSCPPreOptimizer::areExpressionsEquivalent(const SSAInstr* a, const SSAInstr* b) const
{
    // Reuse CSE equivalence checking logic
    // For PRE, we need to check if expressions compute the same value

    if (!a || !b)
        return false;

    // Check type
    if (typeid(*a) != typeid(*b))
        return false;

    // Simple check: if they have the same expression ID, they're equivalent
    auto itA = mInstructionToExpression.find(a);
    auto itB = mInstructionToExpression.find(b);

    if (itA != mInstructionToExpression.end() && itB != mInstructionToExpression.end())
        return itA->second == itB->second;

    return false;
}

size_t SSCPPreOptimizer::getExpressionId(const SSAInstr* instr) const
{
    auto it = mInstructionToExpression.find(instr);
    if (it != mInstructionToExpression.end())
        return it->second;
    return SIZE_MAX;
}

bool SSCPPreOptimizer::isSafeToMove(const SSAInstr* instr, const std::unordered_set<std::string>& sideEffectedFunctions) const
{
    if (!instr)
        return false;

    // Check for side effects
    if (auto call = dynamic_cast<const SSAInstrCall*>(instr))
        return !sideEffectedFunctions.contains(call->FunctionName);

    // Other instructions are generally safe to move
    return true;
}

bool SSCPPreOptimizer::dominatesAllUses(size_t blockIdx, const SSAInstr* instr) const
{
    // Check if the block dominates all blocks where the value is used
    // This is a simplified check

    // For PRE, we need to ensure that moving code doesn't introduce
    // new computations on paths where they weren't previously computed

    return true; // Simplified
}

} // namespace PExpr::ssa