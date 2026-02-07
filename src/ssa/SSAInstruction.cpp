#include "SSAInstruction.h"

#include <sstream>

namespace PExpr::ssa {

size_t SSAInstrAssign::hash(bool includeTargetName) const
{
    size_t h = std::hash<int>{}(static_cast<int>(Operator));
    h        = h * 31 + Target.hash(includeTargetName);

    switch (Operator) {
    case OpKind::Unary:
        h = h * 31 + std::hash<int>{}(static_cast<int>(UnaryOp));
        if (!Operands.empty())
            h = h * 31 + Operands[0].hash(true);
        break;
    case OpKind::Binary:
        h = h * 31 + std::hash<int>{}(static_cast<int>(BinaryOp));
        if (Operands.size() >= 2) {
            h = h * 31 + Operands[0].hash(true);
            h = h * 31 + Operands[1].hash(true);
        }
        break;
    case OpKind::Access:
        if (Operands.size() >= 2) {
            h = h * 31 + Operands[0].hash(true);
            h = h * 31 + Operands[1].hash(true);
        }
        break;
    case OpKind::Cast:
        h = h * 31 + Target.type().hash();
        if (!Operands.empty())
            h = h * 31 + Operands[0].hash(true);
        break;
    case OpKind::Assign:
        h = h * 31 + std::hash<size_t>{}(Operands.size());
        for (const auto& op : Operands)
            h = h * 31 + op.hash(true);
        break;
    }
    return h;
}

bool SSAInstrAssign::isEquivalent(const SSAInstr* other) const
{
    if (const auto* otherAsg = dynamic_cast<const SSAInstrAssign*>(other)) {
        if (Operator != otherAsg->Operator)
            return false;

        if (Target.type() != otherAsg->Target.type())
            return false;

        if (Operands.size() != otherAsg->Operands.size())
            return false;

        switch (Operator) {
        case OpKind::Unary:
            if (UnaryOp != otherAsg->UnaryOp)
                return false;
            break;
        case OpKind::Binary:
            if (BinaryOp != otherAsg->BinaryOp)
                return false;
            break;
        case OpKind::Cast:
            if (Target.type() != otherAsg->Target.type())
                return false;
            break;
        default:
            PEXPR_ASSERT(false, "Non exhaustive Operator check in isEquivalent");
            break;
        }

        for (size_t i = 0; i < Operands.size(); ++i) {
            if (Operands[i] != otherAsg->Operands[i])
                return false;
        }

        return true;
    }
    return false;
}

size_t SSAInstrCall::hash(bool includeTargetName) const
{
    size_t h = std::hash<std::string>{}(FunctionName);
    h        = h * 31 + std::hash<std::string>{}(PublicFunctionName);
    h        = h * 31 + Target.hash(includeTargetName);
    h        = h * 31 + std::hash<size_t>{}(Arguments.size());
    for (const auto& arg : Arguments)
        h = h * 31 + arg.hash(true);
    return h;
}

bool SSAInstrCall::isEquivalent(const SSAInstr* other) const
{
    if (const auto* otherCall = dynamic_cast<const SSAInstrCall*>(other)) {
        if (FunctionName != otherCall->FunctionName)
            return false;
        if (PublicFunctionName != otherCall->PublicFunctionName)
            return false;
        if (Arguments.size() != otherCall->Arguments.size())
            return false;
        if (Target.type() != otherCall->Target.type())
            return false;
        for (size_t i = 0; i < Arguments.size(); ++i) {
            if (Arguments[i] != otherCall->Arguments[i])
                return false;
        }
        return true;
    }
    return false;
}

void SSAInstrAssign::forEachOperand(const std::function<void(SSAValue&)>& visitor)
{
    for (auto& op : Operands)
        visitor(op);
}

void SSAInstrAssign::forEachOperand(const std::function<void(const SSAValue&)>& visitor) const
{
    for (const auto& op : Operands)
        visitor(op);
}

void SSAInstrAssign::forEachTarget(const std::function<void(SSAValue&)>& visitor) { visitor(Target); }

void SSAInstrAssign::forEachTarget(const std::function<void(const SSAValue&)>& visitor) const { visitor(Target); }

void SSAInstrCall::forEachOperand(const std::function<void(SSAValue&)>& visitor)
{
    for (auto& arg : Arguments)
        visitor(arg);
}

void SSAInstrCall::forEachOperand(const std::function<void(const SSAValue&)>& visitor) const
{
    for (const auto& arg : Arguments)
        visitor(arg);
}

void SSAInstrCall::forEachTarget(const std::function<void(SSAValue&)>& visitor) { visitor(Target); }

void SSAInstrCall::forEachTarget(const std::function<void(const SSAValue&)>& visitor) const { visitor(Target); }

void SSAInstrReturn::forEachOperand(const std::function<void(SSAValue&)>& visitor) { visitor(Value); }

void SSAInstrReturn::forEachOperand(const std::function<void(const SSAValue&)>& visitor) const { visitor(Value); }

void SSAInstrBranch::forEachOperand(const std::function<void(SSAValue&)>& visitor) { visitor(Condition); }

void SSAInstrBranch::forEachOperand(const std::function<void(const SSAValue&)>& visitor) const { visitor(Condition); }

void SSAInstrPhi::forEachOperand(const std::function<void(SSAValue&)>& visitor)
{
    for (auto& cond : Conditions)
        visitor(cond);
    for (auto& branch : Branches)
        visitor(branch);
}

void SSAInstrPhi::forEachOperand(const std::function<void(const SSAValue&)>& visitor) const
{
    for (const auto& cond : Conditions)
        visitor(cond);
    for (const auto& branch : Branches)
        visitor(branch);
}

void SSAInstrPhi::forEachTarget(const std::function<void(SSAValue&)>& visitor) { visitor(Target); }

void SSAInstrPhi::forEachTarget(const std::function<void(const SSAValue&)>& visitor) const { visitor(Target); }

} // namespace PExpr::ssa