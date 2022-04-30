#include "Token.h"

namespace PExpr {
std::string_view Token::toString(TokenType type)
{
    switch (type) {
    default:
    case TokenType::Error:
        return "Error";
    case TokenType::Eof:
        return "EndOfStream";
    case TokenType::Float:
        return "Float";
    case TokenType::Integer:
        return "Integer";
    case TokenType::String:
        return "String";
    case TokenType::Identifier:
        return "Identifier";
    case TokenType::Boolean:
        return "Boolean";
    case TokenType::Plus:
        return "+";
    case TokenType::Minus:
        return "-";
    case TokenType::Mul:
        return "*";
    case TokenType::Div:
        return "/";
    case TokenType::Mod:
        return "%";
    case TokenType::Pow:
        return "^";
    case TokenType::Dot:
        return ".";
    case TokenType::Comma:
        return ",";
    case TokenType::ExclamationMark:
        return "!";
    case TokenType::OpenParanthese:
        return "(";
    case TokenType::ClosedParanthese:
        return ")";
    case TokenType::And:
        return "&&";
    case TokenType::Or:
        return "||";
    case TokenType::Less:
        return "<";
    case TokenType::Greater:
        return ">";
    case TokenType::LessEqual:
        return "<=";
    case TokenType::GreaterEqual:
        return ">=";
    case TokenType::Equal:
        return "==";
    case TokenType::NotEqual:
        return "!=";
    }
}
} // namespace PExpr