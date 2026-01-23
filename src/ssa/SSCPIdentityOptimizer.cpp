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
        if (!instrPtr)
            continue;

        // TODO: Would be nice if this could be polymorphed out?
        if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instrPtr.get()))
            mDefinitions[asg->Target.Name] = asg;
        else if (const auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get()))
            mDefinitions[call->Target.Name] = call;
        else if (const auto phi = dynamic_cast<const SSAInstrPhi*>(instrPtr.get()))
            mDefinitions[phi->Target.Name] = phi;
    }

    bool changed = false;
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (tryApplyIdentity(ctx, instructions, i))
            changed = true;
    }
    return changed;
}

bool SSCPIdentityOptimizer::tryApplyIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex)
{
    // The following is always enabled as it clears it up internally and has no side-effects
    if (matchAssignIdentity(ctx, instructions, currentIndex))
        return true;

    if (mOptions.ApplyMathIdentities) {
        if (matchUnaryIdentity(ctx, instructions, currentIndex))
            return true;

        if (matchSquareToPowerIdentity(ctx, instructions, currentIndex))
            return true;
    }

    if (mOptions.ApplyTrigonometricIdentities) {
        if (matchPythagoreanIdentity(ctx, instructions, currentIndex))
            return true;

        if (matchInverseTrigonometricIdentity(ctx, instructions, currentIndex))
            return true;

        if (matchAngleAdditionIdentity(ctx, instructions, currentIndex))
            return true;

        if (matchDoubleAngleIdentity(ctx, instructions, currentIndex))
            return true;

        if (matchPowerReductionIdentity(ctx, instructions, currentIndex))
            return true;
    }

    return false;
}

bool SSCPIdentityOptimizer::matchAssignIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(instructions.at(currentIndex).get());
    if (!asg)
        return false;

    if (asg->Operator != SSAInstrAssign::OpKind::Assign || asg->Operands.size() != 1)
        return false;

    const auto asg2 = dynamic_cast<const SSAInstrAssign*>(getDefinition(asg->Operands[0]));

    if (!asg2)
        return false;

    if (asg2->Operator != SSAInstrAssign::OpKind::Assign || asg2->Operands.size() != 1)
        return false;

    auto newAsg      = std::make_shared<SSAInstrAssign>();
    newAsg->Target   = asg->Target;
    newAsg->Operator = SSAInstrAssign::OpKind::Assign;
    newAsg->Operands = { asg2->Operands[0] };

    mDefinitions[newAsg->Target.Name] = newAsg.get();
    instructions[currentIndex]        = std::move(newAsg);
    return true;
}

bool SSCPIdentityOptimizer::matchUnaryIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(instructions.at(currentIndex).get());
    if (!asg)
        return false;

    if (asg->Operator != SSAInstrAssign::OpKind::Unary || asg->Operands.size() != 1)
        return false;

    // +a = a | Essentially just syntatic sugar
    if (asg->UnaryOp != UnaryOperation::Pos) {
        auto newAsg      = std::make_shared<SSAInstrAssign>();
        newAsg->Target   = asg->Target;
        newAsg->Operator = SSAInstrAssign::OpKind::Assign;
        newAsg->Operands = { asg->Operands[0] };

        mDefinitions[newAsg->Target.Name] = newAsg.get();
        instructions[currentIndex]        = std::move(newAsg);
        return true;
    }

    const auto operand = dynamic_cast<const SSAInstrAssign*>(getDefinition(asg->Operands[0]));

    if (!operand)
        return false;

    // --a = a, !!a = a
    if (operand->Operator != SSAInstrAssign::OpKind::Unary || asg->UnaryOp != operand->UnaryOp || operand->Operands.size() != 1)
        return false;

    PEXPR_ASSERT(asg->UnaryOp == UnaryOperation::Neg || asg->UnaryOp == UnaryOperation::Not, "Unknown unary operator given");

    {
        auto newAsg      = std::make_shared<SSAInstrAssign>();
        newAsg->Target   = asg->Target;
        newAsg->Operator = SSAInstrAssign::OpKind::Assign;
        newAsg->Operands = { operand->Operands[0] };

        mDefinitions[newAsg->Target.Name] = newAsg.get();
        instructions[currentIndex]        = std::move(newAsg);
    }
    return true;
}

