#include "SSCPIdentityOptimizer.h"
#include "ssa/SSAContext.h"

#include <cmath>
#include <ranges>
#include <sstream>

namespace PExpr::opt {
using namespace ast;
using namespace ssa;

bool SSCPIdentityOptimizer::applyIdentities(SSAContext* ctx, InstructionList& instructions)
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
        bool blockChanged = applyIdentitiesToRange(ctx, blockBegin, blockEnd);

        if (blockChanged)
            changed = true;
    }

    return changed;
}

bool SSCPIdentityOptimizer::applyIdentitiesToRange(ssa::SSAContext* ctx, InstructionList::iterator begin, InstructionList::iterator end)
{
    // Build definition map
    mDefinitions.clear();
    std::ranges::for_each(begin, end, [this](const auto& instrPtr) {
        if (const auto asg = dynamic_cast<const SSAInstrAssign*>(instrPtr.get()))
            mDefinitions[asg->Target.name()] = asg;
        else if (const auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get()))
            mDefinitions[call->Target.name()] = call;
        else if (const auto phi = dynamic_cast<const SSAInstrPhi*>(instrPtr.get()))
            mDefinitions[phi->Target.name()] = phi;
    });

    bool changed = false;
    std::ranges::for_each(begin, end, [&](auto& instrPtr) {
        if (tryApplyIdentity(ctx, instrPtr))
            changed = true;
    });
    return changed;
}

bool SSCPIdentityOptimizer::tryApplyIdentity(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    if (mOptions.ApplyMathIdentities) {
        if (matchBasicMathIdentities(ctx, currentInstruction))
            return true;

        if (matchLogicalIdentities(ctx, currentInstruction))
            return true;

        if (matchSelfOperandIdentities(ctx, currentInstruction))
            return true;

        if (matchRepeatedAdditionIdentity(ctx, currentInstruction))
            return true;

        if (matchUnaryIdentity(ctx, currentInstruction))
            return true;

        if (matchPowerToSquareIdentity(ctx, currentInstruction))
            return true;
    }

    if (mOptions.ApplyTrigonometricIdentities) {
        if (matchPythagoreanIdentity(ctx, currentInstruction))
            return true;

        if (matchInverseTrigonometricIdentity(ctx, currentInstruction))
            return true;

        if (matchAngleAdditionIdentity(ctx, currentInstruction))
            return true;

        if (matchDoubleAngleIdentity(ctx, currentInstruction))
            return true;

        if (matchPowerReductionIdentity(ctx, currentInstruction))
            return true;
    }

    return false;
}

