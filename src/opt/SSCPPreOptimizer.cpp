#include "SSCPPreOptimizer.h"

#include "ssa/SSAContext.h"

#include <algorithm>

namespace PExpr::opt {
using namespace ssa;

bool SSCPPreOptimizer::applyPRE(SSAContext* ctx, InstructionList& instructions,
                                const std::unordered_set<std::string>& sideEffectedFunctions)
{
    if (instructions.empty())
        return false;

    // Build CFG
    mBlockAnalyzer.identifyBasicBlocks(instructions);
    mBlockAnalyzer.buildControlFlowGraph(instructions);

    const auto& blocks = mBlockAnalyzer.getBasicBlocks();
    if (blocks.size() < 2)
        return false; // No cross-block opportunities with a single block

    // Phase 1: Identify expressions and build local sets
    identifyExpressions(instructions, blocks, sideEffectedFunctions);

    if (mExpressions.empty())
        return false;

    // Phase 2-3: Dataflow analysis
    computeAnticipated(blocks);
    computeAvailable(blocks);

    // Phase 4: Find insertion/deletion opportunities and apply
    return applyTransformations(ctx, instructions, blocks);
}

// ------------------------------------------------------------------
// Phase 1: Expression identification and local property computation
// ------------------------------------------------------------------

std::optional<SSCPPreOptimizer::ExpressionHash>
SSCPPreOptimizer::hashInstruction(const SSAInstr* instr) const
{
    if (!instr)
        return std::nullopt;

    type::Type type = type::Type(type::TypeKind::Unspecified);

    if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instr)) {
        if (asg->Operator == SSAInstrAssign::OpKind::Assign)
            return std::nullopt; // Simple copy — not a computation
        type = asg->Target.type();
    } else if (const auto call = dynamic_cast<const SSAInstrCall*>(instr)) {
        type = call->Target.type();
    } else {
        return std::nullopt;
    }

    return ExpressionHash{ instr->hash(false), type };
}

void SSCPPreOptimizer::collectOperandNames(const SSAInstr* instr,
                                            std::unordered_set<std::string>& names) const
{
    instr->forEachOperand([&](const SSAValue& val) {
        if (!val.isConstant())
            names.insert(val.name());
    });
}

void SSCPPreOptimizer::identifyExpressions(const InstructionList& instructions,
                                            const std::vector<BasicBlock>& blocks,
                                            const std::unordered_set<std::string>& sideEffectedFunctions)
{
    mExpressions.clear();
    mHashToExprIndex.clear();

    const size_t numBlocks = blocks.size();

    // First pass: discover all unique expressions
    for (const auto& instrPtr : instructions) {
        if (!instrPtr)
            continue;

        // Skip side-effecting calls
        if (const auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
            if (sideEffectedFunctions.contains(call->FunctionName))
                continue;
        }

        auto hash = hashInstruction(instrPtr.get());
        if (!hash)
            continue;

        if (mHashToExprIndex.find(*hash) == mHashToExprIndex.end()) {
            size_t idx              = mExpressions.size();
            mHashToExprIndex[*hash] = idx;

            ExpressionInfo info;
            info.hash     = *hash;
            info.exemplar = instrPtr;
            info.ueExpr.assign(numBlocks, false);
            info.deExpr.assign(numBlocks, false);
            info.exprKill.assign(numBlocks, false);
            info.antIn.assign(numBlocks, false);
            info.antOut.assign(numBlocks, false);
            info.availIn.assign(numBlocks, false);
            info.availOut.assign(numBlocks, false);
            mExpressions.push_back(std::move(info));
        }
    }

    // Second pass: compute per-block local properties (UEExpr, DEExpr, ExprKill)
    for (auto& expr : mExpressions) {
        std::unordered_set<std::string> operandNames;
        collectOperandNames(expr.exemplar.get(), operandNames);

        for (size_t bi = 0; bi < numBlocks; ++bi) {
            const auto& block = blocks[bi];

            bool killed      = false;
            bool generated   = false;
            bool ueGenerated = false;

            for (size_t ii = block.startIndex; ii < block.endIndex && ii < instructions.size(); ++ii) {
                const auto& instr = instructions[ii];
                if (!instr)
                    continue;

                // Check if this instruction computes our expression
                auto h = hashInstruction(instr.get());
                if (h && *h == expr.hash) {
                    generated = true;
                    if (!killed)
                        ueGenerated = true;
                }

                // Check if this instruction kills any operand
                instr->forEachTarget([&](const SSAValue& target) {
                    if (!target.isConstant() && operandNames.contains(target.name())) {
                        killed    = true;
                        generated = false;
                    }
                });
            }

            expr.ueExpr[bi]   = ueGenerated;
            expr.deExpr[bi]   = generated;
            expr.exprKill[bi] = killed;
        }
    }
}