bool SSCPIdentityOptimizer::matchPythagoreanIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(instructions.at(currentIndex).get());
    if (!asg)
        return false;

    // Match: sin(a)^2 + cos(a)^2 = 1
    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->BinaryOp != BinaryOperation::Add)
        return false;

    if (asg->Operands.size() != 2)
        return false;

    SSAValue sinSquared, cosSquared;
    SSAValue sinBase, cosBase;
    SSAValue sinExp, cosExp;

    // Check if operands are power operations
    bool firstIsPower  = isPowerOp(asg->Operands[0], sinSquared, sinExp);
    bool secondIsPower = isPowerOp(asg->Operands[1], cosSquared, cosExp);

    if (!firstIsPower || !secondIsPower)
        return false;

    // Check if exponents are 2
    Number exp1, exp2;
    if (!isConstantNumber(sinExp, exp1) || !isConstantNumber(cosExp, exp2))
        return false;

    if (exp1 != Number(2.0) || exp2 != Number(2.0))
        return false;

    // Check if one is sin(a) and the other is cos(a) with the same argument
    bool firstIsSin  = isCallToIntrinsic(sinSquared, "sin");
    bool secondIsCos = isCallToIntrinsic(cosSquared, "cos");
    bool firstIsCos  = isCallToIntrinsic(sinSquared, "cos");
    bool secondIsSin = isCallToIntrinsic(cosSquared, "sin");

    if ((firstIsSin && secondIsCos) || (firstIsCos && secondIsSin)) {
        // Get the call definitions
        // const SSAInstrCall* call1 = nullptr;
        // const SSAInstrCall* call2 = nullptr;

        // TODO
    }

    return false;
}

bool SSCPIdentityOptimizer::matchSquareToPowerIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(instructions.at(currentIndex).get());
    if (!asg)
        return false;

    // Match: a*a = a^2
    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->BinaryOp != BinaryOperation::Mul)
        return false;

    if (asg->Operands.size() != 2)
        return false;

    // Check if both operands are the same
    if (isSameValue(asg->Operands[0], asg->Operands[1])) {
        auto newAsg      = std::make_shared<SSAInstrAssign>();
        newAsg->Target   = asg->Target;
        newAsg->Operator = SSAInstrAssign::OpKind::Binary;
        newAsg->BinaryOp = BinaryOperation::Pow;
        newAsg->Operands = { asg->Operands[0], SSAValue::Constant(Number(2.0)) };

        mDefinitions[newAsg->Target.Name] = newAsg.get();
        instructions[currentIndex]        = std::move(newAsg);
        return true;
    }

    return false;
}

bool SSCPIdentityOptimizer::matchInverseTrigonometricIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex)
{
    PEXPR_UNUSED(ctx);

    const auto call = dynamic_cast<const SSAInstrCall*>(instructions.at(currentIndex).get());
    if (!call)
        return false;

    // Match: sin(asin(a)) = a, cos(acos(a)) = a, tan(atan(a)) = a
    if (call->Arguments.size() != 1)
        return false;

    const SSAValue& operand = call->Arguments[0];

    // Check if operand is a call to asin, acos, or atan
    if ((isIntrinsic(call, "sin") && isCallToIntrinsic(operand, "asin"))
        || (isIntrinsic(call, "cos") && isCallToIntrinsic(operand, "acos"))
        || (isIntrinsic(call, "tan") && isCallToIntrinsic(operand, "atan"))
        || (isIntrinsic(call, "asin") && isCallToIntrinsic(operand, "sin"))
        || (isIntrinsic(call, "acos") && isCallToIntrinsic(operand, "cos"))
        || (isIntrinsic(call, "atan") && isCallToIntrinsic(operand, "tan"))) {

        if (const auto call2 = dynamic_cast<const SSAInstrCall*>(getDefinition(operand))) {
            auto newAsg      = std::make_shared<SSAInstrAssign>();
            newAsg->Target   = call->Target;
            newAsg->Operator = SSAInstrAssign::OpKind::Assign;
            newAsg->Operands = { call2->Arguments.at(0) };

            mDefinitions[newAsg->Target.Name] = newAsg.get();
            instructions[currentIndex]        = std::move(newAsg);
            return true;
        }
    }

    return false;
}