bool SSCPIdentityOptimizer::matchBasicMathIdentities(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(currentInstruction.get());
    if (!asg)
        return false;

    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->Operands.size() != 2)
        return false;

    SSAValue left  = asg->Operands[0];
    SSAValue right = asg->Operands[1];

    // Check if one operand is a constant number
    Number constVal;
    bool leftIsConst  = isConstantNumber(left, constVal);
    bool rightIsConst = isConstantNumber(right, constVal);

    // If both are constant it is the job of constant folding,
    // if both are non-constant we can't do much
    if (leftIsConst == rightIsConst)
        return false;

    // For identities, we need to know which side is constant
    if (leftIsConst) {
        // Constant is on left side
        switch (asg->BinaryOp) {
        case BinaryOperation::Add:
            // 0 + a = a
            if (constVal == Number(0.0)) {
                auto newAsg      = std::make_shared<SSAInstrAssign>();
                newAsg->Target   = asg->Target;
                newAsg->Operator = SSAInstrAssign::OpKind::Assign;
                newAsg->Operands = { right };

                mDefinitions[newAsg->Target.name()] = newAsg.get();
                currentInstruction                  = std::move(newAsg);
                return true;
            }
            break;

        case BinaryOperation::Mul:
            // 0 * a = 0
            if (constVal == Number(0.0)) {
                auto newAsg      = std::make_shared<SSAInstrAssign>();
                newAsg->Target   = asg->Target;
                newAsg->Operator = SSAInstrAssign::OpKind::Assign;
                newAsg->Operands = { left };

                mDefinitions[newAsg->Target.name()] = newAsg.get();
                currentInstruction                  = std::move(newAsg);
                return true;
            }
            // 1 * a = a
            else if (constVal == Number(1.0)) {
                auto newAsg      = std::make_shared<SSAInstrAssign>();
                newAsg->Target   = asg->Target;
                newAsg->Operator = SSAInstrAssign::OpKind::Assign;
                newAsg->Operands = { right };

                mDefinitions[newAsg->Target.name()] = newAsg.get();
                currentInstruction                  = std::move(newAsg);
                return true;
            }
            break;

        default:
            break;
        }
    }

    if (rightIsConst) {
        // Constant is on right side
        switch (asg->BinaryOp) {
        case BinaryOperation::Add:
            // a + 0 = a
            if (constVal == Number(0.0)) {
                auto newAsg      = std::make_shared<SSAInstrAssign>();
                newAsg->Target   = asg->Target;
                newAsg->Operator = SSAInstrAssign::OpKind::Assign;
                newAsg->Operands = { left };

                mDefinitions[newAsg->Target.name()] = newAsg.get();
                currentInstruction                  = std::move(newAsg);
                return true;
            }
            break;

        case BinaryOperation::Sub:
            // a - 0 = a
            if (constVal == Number(0.0)) {
                auto newAsg      = std::make_shared<SSAInstrAssign>();
                newAsg->Target   = asg->Target;
                newAsg->Operator = SSAInstrAssign::OpKind::Assign;
                newAsg->Operands = { left };

                mDefinitions[newAsg->Target.name()] = newAsg.get();
                currentInstruction                  = std::move(newAsg);
                return true;
            }
            break;

        case BinaryOperation::Mul:
            // a * 0 = 0
            if (constVal == Number(0.0)) {
                auto newAsg      = std::make_shared<SSAInstrAssign>();
                newAsg->Target   = asg->Target;
                newAsg->Operator = SSAInstrAssign::OpKind::Assign;
                newAsg->Operands = { right };

                mDefinitions[newAsg->Target.name()] = newAsg.get();
                currentInstruction                  = std::move(newAsg);
                return true;
            }
            // a * 1 = a
            else if (constVal == Number(1.0)) {
                auto newAsg      = std::make_shared<SSAInstrAssign>();
                newAsg->Target   = asg->Target;
                newAsg->Operator = SSAInstrAssign::OpKind::Assign;
                newAsg->Operands = { left };

                mDefinitions[newAsg->Target.name()] = newAsg.get();
                currentInstruction                  = std::move(newAsg);
                return true;
            }
            break;

        case BinaryOperation::Div:
            // a / 1 = a
            if (constVal == Number(1.0)) {
                auto newAsg      = std::make_shared<SSAInstrAssign>();
                newAsg->Target   = asg->Target;
                newAsg->Operator = SSAInstrAssign::OpKind::Assign;
                newAsg->Operands = { left };

                mDefinitions[newAsg->Target.name()] = newAsg.get();
                currentInstruction                  = std::move(newAsg);
                return true;
            }
            break;

        default:
            break;
        }
    }

    return false;
}

bool SSCPIdentityOptimizer::matchLogicalIdentities(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(currentInstruction.get());
    if (!asg)
        return false;

    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->Operands.size() != 2)
        return false;

    if (asg->BinaryOp != BinaryOperation::And && asg->BinaryOp != BinaryOperation::Or)
        return false;

    SSAValue left  = asg->Operands[0];
    SSAValue right = asg->Operands[1];

    bool constVal;
    bool leftIsConst  = isConstantBool(left, constVal);
    bool rightIsConst = isConstantBool(right, constVal);

    // Both constant -> constant folding handles it; both non-constant -> nothing to do here
    if (leftIsConst == rightIsConst)
        return false;

    const SSAValue& constSide    = leftIsConst ? left : right;
    const SSAValue& nonConstSide = leftIsConst ? right : left;

    bool boolVal = false;
    (void)isConstantBool(constSide, boolVal);

    if (asg->BinaryOp == BinaryOperation::Or) {
        if (boolVal) {
            // a || true = true, true || a = true
            auto newAsg      = std::make_shared<SSAInstrAssign>();
            newAsg->Target   = asg->Target;
            newAsg->Operator = SSAInstrAssign::OpKind::Assign;
            newAsg->Operands = { SSAValue::Constant(true) };

            mDefinitions[newAsg->Target.name()] = newAsg.get();
            currentInstruction                  = std::move(newAsg);
            return true;
        } else {
            // a || false = a, false || a = a
            auto newAsg      = std::make_shared<SSAInstrAssign>();
            newAsg->Target   = asg->Target;
            newAsg->Operator = SSAInstrAssign::OpKind::Assign;
            newAsg->Operands = { nonConstSide };

            mDefinitions[newAsg->Target.name()] = newAsg.get();
            currentInstruction                  = std::move(newAsg);
            return true;
        }
    }

    if (asg->BinaryOp == BinaryOperation::And) {
        if (!boolVal) {
            // a && false = false, false && a = false
            auto newAsg      = std::make_shared<SSAInstrAssign>();
            newAsg->Target   = asg->Target;
            newAsg->Operator = SSAInstrAssign::OpKind::Assign;
            newAsg->Operands = { SSAValue::Constant(false) };

            mDefinitions[newAsg->Target.name()] = newAsg.get();
            currentInstruction                  = std::move(newAsg);
            return true;
        } else {
            // a && true = a, true && a = a
            auto newAsg      = std::make_shared<SSAInstrAssign>();
            newAsg->Target   = asg->Target;
            newAsg->Operator = SSAInstrAssign::OpKind::Assign;
            newAsg->Operands = { nonConstSide };

            mDefinitions[newAsg->Target.name()] = newAsg.get();
            currentInstruction                  = std::move(newAsg);
            return true;
        }
    }

    return false;
}