// ------------------------------------------------------------------
// Phase 2: Anticipated expressions (backward dataflow)
// ------------------------------------------------------------------

void SSCPPreOptimizer::computeAnticipated(const std::vector<BasicBlock>& blocks)
{
    const size_t N = blocks.size();

    for (auto& expr : mExpressions) {
        std::fill(expr.antIn.begin(), expr.antIn.end(), true);
        std::fill(expr.antOut.begin(), expr.antOut.end(), true);

        for (size_t bi = 0; bi < N; ++bi) {
            if (blocks[bi].successors.empty())
                expr.antOut[bi] = false;
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t bi = N; bi-- > 0;) {
                bool newAntOut = true;
                if (blocks[bi].successors.empty()) {
                    newAntOut = false;
                } else {
                    for (size_t succ : blocks[bi].successors)
                        newAntOut = newAntOut && expr.antIn[succ];
                }

                bool newAntIn = expr.ueExpr[bi] || (newAntOut && !expr.exprKill[bi]);

                if (newAntOut != expr.antOut[bi] || newAntIn != expr.antIn[bi]) {
                    expr.antOut[bi] = newAntOut;
                    expr.antIn[bi]  = newAntIn;
                    changed         = true;
                }
            }
        }
    }
}

// ------------------------------------------------------------------
// Phase 3: Available expressions (forward dataflow)
// ------------------------------------------------------------------

void SSCPPreOptimizer::computeAvailable(const std::vector<BasicBlock>& blocks)
{
    const size_t N = blocks.size();

    for (auto& expr : mExpressions) {
        std::fill(expr.availIn.begin(), expr.availIn.end(), true);
        std::fill(expr.availOut.begin(), expr.availOut.end(), true);

        if (N > 0)
            expr.availIn[0] = false;

        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t bi = 0; bi < N; ++bi) {
                bool newAvailIn;
                if (bi == 0 || blocks[bi].predecessors.empty()) {
                    newAvailIn = false;
                } else {
                    newAvailIn = true;
                    for (size_t pred : blocks[bi].predecessors)
                        newAvailIn = newAvailIn && expr.availOut[pred];
                }

                bool newAvailOut = expr.deExpr[bi] || (newAvailIn && !expr.exprKill[bi]);

                if (newAvailIn != expr.availIn[bi] || newAvailOut != expr.availOut[bi]) {
                    expr.availIn[bi]  = newAvailIn;
                    expr.availOut[bi] = newAvailOut;
                    changed           = true;
                }
            }
        }
    }
}

// ------------------------------------------------------------------
// Phase 4: Identify opportunities and apply transformations
// ------------------------------------------------------------------
// Two types of optimizations:
//
// A) Global CSE: If expression is computed in block B and AvailIn(B) = true,
//    replace with the available value (expression already computed on all paths).
//
// B) Partial redundancy: If expression is computed in block B and some (but not all)
//    predecessors have AvailOut = true, insert on missing predecessor paths.
//    Requires: expression is anticipated on those paths (safe to compute).
//
// C) Hoisting: If AntOut(B) = true (expression anticipated on all forward paths)
//    and AvailOut(B) = false and the expression's operands are not killed in B,
//    insert at end of B. The fixpoint loop then eliminates successor computations
//    via Case A on the next iteration. Handles diamond CFG patterns.

