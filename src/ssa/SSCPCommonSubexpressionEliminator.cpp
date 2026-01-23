#include "SSCPCommonSubexpressionEliminator.h"
#include "SSAMapper.h"

#include <cstdint>
#include <functional>
#include <sstream>

namespace PExpr::ssa {

bool SSCPCommonSubexpressionEliminator::applyCSE(SSAContext* ctx, InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions)
{
    PEXPR_UNUSED(ctx);

    // Clear previous state
    mExpressionMap.clear();
    mValueHashes.clear();

    bool changed = false;

    // First pass: compute hashes for all values
    for (const auto& instrPtr : instructions) {
        if (!instrPtr)
            continue;

        // Compute hash for the instruction if it produces a value
        if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instrPtr.get())) {
            if (auto hash = hashAssign(asg))
                mValueHashes[asg->Target.Name] = *hash;
        } else if (const auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
            if (auto hash = hashCall(call))
                mValueHashes[call->Target.Name] = *hash;
        } else if (const auto phi = dynamic_cast<const SSAInstrPhi*>(instrPtr.get())) {
            // Phi nodes are more complex - skip for now or implement special handling
            // mValueHashes[phi->Target.Name] = hashPhi(phi->Conditions, phi->Branches);
            PEXPR_UNUSED(phi);
        }
    }

    // Second pass: eliminate common subexpressions
    for (auto& instrPtr : instructions) {
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
        std::optional<ExpressionHash> currentHash;
        std::string targetName;

        if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instrPtr.get())) {
            currentHash = hashAssign(asg);
            targetName  = asg->Target.Name;
        } else if (const auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
            // Skip calls with side effects
            if (sideEffectedFunctions.contains(call->FunctionName))
                continue;

            currentHash = hashCall(call);
            targetName  = call->Target.Name;
        } else if (const auto phi = dynamic_cast<const SSAInstrPhi*>(instrPtr.get())) {
            PEXPR_UNUSED(phi);
            // Skip phi nodes for now
            continue;
        }

        if (!currentHash)
            continue;

        // Check if we've seen this expression before
        if (const auto it = mExpressionMap.find(*currentHash); it != mExpressionMap.end()) {
            // Found a duplicate expression! Replace with reference to previous result
            const SSAValue& existingValue = it->second;

            // Don't replace with ourselves
            if (existingValue.Name == targetName)
                continue;

            // Create a new assignment: target = existingValue
            auto newAsg         = std::make_shared<SSAInstrAssign>();
            newAsg->Target.Name = targetName;
            newAsg->Target.Type = existingValue.Type;
            newAsg->Target.Kind = SSAValue::Kind::Named;
            newAsg->Operator    = SSAInstrAssign::OpKind::Assign;
            newAsg->Operands    = { existingValue };

            // Update our maps
            mValueHashes[targetName] = *currentHash;

            // Replace instruction
            instrPtr = std::move(newAsg);
            changed  = true;
        } else {
            // First time seeing this expression, add to map
            if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instrPtr.get()))
                mExpressionMap[*currentHash] = asg->Target;
            else if (const auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get()))
                mExpressionMap[*currentHash] = call->Target;
        }
    }

    return changed;
}

std::optional<SSCPCommonSubexpressionEliminator::ExpressionHash>
SSCPCommonSubexpressionEliminator::hashInstruction(const SSAInstr* instr) const
{
    if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instr))
        return hashAssign(asg);
    else if (const auto call = dynamic_cast<const SSAInstrCall*>(instr))
        return hashCall(call);
    return std::nullopt;
}