// WARNING: Some of these identities (a - a = 0, a / a = 1, a == a = true, etc.) are not valid
// for IEEE-754 doubles when the value is NaN (NaN != NaN, NaN == NaN is false, etc.).
// For num types, these are only applied when ApplyUnsafeMathIdentities is set (-O3 / fast-math).
// For int and bool types, they are always safe and applied under ApplyMathIdentities.
bool SSCPIdentityOptimizer::matchSelfOperandIdentities(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(currentInstruction.get());
    if (!asg)
        return false;

    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->Operands.size() != 2)
        return false;

    const SSAValue& left  = asg->Operands[0];
    const SSAValue& right = asg->Operands[1];

    // Both must be non-constant and refer to the same variable
    if (left.isConstant() || right.isConstant() || left.name() != right.name())
        return false;

    const bool isNum = (left.type().kind() == type::TypeKind::Number);
    const bool isInt = (left.type().kind() == type::TypeKind::Integer);

    // For num types, these identities are only valid under fast-math (NaN breaks them)
    if (isNum && !mOptions.ApplyUnsafeMathIdentities)
        return false;

    switch (asg->BinaryOp) {
    case BinaryOperation::Sub:
    case BinaryOperation::Mod: {
        // a - a = 0, a % a = 0
        auto newAsg      = std::make_shared<SSAInstrAssign>();
        newAsg->Target   = asg->Target;
        newAsg->Operator = SSAInstrAssign::OpKind::Assign;
        newAsg->Operands = { isInt ? SSAValue::Constant(Integer(0)) : SSAValue::Constant(Number(0.0)) };

        mDefinitions[newAsg->Target.name()] = newAsg.get();
        currentInstruction                  = std::move(newAsg);
        return true;
    }

    case BinaryOperation::Div: {
        // a / a = 1
        auto newAsg      = std::make_shared<SSAInstrAssign>();
        newAsg->Target   = asg->Target;
        newAsg->Operator = SSAInstrAssign::OpKind::Assign;
        newAsg->Operands = { isInt ? SSAValue::Constant(Integer(1)) : SSAValue::Constant(Number(1.0)) };

        mDefinitions[newAsg->Target.name()] = newAsg.get();
        currentInstruction                  = std::move(newAsg);
        return true;
    }

    case BinaryOperation::And:
    case BinaryOperation::Or: {
        // a && a = a, a || a = a (idempotent)
        auto newAsg      = std::make_shared<SSAInstrAssign>();
        newAsg->Target   = asg->Target;
        newAsg->Operator = SSAInstrAssign::OpKind::Assign;
        newAsg->Operands = { left };

        mDefinitions[newAsg->Target.name()] = newAsg.get();
        currentInstruction                  = std::move(newAsg);
        return true;
    }

    case BinaryOperation::Equal:
    case BinaryOperation::LessEqual:
    case BinaryOperation::GreaterEqual: {
        // a == a = true, a <= a = true, a >= a = true
        auto newAsg      = std::make_shared<SSAInstrAssign>();
        newAsg->Target   = asg->Target;
        newAsg->Operator = SSAInstrAssign::OpKind::Assign;
        newAsg->Operands = { SSAValue::Constant(true) };

        mDefinitions[newAsg->Target.name()] = newAsg.get();
        currentInstruction                  = std::move(newAsg);
        return true;
    }

    case BinaryOperation::NotEqual:
    case BinaryOperation::Less:
    case BinaryOperation::Greater: {
        // a != a = false, a < a = false, a > a = false
        auto newAsg      = std::make_shared<SSAInstrAssign>();
        newAsg->Target   = asg->Target;
        newAsg->Operator = SSAInstrAssign::OpKind::Assign;
        newAsg->Operands = { SSAValue::Constant(false) };

        mDefinitions[newAsg->Target.name()] = newAsg.get();
        currentInstruction                  = std::move(newAsg);
        return true;
    }

    default:
        break;
    }

    return false;
}