bool SSCPPreOptimizer::applyTransformations(SSAContext* ctx, InstructionList& instructions,
                                             const std::vector<BasicBlock>& blocks)
{
    bool anyChanged    = false;
    const size_t numBlocks = blocks.size();

    using Insertion = std::pair<size_t, std::shared_ptr<SSAInstr>>;

    for (auto& expr : mExpressions) {
        std::vector<Insertion> insertions;
        std::string preName;
        SSAValue preTarget;
        bool preNameCreated = false;

        auto ensurePreName = [&]() {
            if (!preNameCreated) {
                type::Type preType;
                if (const auto asg = dynamic_cast<const SSAInstrAssign*>(expr.exemplar.get()))
                    preType = asg->Target.type();
                else if (const auto call = dynamic_cast<const SSAInstrCall*>(expr.exemplar.get()))
                    preType = call->Target.type();
                else
                    preType = expr.hash.type;

                preName        = ctx->fresh("%pre");
                preTarget      = SSAValue::Named(preName, preType);
                preNameCreated = true;
            }
        };

        for (size_t bi = 0; bi < numBlocks; ++bi) {
            if (!expr.ueExpr[bi])
                continue; // Expression not computed in this block

            // --- Case A: Fully available (global CSE) ---
            if (expr.availIn[bi]) {
                // Expression is available on ALL paths to this block.
                // We need to find which predecessor's computation provides the value.
                // For now, we can't easily name the available value because it might
                // have different SSA names on different paths. We need a PRE temp.
                //
                // Strategy: assign PRE temp at each predecessor's computation,
                // then replace this computation with the PRE temp.

                ensurePreName();

                // Add "preTarget = originalTarget" after each predecessor's computation
                for (size_t pred : blocks[bi].predecessors) {
                    if (!expr.availOut[pred])
                        continue;
                    addCopyAfterComputation(expr, pred, preTarget, instructions, blocks, insertions);
                }

                // Replace this block's computation
                replaceComputation(expr, bi, preTarget, instructions, blocks);
                anyChanged = true;
                continue;
            }

            // --- Case B: Partially available ---
            // Check if the expression is available on SOME paths but not all
            if (blocks[bi].predecessors.size() < 2)
                continue; // Need multiple paths for partial redundancy

            bool someAvail = false;
            bool allAvail  = true;
            for (size_t pred : blocks[bi].predecessors) {
                if (expr.availOut[pred])
                    someAvail = true;
                else
                    allAvail = false;
            }

            if (!someAvail || allAvail)
                continue; // Either nothing available or fully available (handled above)

            // Check: can we safely insert on missing paths?
            // The expression must be anticipated at the end of each missing predecessor.
            bool canInsert = true;
            for (size_t pred : blocks[bi].predecessors) {
                if (!expr.availOut[pred] && !expr.antOut[pred]) {
                    canInsert = false;
                    break;
                }
            }

            if (!canInsert)
                continue;

            ensurePreName();

            // Insert computation on missing predecessor paths
            for (size_t pred : blocks[bi].predecessors) {
                if (expr.availOut[pred]) {
                    // Already available — add copy to PRE temp after existing computation
                    addCopyAfterComputation(expr, pred, preTarget, instructions, blocks, insertions);
                } else {
                    // Missing — insert full computation
                    auto clone = cloneExemplar(expr, preTarget);
                    if (!clone)
                        continue;

                    // Insert before the block's terminator
                    size_t insertIdx = blocks[pred].endIndex;
                    if (insertIdx > blocks[pred].startIndex) {
                        size_t lastIdx = insertIdx - 1;
                        if (lastIdx < instructions.size()) {
                            const auto& lastInstr = instructions[lastIdx];
                            if (dynamic_cast<const SSAInstrBranch*>(lastInstr.get())
                                || dynamic_cast<const SSAInstrGoto*>(lastInstr.get())
                                || dynamic_cast<const SSAInstrReturn*>(lastInstr.get())) {
                                insertIdx = lastIdx;
                            }
                        }
                    }
                    insertions.emplace_back(insertIdx, std::move(clone));
                }
            }

            // Replace this block's computation with the PRE temp
            replaceComputation(expr, bi, preTarget, instructions, blocks);
            anyChanged = true;
        }

        // --- Case C: Hoisting ---
        // If expression is anticipated at exit of block B (will be computed on all
        // forward paths) but not available at exit, and operands are not killed in B,
        // insert at end of B. Successor computations become globally available on the
        // next fixpoint iteration and get eliminated by Case A.
        for (size_t bi = 0; bi < numBlocks; ++bi) {
            if (!expr.antOut[bi])
                continue; // Not anticipated on all paths from here
            if (expr.availOut[bi])
                continue; // Already available — nothing to hoist
            if (expr.deExpr[bi])
                continue; // Already computed (and not killed) in this block — already available

            // Verify: all successors must compute the expression (upward exposed)
            // to ensure we're actually eliminating redundant work
            if (blocks[bi].successors.empty())
                continue;

            bool allSuccsCompute = true;
            for (size_t succ : blocks[bi].successors) {
                if (!expr.ueExpr[succ]) {
                    allSuccsCompute = false;
                    break;
                }
            }

            if (!allSuccsCompute)
                continue;

            ensurePreName();

            auto clone = cloneExemplar(expr, preTarget);
            if (!clone)
                continue;

            // Insert before the terminator
            size_t insertIdx = blocks[bi].endIndex;
            if (insertIdx > blocks[bi].startIndex) {
                size_t lastIdx = insertIdx - 1;
                if (lastIdx < instructions.size()) {
                    const auto& lastInstr = instructions[lastIdx];
                    if (dynamic_cast<const SSAInstrBranch*>(lastInstr.get())
                        || dynamic_cast<const SSAInstrGoto*>(lastInstr.get())
                        || dynamic_cast<const SSAInstrReturn*>(lastInstr.get())) {
                        insertIdx = lastIdx;
                    }
                }
            }
            insertions.emplace_back(insertIdx, std::move(clone));

            // Replace computations in all successors with the PRE temp
            for (size_t succ : blocks[bi].successors)
                replaceComputation(expr, succ, preTarget, instructions, blocks);

            anyChanged = true;
        }

        // Apply insertions in reverse order to preserve indices
        std::sort(insertions.begin(), insertions.end(),
                  [](const Insertion& a, const Insertion& b) { return a.first > b.first; });

        for (auto& ins : insertions) {
            size_t idx = std::min(ins.first, instructions.size());
            instructions.insert(instructions.begin() + idx, std::move(ins.second));
            anyChanged = true;
        }
    }

    return anyChanged;
}

