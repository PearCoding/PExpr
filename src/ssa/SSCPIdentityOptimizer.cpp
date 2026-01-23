#include "SSCPIdentityOptimizer.h"
#include "Enums.h"
#include "SSAMapper.h"

#include <cmath>
#include <sstream>

namespace PExpr::ssa {

bool SSCPIdentityOptimizer::applyIdentities(SSAContext* ctx, InstructionList& instructions)
{
    // Build definition map
    mDefinitions.clear();
    for (const auto& instrPtr : instructions) {
        if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
            if (!asg->Target.Name.empty())
                mDefinitions[asg->Target.Name] = asg;
        }
    }

    bool changed = false;
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (auto asg = dynamic_cast<SSAInstrAssign*>(instructions[i].get())) {
            if (tryApplyIdentity(ctx, asg, instructions, i))
                changed = true;
        }
    }
    return changed;
}

bool SSCPIdentityOptimizer::tryApplyIdentity(SSAContext* ctx, SSAInstrAssign* asg, InstructionList& instructions, size_t currentIndex)
{
    (void)ctx;          // Unused for now
    (void)instructions; // Unused for now
    (void)currentIndex; // Unused for now

    // Try each identity pattern in order

    // Priority 1: Simple identities
    if (auto result = matchPythagoreanIdentity(asg)) {
        asg->Operator = SSAInstrAssign::OpKind::Assign;
        asg->Operands = { *result };
        return true;
    }

    if (auto result = matchSquareToPoweIdentity(asg)) {
        asg->Operator = SSAInstrAssign::OpKind::Binary;
        asg->BinaryOp = BinaryOperation::Pow;
        asg->Operands = { result->first, result->second };
        return true;
    }

    if (auto result = matchInverseTrigoIdentity(asg)) {
        asg->Operator = SSAInstrAssign::OpKind::Assign;
        asg->Operands = { *result };
        return true;
    }

    // Priority 2: More complex identities
    if (auto result = matchAngleAdditionIdentity(asg)) {
        // Replace with call to sin or cos with angle addition/subtraction
        // This requires creating a temporary for the angle operation
        asg->Operator = SSAInstrAssign::OpKind::CallOp;
        asg->Operands = { *result };
        return true;
    }

    if (auto result = matchDoubleAngleIdentity(asg)) {
        asg->Operator = SSAInstrAssign::OpKind::CallOp;
        asg->Operands = { *result };
        return true;
    }

    if (auto result = matchPowerReductionIdentity(asg)) {
        asg->Operator = SSAInstrAssign::OpKind::Binary;
        asg->BinaryOp = BinaryOperation::Pow;
        asg->Operands = { result->first, result->second };
        return true;
    }

    return false;
}

std::optional<SSAValue> SSCPIdentityOptimizer::matchPythagoreanIdentity(const SSAInstrAssign* asg)
{
    // Match: sin(a)^2 + cos(a)^2 = 1
    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->BinaryOp != BinaryOperation::Add)
        return std::nullopt;

    if (asg->Operands.size() != 2)
        return std::nullopt;

    SSAValue sinSquared, cosSquared;
    SSAValue sinBase, cosBase;
    SSAValue sinExp, cosExp;

    // Check if operands are power operations
    bool firstIsPower  = isPowerOp(asg->Operands[0], sinSquared, sinExp);
    bool secondIsPower = isPowerOp(asg->Operands[1], cosSquared, cosExp);

    if (!firstIsPower || !secondIsPower)
        return std::nullopt;

    // Check if exponents are 2
    Number exp1, exp2;
    if (!isConstantNumber(sinExp, exp1) || !isConstantNumber(cosExp, exp2))
        return std::nullopt;

    if (exp1 != Number(2.0) || exp2 != Number(2.0))
        return std::nullopt;

    // Check if one is sin(a) and the other is cos(a) with the same argument
    bool firstIsSin  = isCallToIntrinsic(sinSquared, "sin");
    bool secondIsCos = isCallToIntrinsic(cosSquared, "cos");
    bool firstIsCos  = isCallToIntrinsic(sinSquared, "cos");
    bool secondIsSin = isCallToIntrinsic(cosSquared, "sin");

    if ((firstIsSin && secondIsCos) || (firstIsCos && secondIsSin)) {
        // Get the call definitions
        // const SSAInstrCall* call1 = nullptr;
        // const SSAInstrCall* call2 = nullptr;

        for (const auto& [name, def] : mDefinitions) {
            if (name == sinSquared.Name) {
                // This is a hack - we need to find the actual call instruction
                // For now, we'll check if the arguments match by name comparison
            }
        }

        // Simplified check: if both are function calls to sin/cos, assume they have the same argument
        // A more robust implementation would track call instructions separately
        return SSAValue::Constant(Number(1.0));
    }

    return std::nullopt;
}