bool SSCPIdentityOptimizer::matchAngleAdditionIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(instructions.at(currentIndex).get());
    if (!asg)
        return false;

    // Match: sin(a)*cos(b) + cos(a)*sin(b) = sin(a+b)
    // Match: sin(a)*cos(b) - cos(a)*sin(b) = sin(a-b)
    // Match: cos(a)*cos(b) + sin(a)*sin(b) = cos(a-b)
    // Match: cos(a)*cos(b) - sin(a)*sin(b) = cos(a+b)

    if (asg->Operator != SSAInstrAssign::OpKind::Binary)
        return false;

    if (asg->BinaryOp != BinaryOperation::Add && asg->BinaryOp != BinaryOperation::Sub)
        return false;

    if (asg->Operands.size() != 2)
        return false;

    // This is complex pattern matching that would require analyzing
    // the structure of both operands to see if they match the pattern
    // For a complete implementation, we would need to:
    // 1. Check if operands are multiplication operations
    // 2. Check if the factors are sin/cos calls
    // 3. Verify the pattern matches one of the identities

    return false;
}

bool SSCPIdentityOptimizer::matchDoubleAngleIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(instructions.at(currentIndex).get());
    if (!asg)
        return false;

    // Match: 2*sin(a)*cos(a) = sin(2*a)
    // Match: 2*cos(a)*cos(a) - 1 = cos(2*a)

    if (asg->Operator != SSAInstrAssign::OpKind::Binary)
        return false;

    // TODO: This would require complex pattern matching similar to angle addition

    return false;
}

bool SSCPIdentityOptimizer::matchPowerReductionIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(instructions.at(currentIndex).get());
    if (!asg)
        return false;

    // Match: (1-cos(2*a))/2 = sin(a)^2
    // Match: (1+cos(2*a))/2 = cos(a)^2

    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->BinaryOp != BinaryOperation::Div)
        return false;

    // TODO: This would require analyzing the structure to match the pattern

    return false;
}

bool SSCPIdentityOptimizer::isCallToIntrinsic(const SSAValue& val, std::string_view funcName) const
{
    if (val.Kind == SSAValue::Kind::Constant)
        return false;

    // Look up the definition
    if (const auto call = dynamic_cast<const SSAInstrCall*>(getDefinition(val)))
        return isIntrinsic(call, funcName);

    return false;
}

bool SSCPIdentityOptimizer::isIntrinsic(const SSAInstrCall* call, std::string_view funcName) const
{
    if (!call)
        return false;

    // FIXME: This will not work when the function name is overwritten in a closure.
    return call->PublicFunctionName == funcName;
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

const SSAInstr* SSCPIdentityOptimizer::findDefinition(const std::string& name) const
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

    const SSAInstr* def = findDefinition(val.Name);
    if (!def)
        return false;

    const auto asg = dynamic_cast<const SSAInstrAssign*>(def);
    if (!asg)
        return false;

    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->BinaryOp != op)
        return false;

    if (asg->Operands.size() != 2)
        return false;

    left  = asg->Operands[0];
    right = asg->Operands[1];
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

const SSAInstr* SSCPIdentityOptimizer::getDefinition(const SSAValue& val) const
{
    if (val.Kind == SSAValue::Kind::Constant)
        return nullptr;

    if (const auto it = mDefinitions.find(val.Name); it != mDefinitions.end())
        return it->second;

    return nullptr;
}
} // namespace PExpr::ssa