std::optional<SSCPCommonSubexpressionEliminator::ExpressionHash>
SSCPCommonSubexpressionEliminator::hashAssign(const SSAInstrAssign* asg) const
{
    if (!asg)
        return std::nullopt;

    size_t hash         = 0;
    ElementaryType type = asg->Target.Type;

    switch (asg->Operator) {
    case SSAInstrAssign::OpKind::Assign:
        // Ignore assigns as they have no real value beside keeping the variables alive.
        // Tracking these would introduce an infinite loop.
        return std::nullopt;

    case SSAInstrAssign::OpKind::Unary:
        if (asg->Operands.size() != 1)
            return std::nullopt;
        hash = hashUnaryOp(asg->UnaryOp, asg->Operands[0]);
        break;

    case SSAInstrAssign::OpKind::Binary:
        if (asg->Operands.size() != 2)
            return std::nullopt;
        hash = hashBinaryOp(asg->BinaryOp, asg->Operands[0], asg->Operands[1]);
        break;

    case SSAInstrAssign::OpKind::Swizzle:
        if (asg->Operands.size() != 1)
            return std::nullopt;
        hash = hashSwizzle(asg->Operands[0], asg->Swizzle);
        break;

    case SSAInstrAssign::OpKind::Access:
        if (asg->Operands.size() != 2)
            return std::nullopt;
        hash = hashAccess(asg->Operands[0], asg->Operands[1]);
        break;

    case SSAInstrAssign::OpKind::Vector:
        hash = hashVector(asg->Operands);
        break;

    case SSAInstrAssign::OpKind::Cast:
        if (asg->Operands.size() != 1)
            return std::nullopt;
        hash = hashCast(asg->Operands[0], asg->Target.Type);
        break;

    case SSAInstrAssign::OpKind::Nop:
        // Nop produces no value, shouldn't be hashed
        return std::nullopt;

    case SSAInstrAssign::OpKind::Phi:
        // Phi nodes handled separately
        return std::nullopt;
    }

    return ExpressionHash{ hash, type };
}

std::optional<SSCPCommonSubexpressionEliminator::ExpressionHash>
SSCPCommonSubexpressionEliminator::hashCall(const SSAInstrCall* call) const
{
    if (!call)
        return std::nullopt;

    // Start with function name hash
    size_t hash = hashString(call->FunctionName);

    // Combine with argument hashes
    for (const auto& arg : call->Arguments)
        hash = combineHashes(hash, hashValue(arg));

    return ExpressionHash{ hash, call->Target.Type };
}

size_t SSCPCommonSubexpressionEliminator::hashBinaryOp(BinaryOperation op, const SSAValue& left, const SSAValue& right) const
{
    size_t hash = std::hash<int>{}(static_cast<int>(op));
    hash        = combineHashes(hash, hashValue(left));
    hash        = combineHashes(hash, hashValue(right));
    return hash;
}

size_t SSCPCommonSubexpressionEliminator::hashUnaryOp(UnaryOperation op, const SSAValue& operand) const
{
    size_t hash = std::hash<int>{}(static_cast<int>(op));
    hash        = combineHashes(hash, hashValue(operand));
    return hash;
}

size_t SSCPCommonSubexpressionEliminator::hashSwizzle(const SSAValue& operand, const std::string& swizzle) const
{
    size_t hash = hashValue(operand);
    hash        = combineHashes(hash, hashString(swizzle));
    return hash;
}

size_t SSCPCommonSubexpressionEliminator::hashAccess(const SSAValue& operand, const SSAValue& index) const
{
    size_t hash = hashValue(operand);
    hash        = combineHashes(hash, hashValue(index));
    return hash;
}

size_t SSCPCommonSubexpressionEliminator::hashVector(const std::vector<SSAValue>& operands) const
{
    size_t hash = std::hash<size_t>{}(operands.size());
    for (const auto& op : operands)
        hash = combineHashes(hash, hashValue(op));
    return hash;
}

size_t SSCPCommonSubexpressionEliminator::hashCast(const SSAValue& operand, ElementaryType targetType) const
{
    size_t hash = std::hash<int>{}(static_cast<int>(targetType));
    hash        = combineHashes(hash, hashValue(operand));
    return hash;
}

size_t SSCPCommonSubexpressionEliminator::hashPhi(const std::vector<SSAValue>& conditions, const std::vector<SSAValue>& branches) const
{
    // Phi nodes are complex - we need to consider that the same phi
    // with different incoming edges might be equivalent
    // For now, use a simple hash that might have false positives
    size_t hash = 0;
    for (const auto& cond : conditions)
        hash = combineHashes(hash, hashValue(cond));
    for (const auto& branch : branches)
        hash = combineHashes(hash, hashValue(branch));
    return hash;
}

size_t SSCPCommonSubexpressionEliminator::hashValue(const SSAValue& val) const
{
    if (val.Kind == SSAValue::Kind::Constant) {
        return hashConstant(val.Value, val.Type);
    } else {
        // For variable references, use their expression hash if available
        if (const auto it = mValueHashes.find(val.Name); it != mValueHashes.end())
            return it->second.hash;

        // Fallback: hash the name
        return hashString(val.Name);
    }
}

