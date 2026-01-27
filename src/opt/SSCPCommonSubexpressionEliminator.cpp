#include "SSCPCommonSubexpressionEliminator.h"

#include <cstdint>
#include <functional>
#include <ranges>
#include <sstream>

namespace PExpr::opt {
using namespace ssa;

bool SSCPCommonSubexpressionEliminator::applyCSEToRange(SSAContext* ctx, InstructionList::iterator begin, InstructionList::iterator end, const std::unordered_set<std::string>& sideEffectedFunctions)
{
    PEXPR_UNUSED(ctx);

    // Clear previous state
    mExpressionMap.clear();

    bool changed = false;

    for (auto it = begin; it != end; ++it) {
        auto& instrPtr = *it;
        if (!instrPtr)
            continue;

        // Skip instructions that don't produce values
        if (dynamic_cast<const SSAInstrBranch*>(instrPtr.get())
            || dynamic_cast<const SSAInstrGoto*>(instrPtr.get())
            || dynamic_cast<const SSAInstrLabel*>(instrPtr.get())
            || dynamic_cast<const SSAInstrReturn*>(instrPtr.get())) {
            continue;
        }

        // Get hash for this instruction
        std::vector<ExpressionHash> currentHashes;
        std::string targetName;

        if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instrPtr.get())) {
            // Skip simple assignments (they don't compute values)
            if (asg->Operator == SSAInstrAssign::OpKind::Assign)
                continue;

            const auto hash = hashInstruction(asg);
            if (hash) {
                currentHashes.push_back(*hash);

                // Check for commutativity stuff
                if (asg->Operator == SSAInstrAssign::OpKind::Binary) {
                    if (asg->BinaryOp == ast::BinaryOperation::Add
                        || asg->BinaryOp == ast::BinaryOperation::Mul
                        || asg->BinaryOp == ast::BinaryOperation::And
                        || asg->BinaryOp == ast::BinaryOperation::Or
                        || asg->BinaryOp == ast::BinaryOperation::Equal
                        || asg->BinaryOp == ast::BinaryOperation::NotEqual) {
                        SSAInstrAssign copy = *asg;
                        std::swap(copy.Operands[0], copy.Operands[1]);
                        const auto cumHash = hashInstruction(&copy);
                        if (cumHash)
                            currentHashes.push_back(*cumHash);
                    }
                }
            }
            targetName = asg->Target.name();
        } else if (const auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
            // Skip calls with side effects
            if (sideEffectedFunctions.contains(call->FunctionName))
                continue;

            const auto hash = hashInstruction(call);
            if (hash)
                currentHashes.push_back(*hash);
            targetName = call->Target.name();
        } else if (const auto phi = dynamic_cast<const SSAInstrPhi*>(instrPtr.get())) {
            // Skip phi nodes (they're too complex for CSE and handled by PRE)
            PEXPR_UNUSED(phi);
            continue;
        }

        for (const auto& hash : currentHashes) {
            // Check if we've seen this expression before
            if (const auto itMap = mExpressionMap.find(hash); itMap != mExpressionMap.end()) {
                // Found a duplicate expression! Replace with reference to previous result
                const SSAValue& existingValue = itMap->second;

                // Don't replace with ourselves
                if (!existingValue.isConstant() && existingValue.name() == targetName)
                    continue;

                // Create a new assignment: target = existingValue
                auto newAsg      = std::make_shared<SSAInstrAssign>();
                newAsg->Target   = SSAValue::Named(targetName, existingValue.type());
                newAsg->Operator = SSAInstrAssign::OpKind::Assign;
                newAsg->Operands = { existingValue };

                // Replace instruction
                instrPtr = std::move(newAsg);
                changed  = true;
            } else {
                // First time seeing this expression, add to map
                if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instrPtr.get()))
                    mExpressionMap[hash] = asg->Target;
                else if (const auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get()))
                    mExpressionMap[hash] = call->Target;
            }
        }
    }

    return changed;
}

bool SSCPCommonSubexpressionEliminator::applyCSE(SSAContext* ctx, InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions)
{
    mBlockAnalyzer.identifyBasicBlocks(instructions);
    // mBlockAnalyzer.buildControlFlowGraph(instructions); // < Not needed here

    bool changed = false;

    // Process each basic block separately
    const auto& blocks = mBlockAnalyzer.getBasicBlocks();
    for (size_t blockIdx = 0; blockIdx < blocks.size(); ++blockIdx) {
        const auto& block = blocks[blockIdx];
        if (block.startIndex >= instructions.size()
            || block.endIndex > instructions.size()
            || block.startIndex >= block.endIndex)
            continue;

        // Apply CSE to this basic block range
        auto blockBegin   = instructions.begin() + block.startIndex;
        auto blockEnd     = instructions.begin() + block.endIndex;
        bool blockChanged = applyCSEToRange(ctx, blockBegin, blockEnd, sideEffectedFunctions);

        if (blockChanged)
            changed = true;
    }

    return changed;
}

std::optional<SSCPCommonSubexpressionEliminator::ExpressionHash>
SSCPCommonSubexpressionEliminator::hashInstruction(const SSAInstr* instr) const
{
    if (!instr)
        return std::nullopt;

    // For CSE, we don't want to include target names in the hash
    // because identical expressions with different target names should be eliminated
    size_t hash     = instr->hash(false);
    type::Type type = type::Type(type::TypeKind::Unspecified);

    // Get the result type from the instruction
    if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instr)) {
        type = asg->Target.type();
    } else if (const auto call = dynamic_cast<const SSAInstrCall*>(instr)) {
        type = call->Target.type();
    } else if (const auto phi = dynamic_cast<const SSAInstrPhi*>(instr)) {
        type = phi->Target.type();
    } else {
        // Other instruction types don't produce values for CSE
        return std::nullopt;
    }

    return ExpressionHash{ hash, type };
}

} // namespace PExpr::opt