bool SSCPIdentityOptimizer::matchRepeatedAdditionIdentity(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(currentInstruction.get());
    if (!asg)
        return false;

    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->Operands.size() != 2)
        return false;

    if (asg->BinaryOp != BinaryOperation::Add)
        return false;

    SSAValue left  = asg->Operands[0];
    SSAValue right = asg->Operands[1];

    // Map a + a + a = 3*a
    // Note: a + a is not mapped to 2*n as this might introduce a more costly instruction
    SSAValue addLeft, addRight;
    if (isBinaryOp(left, BinaryOperation::Add, addLeft, addRight)) {
        if (!right.isConstant() && !addLeft.isConstant() && !addRight.isConstant()
            && addLeft.name() == right.name() && addRight.name() == right.name()) {
            auto newAsg      = std::make_shared<SSAInstrAssign>();
            newAsg->Target   = asg->Target;
            newAsg->Operator = SSAInstrAssign::OpKind::Binary;
            newAsg->BinaryOp = BinaryOperation::Mul;
            newAsg->Operands = { right, SSAValue::Constant(Number(3.0)) };

            mDefinitions[newAsg->Target.name()] = newAsg.get();
            currentInstruction                  = std::move(newAsg);
            return true;
        }
    }
    if (isBinaryOp(right, BinaryOperation::Add, addLeft, addRight)) {
        if (!left.isConstant() && !addLeft.isConstant() && !addRight.isConstant()
            && addLeft.name() == left.name() && addRight.name() == left.name()) {
            auto newAsg      = std::make_shared<SSAInstrAssign>();
            newAsg->Target   = asg->Target;
            newAsg->Operator = SSAInstrAssign::OpKind::Binary;
            newAsg->BinaryOp = BinaryOperation::Mul;
            newAsg->Operands = { left, SSAValue::Constant(Number(3.0)) };

            mDefinitions[newAsg->Target.name()] = newAsg.get();
            currentInstruction                  = std::move(newAsg);
            return true;
        }
    }

    // Check for n*a + a pattern where n is constant
    SSAValue mulLeft, mulRight;
    Number constNum;

    // Check if left operand is a multiplication with a constant
    if (isBinaryOp(left, BinaryOperation::Mul, mulLeft, mulRight)) {
        if (isConstantNumber(mulLeft, constNum) && mulRight.name() == right.name()) {
            // n*a + a = (n+1)*a
            Number newConst  = constNum + Number(1.0);
            auto newAsg      = std::make_shared<SSAInstrAssign>();
            newAsg->Target   = asg->Target;
            newAsg->Operator = SSAInstrAssign::OpKind::Binary;
            newAsg->BinaryOp = BinaryOperation::Mul;
            newAsg->Operands = { right, SSAValue::Constant(newConst) };

            mDefinitions[newAsg->Target.name()] = newAsg.get();
            currentInstruction                  = std::move(newAsg);
            return true;
        }
        if (isConstantNumber(mulRight, constNum) && mulLeft.name() == right.name()) {
            // a*n + a = (n+1)*a
            Number newConst  = constNum + Number(1.0);
            auto newAsg      = std::make_shared<SSAInstrAssign>();
            newAsg->Target   = asg->Target;
            newAsg->Operator = SSAInstrAssign::OpKind::Binary;
            newAsg->BinaryOp = BinaryOperation::Mul;
            newAsg->Operands = { right, SSAValue::Constant(newConst) };

            mDefinitions[newAsg->Target.name()] = newAsg.get();
            currentInstruction                  = std::move(newAsg);
            return true;
        }
    }

    // Check if right operand is a multiplication with a constant
    if (isBinaryOp(right, BinaryOperation::Mul, mulLeft, mulRight)) {
        if (isConstantNumber(mulLeft, constNum) && mulRight.name() == left.name()) {
            // a + n*a = (n+1)*a
            Number newConst  = constNum + Number(1.0);
            auto newAsg      = std::make_shared<SSAInstrAssign>();
            newAsg->Target   = asg->Target;
            newAsg->Operator = SSAInstrAssign::OpKind::Binary;
            newAsg->BinaryOp = BinaryOperation::Mul;
            newAsg->Operands = { left, SSAValue::Constant(newConst) };

            mDefinitions[newAsg->Target.name()] = newAsg.get();
            currentInstruction                  = std::move(newAsg);
            return true;
        }
        if (isConstantNumber(mulRight, constNum) && mulLeft.name() == left.name()) {
            // a + a*n = (n+1)*a
            Number newConst  = constNum + Number(1.0);
            auto newAsg      = std::make_shared<SSAInstrAssign>();
            newAsg->Target   = asg->Target;
            newAsg->Operator = SSAInstrAssign::OpKind::Binary;
            newAsg->BinaryOp = BinaryOperation::Mul;
            newAsg->Operands = { left, SSAValue::Constant(newConst) };

            mDefinitions[newAsg->Target.name()] = newAsg.get();
            currentInstruction                  = std::move(newAsg);
            return true;
        }
    }

    return false;
}