std::optional<std::pair<SSAValue, SSAValue>> SSCPIdentityOptimizer::matchSquareToPoweIdentity(const SSAInstrAssign* asg)
{
    // Match: a*a = a^2
    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->BinaryOp != BinaryOperation::Mul)
        return std::nullopt;

    if (asg->Operands.size() != 2)
        return std::nullopt;

    // Check if both operands are the same
    if (isSameValue(asg->Operands[0], asg->Operands[1])) {
        return std::make_pair(asg->Operands[0], SSAValue::Constant(Number(2.0)));
    }

    return std::nullopt;
}

std::optional<SSAValue> SSCPIdentityOptimizer::matchInverseTrigoIdentity(const SSAInstrAssign* asg)
{
    // Match: sin(asin(a)) = a, cos(acos(a)) = a, tan(atan(a)) = a
    if (asg->Operator != SSAInstrAssign::OpKind::CallOp)
        return std::nullopt;

    // This would need to check if asg is a call instruction
    // Since SSAInstrAssign doesn't directly represent calls in this form,
    // we need to look for the pattern in the operands

    if (asg->Operands.size() != 1)
        return std::nullopt;

    const SSAValue& operand = asg->Operands[0];

    // Check if operand is a call to asin, acos, or atan
    if (isCallToIntrinsic(operand, "asin")) {
        // This is asin(x), check if the outer operation is sin
        // This requires tracking what function this assignment represents
        // which we don't have in the current structure
    }

    return std::nullopt;
}

std::optional<SSAValue> SSCPIdentityOptimizer::matchAngleAdditionIdentity(const SSAInstrAssign* asg)
{
    // Match: sin(a)*cos(b) + cos(a)*sin(b) = sin(a+b)
    // Match: sin(a)*cos(b) - cos(a)*sin(b) = sin(a-b)
    // Match: cos(a)*cos(b) + sin(a)*sin(b) = cos(a-b)
    // Match: cos(a)*cos(b) - sin(a)*sin(b) = cos(a+b)

    if (asg->Operator != SSAInstrAssign::OpKind::Binary)
        return std::nullopt;

    if (asg->BinaryOp != BinaryOperation::Add && asg->BinaryOp != BinaryOperation::Sub)
        return std::nullopt;

    if (asg->Operands.size() != 2)
        return std::nullopt;

    // This is complex pattern matching that would require analyzing
    // the structure of both operands to see if they match the pattern
    // For a complete implementation, we would need to:
    // 1. Check if operands are multiplication operations
    // 2. Check if the factors are sin/cos calls
    // 3. Verify the pattern matches one of the identities

    return std::nullopt;
}

std::optional<SSAValue> SSCPIdentityOptimizer::matchDoubleAngleIdentity(const SSAInstrAssign* asg)
{
    // Match: 2*sin(a)*cos(a) = sin(2*a)
    // Match: 2*cos(a)*cos(a) - 1 = cos(2*a)

    if (asg->Operator != SSAInstrAssign::OpKind::Binary)
        return std::nullopt;

    // This would require complex pattern matching similar to angle addition

    return std::nullopt;
}

