#include "Enums.h"

namespace PExpr::ast {

std::string_view toString(UnaryOperation op)
{
    switch (op) {
    case UnaryOperation::Pos:
        return "+";
    case UnaryOperation::Neg:
        return "-";
    case UnaryOperation::Not:
        return "!";
    default:
        PEXPR_ASSERT(false, "Invalid unary operation enum");
        return "";
    }
}

std::string_view toString(BinaryOperation op)
{
    switch (op) {
    case BinaryOperation::Add:
        return "+";
    case BinaryOperation::Sub:
        return "-";
    case BinaryOperation::Mul:
        return "*";
    case BinaryOperation::Div:
        return "/";
    case BinaryOperation::Pow:
        return "^";
    case BinaryOperation::Mod:
        return "%";
    case BinaryOperation::And:
        return "&&";
    case BinaryOperation::Or:
        return "||";
    case BinaryOperation::Less:
        return "<";
    case BinaryOperation::Greater:
        return ">";
    case BinaryOperation::LessEqual:
        return "<=";
    case BinaryOperation::GreaterEqual:
        return ">=";
    case BinaryOperation::Equal:
        return "==";
    case BinaryOperation::NotEqual:
        return "!=";
    default:
        PEXPR_ASSERT(false, "Invalid binary operation enum");
        return "";
    }
}

} // namespace PExpr