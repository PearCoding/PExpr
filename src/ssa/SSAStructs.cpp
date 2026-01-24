#include "SSAStructs.h"

#include <sstream>

namespace PExpr::ssa {
std::string SSAValue::baseName() const
{
    PEXPR_ASSERT(Kind != SSAValue::Kind::Constant, "Only named and temporary values have a base name");
    PEXPR_ASSERT(!Name.empty(), "The name should never be empty!");

    if (const auto pos = Name.rfind('.'); pos != std::string::npos)
        return Name.substr(0, pos);
    return Name;
}

size_t SSAInstrAssign::hash() const
{
    size_t h = std::hash<int>{}(static_cast<int>(Operator));
    h        = h * 31 + Target.hash();

    switch (Operator) {
    case OpKind::Unary:
        h = h * 31 + std::hash<int>{}(static_cast<int>(UnaryOp));
        if (!Operands.empty())
            h = h * 31 + Operands[0].hash();
        break;
    case OpKind::Binary:
        h = h * 31 + std::hash<int>{}(static_cast<int>(BinaryOp));
        if (Operands.size() >= 2) {
            h = h * 31 + Operands[0].hash();
            h = h * 31 + Operands[1].hash();
        }
        break;
    case OpKind::Swizzle:
        h = h * 31 + std::hash<std::string>{}(Swizzle);
        if (!Operands.empty())
            h = h * 31 + Operands[0].hash();
        break;
    case OpKind::Access:
        if (Operands.size() >= 2) {
            h = h * 31 + Operands[0].hash();
            h = h * 31 + Operands[1].hash();
        }
        break;
    case OpKind::Vector:
        h = h * 31 + std::hash<size_t>{}(Operands.size());
        for (const auto& op : Operands)
            h = h * 31 + op.hash();
        break;
    case OpKind::Cast:
        h = h * 31 + std::hash<int>{}(static_cast<int>(Target.Type));
        if (!Operands.empty())
            h = h * 31 + Operands[0].hash();
        break;
    case OpKind::Assign:
    case OpKind::Nop:
    case OpKind::Phi:
        // These don't contribute to expression hash
        break;
    }
    return h;
}

bool SSAInstrAssign::isEquivalent(const SSAInstr* other) const
{
    if (const auto* otherAsg = dynamic_cast<const SSAInstrAssign*>(other)) {
        if (Operator != otherAsg->Operator)
            return false;

        if (Target.Type != otherAsg->Target.Type)
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
        case OpKind::Swizzle:
            if (Swizzle != otherAsg->Swizzle)
                return false;
            break;
        case OpKind::Cast:
            if (Target.Type != otherAsg->Target.Type)
                return false;
            break;
        default:
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

size_t SSAInstrCall::hash() const
{
    size_t h = std::hash<std::string>{}(FunctionName);
    h        = h * 31 + Target.hash();
    h        = h * 31 + std::hash<size_t>{}(Arguments.size());
    for (const auto& arg : Arguments)
        h = h * 31 + arg.hash();
    return h;
}

bool SSAInstrCall::isEquivalent(const SSAInstr* other) const
{
    if (const auto* otherCall = dynamic_cast<const SSAInstrCall*>(other)) {
        if (FunctionName != otherCall->FunctionName)
            return false;
        if (Arguments.size() != otherCall->Arguments.size())
            return false;
        if (Target.Type != otherCall->Target.Type)
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
    visitor(Target);
    for (auto& cond : Conditions)
        visitor(cond);
    for (auto& branch : Branches)
        visitor(branch);
}

void SSAInstrPhi::forEachOperand(const std::function<void(const SSAValue&)>& visitor) const
{
    visitor(Target);
    for (const auto& cond : Conditions)
        visitor(cond);
    for (const auto& branch : Branches)
        visitor(branch);
}

void SSAInstrPhi::forEachTarget(const std::function<void(SSAValue&)>& visitor) { visitor(Target); }

void SSAInstrPhi::forEachTarget(const std::function<void(const SSAValue&)>& visitor) const { visitor(Target); }

// SSAValue hash implementation
size_t SSAValue::hash() const
{
    size_t h = std::hash<int>{}(static_cast<int>(Kind));
    h        = h * 31 + std::hash<std::string>{}(Name);
    h        = h * 31 + std::hash<int>{}(static_cast<int>(Type));

    if (Kind == Kind::Constant) {
        // Hash the constant value based on type
        switch (Type) {
        case ElementaryType::Boolean:
            if (const bool* b = std::get_if<bool>(&Value))
                h = h * 31 + std::hash<bool>{}(*b);
            break;
        case ElementaryType::Integer:
            if (const Integer* i = std::get_if<Integer>(&Value))
                h = h * 31 + std::hash<Integer>{}(*i);
            break;
        case ElementaryType::Number:
            if (const Number* n = std::get_if<Number>(&Value))
                h = h * 31 + std::hash<Number>{}(*n);
            break;
        case ElementaryType::String:
            if (const std::string* s = std::get_if<std::string>(&Value))
                h = h * 31 + std::hash<std::string>{}(*s);
            break;
        default:
            if (Type >= ElementaryType::Vec1) {
                if (const VecN* v = std::get_if<VecN>(&Value)) {
                    for (Number n : *v)
                        h = h * 31 + std::hash<Number>{}(n);
                }
            }
            break;
        }
    }
    return h;
}

bool SSAValue::operator==(const SSAValue& other) const
{
    if (Kind != other.Kind || Type != other.Type)
        return false;

    if (Kind == Kind::Constant) {
        // Compare constant values
        if (Type != other.Type)
            return false;

        switch (Type) {
        case ElementaryType::Boolean:
            return std::get<bool>(Value) == std::get<bool>(other.Value);
        case ElementaryType::Integer:
            return std::get<Integer>(Value) == std::get<Integer>(other.Value);
        case ElementaryType::Number:
            return std::get<Number>(Value) == std::get<Number>(other.Value);
        case ElementaryType::String:
            return std::get<std::string>(Value) == std::get<std::string>(other.Value);
        default:
            if (Type >= ElementaryType::Vec1) {
                return std::get<VecN>(Value) == std::get<VecN>(other.Value);
            }
            return false;
        }
    } else {
        // For Named/Temp values, compare names
        return Name == other.Name;
    }
}

} // namespace PExpr::ssa