std::shared_ptr<SSAInstr> SSCPPreOptimizer::cloneExemplar(const ExpressionInfo& expr,
                                                           const SSAValue& target) const
{
    if (const auto asg = dynamic_cast<const SSAInstrAssign*>(expr.exemplar.get())) {
        auto clone      = std::make_shared<SSAInstrAssign>();
        clone->Target   = target;
        clone->Operator = asg->Operator;
        clone->UnaryOp  = asg->UnaryOp;
        clone->BinaryOp = asg->BinaryOp;
        clone->Operands = asg->Operands;
        return clone;
    }

    if (const auto call = dynamic_cast<const SSAInstrCall*>(expr.exemplar.get())) {
        auto clone                = std::make_shared<SSAInstrCall>();
        clone->Target             = target;
        clone->FunctionName       = call->FunctionName;
        clone->PublicFunctionName = call->PublicFunctionName;
        clone->Arguments          = call->Arguments;
        return clone;
    }

    return nullptr;
}

void SSCPPreOptimizer::addCopyAfterComputation(const ExpressionInfo& expr, size_t blockIdx,
                                                const SSAValue& preTarget,
                                                const InstructionList& instructions,
                                                const std::vector<BasicBlock>& blocks,
                                                std::vector<std::pair<size_t, std::shared_ptr<SSAInstr>>>& insertions) const
{
    const auto& block = blocks[blockIdx];
    // Find the last computation of this expression in the block (scan backwards)
    for (size_t ii = block.endIndex; ii-- > block.startIndex;) {
        if (ii >= instructions.size())
            continue;
        const auto& instr = instructions[ii];
        if (!instr)
            continue;

        auto h = hashInstruction(instr.get());
        if (!h || !(*h == expr.hash))
            continue;

        // Get the original target value
        SSAValue originalTarget;
        if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instr.get()))
            originalTarget = asg->Target;
        else if (const auto call = dynamic_cast<const SSAInstrCall*>(instr.get()))
            originalTarget = call->Target;
        else
            continue;

        // Insert: preTarget = originalTarget
        auto copy      = std::make_shared<SSAInstrAssign>();
        copy->Target   = preTarget;
        copy->Operator = SSAInstrAssign::OpKind::Assign;
        copy->Operands = { originalTarget };
        insertions.emplace_back(ii + 1, std::move(copy));
        return;
    }
}

void SSCPPreOptimizer::replaceComputation(const ExpressionInfo& expr, size_t blockIdx,
                                           const SSAValue& preTarget,
                                           InstructionList& instructions,
                                           const std::vector<BasicBlock>& blocks) const
{
    const auto& block = blocks[blockIdx];
    for (size_t ii = block.startIndex; ii < block.endIndex && ii < instructions.size(); ++ii) {
        auto& instr = instructions[ii];
        if (!instr)
            continue;

        auto h = hashInstruction(instr.get());
        if (!h || !(*h == expr.hash))
            continue;

        SSAValue originalTarget;
        if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instr.get()))
            originalTarget = asg->Target;
        else if (const auto call = dynamic_cast<const SSAInstrCall*>(instr.get()))
            originalTarget = call->Target;
        else
            continue;

        auto newAsg      = std::make_shared<SSAInstrAssign>();
        newAsg->Target   = originalTarget;
        newAsg->Operator = SSAInstrAssign::OpKind::Assign;
        newAsg->Operands = { preTarget };
        instr            = std::move(newAsg);
        return;
    }
}

} // namespace PExpr::opt
