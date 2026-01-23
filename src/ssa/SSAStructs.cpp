#include "SSAStructs.h"

#include <sstream>

namespace PExpr::ssa {
// Strings

static inline std::string_view toInstructionString(UnaryOperation op)
{
    switch (op) {
    case UnaryOperation::Pos:
        return "pos";
    case UnaryOperation::Neg:
        return "neg";
    case UnaryOperation::Not:
        return "not";
    default:
        PEXPR_ASSERT(false, "Invalid unary operation enum");
        return "";
    }
}

static inline std::string_view toInstructionString(BinaryOperation op)
{
    switch (op) {
    case BinaryOperation::Add:
        return "add";
    case BinaryOperation::Sub:
        return "sub";
    case BinaryOperation::Mul:
        return "mul";
    case BinaryOperation::Div:
        return "div";
    case BinaryOperation::Pow:
        return "pow";
    case BinaryOperation::Mod:
        return "mod";
    case BinaryOperation::And:
        return "and";
    case BinaryOperation::Or:
        return "or";
    case BinaryOperation::Less:
        return "ls";
    case BinaryOperation::Greater:
        return "gt";
    case BinaryOperation::LessEqual:
        return "le";
    case BinaryOperation::GreaterEqual:
        return "ge";
    case BinaryOperation::Equal:
        return "eq";
    case BinaryOperation::NotEqual:
        return "neq";
    default:
        PEXPR_ASSERT(false, "Invalid binary operation enum");
        return "";
    }
}

// SSAValue / Instr dumps

std::string SSAValue::toString(bool showType) const
{
    std::string prefix;
    if (this->Kind == Kind::Constant) {
        switch (this->Type) {
        case ElementaryType::Boolean:
            prefix = std::get<bool>(Value) ? "true" : "false";
            break;
        case ElementaryType::Integer:
            prefix = std::to_string(std::get<Integer>(Value));
            break;
        case ElementaryType::Number:
            prefix = std::to_string(std::get<Number>(Value));
            break;
        case ElementaryType::String:
            prefix = "\"" + std::get<std::string>(Value) + "\"";
            break;
        default:
            if (this->Type >= ElementaryType::Vec1) {
                const auto v = std::get<VecN>(Value);
                PEXPR_ASSERT(v.size() == typeArraySize(this->Type), "Vector data and vector type missmatch");

                if (v.empty()) {
                    prefix = "[]";
                } else {
                    prefix = "[" + std::to_string(v[0]);
                    for (size_t i = 1; i < v.size(); ++i)
                        prefix += "," + std::to_string(v[i]);
                    prefix += "]";
                }
            } else {
                PEXPR_ASSERT(false, "Expected specified type for SSAValue constants");
            }
        }
    } else {
        prefix = Name;
    }

    if (prefix.empty())
        return std::string("_");

    if ((showType || this->Kind == Kind::Constant) && Type != PExpr::ElementaryType::Unspecified) {
        std::stringstream ss;
        ss << prefix << ":" << std::string(PExpr::toString(Type));
        return ss.str();
    }
    return prefix;
}

std::string SSAValue::baseName() const
{
    PEXPR_ASSERT(Kind == SSAValue::Kind::Named, "Only named values have a base name");
    if (Name.empty())
        return std::string();
    auto pos = Name.find('.');
    if (pos == std::string::npos)
        return Name;
    return Name.substr(0, pos);
}

std::string SSAInstrAssign::dump() const
{
    std::stringstream ss;
    ss << Target.toString(true) << " = ";
    switch (Operator) {
    case OpKind::Assign:
        ss << "assign";
        break;
    case OpKind::Unary:
        ss << toInstructionString(UnaryOp);
        break;
    case OpKind::Binary:
        ss << toInstructionString(BinaryOp);
        break;
    case OpKind::Swizzle:
        ss << "swizzle[" << Swizzle << "]";
        break;
    case OpKind::Access:
        ss << "access";
        break;
    case OpKind::Vector:
        ss << "vec[" << Operands.size() << "]";
        break;
    case OpKind::Nop:
        ss << "nop";
        break;
    case OpKind::Cast:
        ss << "cast";
        break;
    default:
        ss << "unknown";
        break;
    }
    ss << "(";
    for (size_t i = 0; i < Operands.size(); ++i) {
        if (i)
            ss << ", ";
        ss << Operands[i].toString(false);
    }
    ss << ")";
    return ss.str();
}

std::string SSAInstrCall::dump() const
{
    std::stringstream ss;
    ss << Target.toString(true) << " = call[" << FunctionName << "](";
    for (size_t i = 0; i < Arguments.size(); ++i) {
        if (i)
            ss << ", ";
        ss << Arguments[i].toString(false);
    }
    ss << ")";
    return ss.str();
}

std::string SSAInstrReturn::dump() const
{
    std::stringstream ss;
    ss << "return " << Value.toString(false);
    return ss.str();
}

std::string SSAInstrLabel::dump() const
{
    return std::string(Name + ":");
}

std::string SSAInstrBranch::dump() const
{
    std::stringstream ss;
    ss << "br " << Condition.toString(false) << " -> " << TargetLabel;
    return ss.str();
}

std::string SSAInstrGoto::dump() const
{
    return std::string("goto " + TargetLabel);
}

std::string SSAInstrPhi::dump() const
{
    std::stringstream ss;
    ss << Target.toString(true) << " = phi[";
    for (size_t i = 0; i < Conditions.size(); ++i) {
        if (i)
            ss << ", ";
        ss << Conditions[i].toString(false);
    }
    ss << "](";
    for (size_t i = 0; i < Branches.size(); ++i) {
        if (i)
            ss << ", ";
        ss << Branches[i].toString(false);
    }
    ss << ")";
    return ss.str();
}

std::string SSAFunction::dump() const
{
    std::stringstream ss;
    if (External)
        ss << "extern ";
    if (External && !HasSideEffect)
        ss << "pure ";
    ss << "fn " << Name << "(";
    for (size_t i = 0; i < Parameters.size(); ++i) {
        if (i)
            ss << ", ";
        ss << Parameters[i];
    }
    ss << ")";
    if (ReturnType != PExpr::ElementaryType::Unspecified)
        ss << ":" << PExpr::toString(ReturnType);
    ss << std::endl;

    if (!Body.empty()) {
        for (const auto& instr : Body)
            ss << "  " << instr->dump() << std::endl;
        ss << "endfn" << std::endl;
    }
    return ss.str();
}

std::string SSAProgram::dump() const
{
    std::stringstream ss;
    for (const auto& f : Functions)
        ss << f.dump() << std::endl;
    for (const auto& instr : Body)
        ss << instr->dump() << std::endl;
    return ss.str();
}

// forEachValue implementations

void SSAInstrAssign::forEachValue(const std::function<void(SSAValue&)>& visitor)
{
    visitor(Target);
    for (auto& op : Operands)
        visitor(op);
}

void SSAInstrAssign::forEachValue(const std::function<void(const SSAValue&)>& visitor) const
{
    visitor(Target);
    for (const auto& op : Operands)
        visitor(op);
}

void SSAInstrCall::forEachValue(const std::function<void(SSAValue&)>& visitor)
{
    visitor(Target);
    for (auto& arg : Arguments)
        visitor(arg);
}

void SSAInstrCall::forEachValue(const std::function<void(const SSAValue&)>& visitor) const
{
    visitor(Target);
    for (const auto& arg : Arguments)
        visitor(arg);
}

void SSAInstrReturn::forEachValue(const std::function<void(SSAValue&)>& visitor)
{
    visitor(Value);
}

void SSAInstrReturn::forEachValue(const std::function<void(const SSAValue&)>& visitor) const
{
    visitor(Value);
}

void SSAInstrLabel::forEachValue(const std::function<void(SSAValue&)>& visitor)
{
    // Labels don't contain any SSAValues
    (void)visitor;
}

void SSAInstrLabel::forEachValue(const std::function<void(const SSAValue&)>& visitor) const
{
    // Labels don't contain any SSAValues
    (void)visitor;
}

void SSAInstrBranch::forEachValue(const std::function<void(SSAValue&)>& visitor)
{
    visitor(Condition);
}

void SSAInstrBranch::forEachValue(const std::function<void(const SSAValue&)>& visitor) const
{
    visitor(Condition);
}

void SSAInstrGoto::forEachValue(const std::function<void(SSAValue&)>& visitor)
{
    // Gotos don't contain any SSAValues
    (void)visitor;
}

void SSAInstrGoto::forEachValue(const std::function<void(const SSAValue&)>& visitor) const
{
    // Gotos don't contain any SSAValues
    (void)visitor;
}

void SSAInstrPhi::forEachValue(const std::function<void(SSAValue&)>& visitor)
{
    visitor(Target);
    for (auto& cond : Conditions)
        visitor(cond);
    for (auto& branch : Branches)
        visitor(branch);
}

void SSAInstrPhi::forEachValue(const std::function<void(const SSAValue&)>& visitor) const
{
    visitor(Target);
    for (const auto& cond : Conditions)
        visitor(cond);
    for (const auto& branch : Branches)
        visitor(branch);
}

} // namespace PExpr::ssa
