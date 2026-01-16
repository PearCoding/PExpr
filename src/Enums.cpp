#include "Enums.h"

namespace PExpr {

std::string toString(ElementaryType type)
{
    switch (type) {
    default:
        if (isArray(type)) {
            return std::string("vec") + std::to_string(typeArraySize(type));
        } else {
            PEXPR_ASSERT(false, "Invalid elementary type");
            return "invalid";
        }
    case ElementaryType::Error:
        return "error";
    case ElementaryType::Unspecified:
        return "unspecified";
    case ElementaryType::Boolean:
        return "bool";
    case ElementaryType::Integer:
        return "int";
    case ElementaryType::Number:
        return "num";
    case ElementaryType::String:
        return "str";
    }
}

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