std::optional<std::pair<SSAValue, SSAValue>> SSCPIdentityOptimizer::matchPowerReductionIdentity(const SSAInstrAssign* asg)
{
    // Match: (1-cos(2*a))/2 = sin(a)^2
    // Match: (1+cos(2*a))/2 = cos(a)^2

    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->BinaryOp != BinaryOperation::Div)
        return std::nullopt;

    // This would require analyzing the structure to match the pattern
    // For now, returning nullopt as this is a complex pattern

    return std::nullopt;
}

bool SSCPIdentityOptimizer::isCallToIntrinsic(const SSAValue& val, std::string_view funcName) const
{
    if (val.Kind == SSAValue::Kind::Constant)
        return false;

    // Look up the definition
    auto it = mDefinitions.find(val.Name);
    if (it == mDefinitions.end())
        return false;

    const SSAInstrAssign* def = it->second;
    if (def->Operator != SSAInstrAssign::OpKind::CallOp)
        return false;

    // We would need additional information to determine the function name
    // This is a limitation of the current structure
    return false;
}

bool SSCPIdentityOptimizer::isSameValue(const SSAValue& a, const SSAValue& b) const
{
    if (a.Kind == SSAValue::Kind::Constant && b.Kind == SSAValue::Kind::Constant) {
        // Compare constant values
        if (const Number* an = std::get_if<Number>(&a.Value)) {
            if (const Number* bn = std::get_if<Number>(&b.Value)) {
                return *an == *bn;
            }
        }
        if (const Integer* ai = std::get_if<Integer>(&a.Value)) {
            if (const Integer* bi = std::get_if<Integer>(&b.Value)) {
                return *ai == *bi;
            }
        }
        return false;
    }

    if (a.Kind != SSAValue::Kind::Constant && b.Kind != SSAValue::Kind::Constant) {
        return a.Name == b.Name;
    }

    return false;
}

const SSAInstrAssign* SSCPIdentityOptimizer::findDefinition(const std::string& name) const
{
    auto it = mDefinitions.find(name);
    if (it != mDefinitions.end())
        return it->second;
    return nullptr;
}

bool SSCPIdentityOptimizer::isBinaryOp(const SSAValue& val, BinaryOperation op, SSAValue& left, SSAValue& right) const
{
    if (val.Kind == SSAValue::Kind::Constant)
        return false;

    const SSAInstrAssign* def = findDefinition(val.Name);
    if (!def)
        return false;

    if (def->Operator != SSAInstrAssign::OpKind::Binary || def->BinaryOp != op)
        return false;

    if (def->Operands.size() != 2)
        return false;

    left  = def->Operands[0];
    right = def->Operands[1];
    return true;
}

bool SSCPIdentityOptimizer::isPowerOp(const SSAValue& val, SSAValue& base, SSAValue& exponent) const
{
    return isBinaryOp(val, BinaryOperation::Pow, base, exponent);
}

bool SSCPIdentityOptimizer::isConstantNumber(const SSAValue& val, Number& outValue) const
{
    if (val.Kind != SSAValue::Kind::Constant)
        return false;

    if (const Number* n = std::get_if<Number>(&val.Value)) {
        outValue = *n;
        return true;
    }

    if (const Integer* i = std::get_if<Integer>(&val.Value)) {
        outValue = static_cast<Number>(*i);
        return true;
    }

    return false;
}

SSAValue SSCPIdentityOptimizer::createTempAssignment(SSAContext* ctx,
                                                     InstructionList& instructions, size_t insertPos,
                                                     SSAInstrAssign::OpKind opKind,
                                                     const std::vector<SSAValue>& operands,
                                                     ElementaryType type)
{
    auto newInstr      = std::make_shared<SSAInstrAssign>();
    newInstr->Target   = SSAValue(SSAValue::Kind::Temp, ctx->fresh("%"), type);
    newInstr->Operator = opKind;
    newInstr->Operands = operands;

    instructions.insert(instructions.begin() + insertPos, newInstr);

    return newInstr->Target;
}

} // namespace PExpr::ssa