bool SSCPIdentityOptimizer::matchUnaryIdentity(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(currentInstruction.get());
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

        mDefinitions[newAsg->Target.name()] = newAsg.get();
        currentInstruction                  = std::move(newAsg);
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

        mDefinitions[newAsg->Target.name()] = newAsg.get();
        currentInstruction                  = std::move(newAsg);
    }
    return true;
}

bool SSCPIdentityOptimizer::matchPythagoreanIdentity(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(currentInstruction.get());
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

bool SSCPIdentityOptimizer::matchPowerToSquareIdentity(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(currentInstruction.get());
    if (!asg)
        return false;

    // Match: a^2 = a*a
    if (asg->Operator != SSAInstrAssign::OpKind::Binary || asg->BinaryOp != BinaryOperation::Pow)
        return false;

    if (asg->Operands.size() != 2)
        return false;

    // Check if this is pow(x, 2)
    if (Number powExponent; isConstantNumber(asg->Operands[1], powExponent) && powExponent == Number(2.0)) {
        auto newAsg      = std::make_shared<SSAInstrAssign>();
        newAsg->Target   = asg->Target;
        newAsg->Operator = SSAInstrAssign::OpKind::Binary;
        newAsg->BinaryOp = BinaryOperation::Mul;
        newAsg->Operands = { asg->Operands[0], asg->Operands[0] };

        mDefinitions[newAsg->Target.name()] = newAsg.get();
        currentInstruction                  = std::move(newAsg);
        return true;
    }

    return false;
}

bool SSCPIdentityOptimizer::matchInverseTrigonometricIdentity(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    PEXPR_UNUSED(ctx);

    const auto call = dynamic_cast<const SSAInstrCall*>(currentInstruction.get());
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

            mDefinitions[newAsg->Target.name()] = newAsg.get();
            currentInstruction                  = std::move(newAsg);
            return true;
        }
    }

    return false;
}

bool SSCPIdentityOptimizer::matchAngleAdditionIdentity(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(currentInstruction.get());
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

bool SSCPIdentityOptimizer::matchDoubleAngleIdentity(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(currentInstruction.get());
    if (!asg)
        return false;

    // Match: 2*sin(a)*cos(a) = sin(2*a)
    // Match: 2*cos(a)*cos(a) - 1 = cos(2*a)

    if (asg->Operator != SSAInstrAssign::OpKind::Binary)
        return false;

    // TODO: This would require complex pattern matching similar to angle addition

    return false;
}

bool SSCPIdentityOptimizer::matchPowerReductionIdentity(SSAContext* ctx, std::shared_ptr<ssa::SSAInstr>& currentInstruction)
{
    PEXPR_UNUSED(ctx);

    const auto asg = dynamic_cast<const SSAInstrAssign*>(currentInstruction.get());
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
    // The function inliner takes care of constant calls
    if (val.isConstant())
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
    return a == b;
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
    // Constant folding takes care
    if (val.isConstant())
        return false;

    const SSAInstr* def = findDefinition(val.name());
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

bool SSCPIdentityOptimizer::isConstantBool(const SSAValue& val, bool& outValue) const
{
    if (!val.isConstant())
        return false;

    if (const bool* b = val.valueAsIf<bool>()) {
        outValue = *b;
        return true;
    }

    return false;
}

bool SSCPIdentityOptimizer::isConstantNumber(const SSAValue& val, Number& outValue) const
{
    if (!val.isConstant())
        return false;

    if (const Number* n = val.valueAsIf<Number>()) {
        outValue = *n;
        return true;
    }

    if (const Integer* i = val.valueAsIf<Integer>()) {
        outValue = static_cast<Number>(*i);
        return true;
    }

    return false;
}

const SSAInstr* SSCPIdentityOptimizer::getDefinition(const SSAValue& val) const
{
    if (val.isConstant())
        return nullptr;

    if (const auto it = mDefinitions.find(val.name()); it != mDefinitions.end())
        return it->second;

    return nullptr;
}
} // namespace PExpr::opt