size_t SSCPCommonSubexpressionEliminator::hashConstant(const ExtendedValueVariant& val, ElementaryType type) const
{
    size_t hash = std::hash<int>{}(static_cast<int>(type));

    // Hash based on type
    if (type == ElementaryType::Boolean) {
        if (const bool* b = std::get_if<bool>(&val))
            hash = combineHashes(hash, std::hash<bool>{}(*b));
    } else if (type == ElementaryType::Integer) {
        if (const Integer* i = std::get_if<Integer>(&val))
            hash = combineHashes(hash, std::hash<Integer>{}(*i));
    } else if (type == ElementaryType::Number) {
        if (const Number* n = std::get_if<Number>(&val))
            hash = combineHashes(hash, std::hash<Number>{}(*n));
    } else if (type == ElementaryType::String) {
        if (const std::string* s = std::get_if<std::string>(&val))
            hash = combineHashes(hash, hashString(*s));
    } else if (type >= ElementaryType::Vec1) {
        // Vector types: Vec2 = Vec1 + 1, Vec3 = Vec1 + 2, Vec4 = Vec1 + 3
        if (const VecN* v = std::get_if<VecN>(&val)) {
            for (Number n : *v)
                hash = combineHashes(hash, std::hash<Number>{}(n));
        }
    }

    return hash;
}

size_t SSCPCommonSubexpressionEliminator::hashString(const std::string& str) const
{
    return std::hash<std::string>{}(str);
}

bool SSCPCommonSubexpressionEliminator::areInstructionsEquivalent(const SSAInstr* a, const SSAInstr* b) const
{
    if (!a || !b)
        return false;

    if (typeid(*a) != typeid(*b))
        return false;

    if (const auto asgA = dynamic_cast<const SSAInstrAssign*>(a)) {
        if (const auto asgB = dynamic_cast<const SSAInstrAssign*>(b))
            return areAssignsEquivalent(asgA, asgB);
    } else if (const auto callA = dynamic_cast<const SSAInstrCall*>(a)) {
        if (const auto callB = dynamic_cast<const SSAInstrCall*>(b))
            return areCallsEquivalent(callA, callB);
    }

    return false;
}

bool SSCPCommonSubexpressionEliminator::areAssignsEquivalent(const SSAInstrAssign* a, const SSAInstrAssign* b) const
{
    if (!a || !b)
        return false;

    if (a->Operator != b->Operator)
        return false;

    if (a->Operands.size() != b->Operands.size())
        return false;

    // Check operator-specific fields
    switch (a->Operator) {
    case SSAInstrAssign::OpKind::Unary:
        if (a->UnaryOp != b->UnaryOp)
            return false;
        break;

    case SSAInstrAssign::OpKind::Binary:
        if (a->BinaryOp != b->BinaryOp)
            return false;
        break;

    case SSAInstrAssign::OpKind::Swizzle:
        if (a->Swizzle != b->Swizzle)
            return false;
        break;

    case SSAInstrAssign::OpKind::Cast:
        if (a->Target.Type != b->Target.Type)
            return false;
        break;

    default:
        break;
    }

    // Check operands
    for (size_t i = 0; i < a->Operands.size(); ++i) {
        if (!areValuesEquivalent(a->Operands[i], b->Operands[i]))
            return false;
    }

    return true;
}

bool SSCPCommonSubexpressionEliminator::areCallsEquivalent(const SSAInstrCall* a, const SSAInstrCall* b) const
{
    if (!a || !b)
        return false;

    if (a->FunctionName != b->FunctionName)
        return false;

    if (a->Arguments.size() != b->Arguments.size())
        return false;

    for (size_t i = 0; i < a->Arguments.size(); ++i) {
        if (!areValuesEquivalent(a->Arguments[i], b->Arguments[i]))
            return false;
    }

    return true;
}

bool SSCPCommonSubexpressionEliminator::areValuesEquivalent(const SSAValue& a, const SSAValue& b) const
{
    if (a.Kind != b.Kind)
        return false;

    if (a.Type != b.Type)
        return false;

    if (a.Kind == SSAValue::Kind::Constant) {
        // Compare constant values
        // This is simplified - a full implementation would need to compare
        // the ExtendedValueVariant values properly
        return hashValue(a) == hashValue(b);
    } else {
        // For variables, check if they compute the same expression
        auto hashA = mValueHashes.find(a.Name);
        auto hashB = mValueHashes.find(b.Name);

        if (hashA != mValueHashes.end() && hashB != mValueHashes.end())
            return hashA->second.hash == hashB->second.hash;

        // Fallback: compare names
        return a.Name == b.Name;
    }
}

} // namespace PExpr::ssa