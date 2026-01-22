#include "Token.h"

namespace PExpr::internal {
std::string Token::toString(TokenType type)
{
    switch (type) {
    default:
        if (type >= TokenType::Vec1Type)
            return std::string("vec") + std::to_string(Token::arraySize(type));
        else {
            PEXPR_ASSERT(false, "Invalid token type enum");
            return "ERROR";
        }
    case TokenType::Error:
        return "Error";
    case TokenType::Eof:
        return "EndOfStream";
    case TokenType::NumberLiteral:
        return "NumberLiteral";
    case TokenType::IntegerLiteral:
        return "IntegerLiteral";
    case TokenType::StringLiteral:
        return "StringLiteral";
    case TokenType::Identifier:
        return "Identifier";
    case TokenType::BooleanLiteral:
        return "BooleanLiteral";
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
    case TokenType::Colon:
        return ":";
    case TokenType::Semicolon:
        return ";";
    case TokenType::ExclamationMark:
        return "!";
    case TokenType::OpenParentheses:
        return "(";
    case TokenType::ClosedParentheses:
        return ")";
    case TokenType::OpenBraces:
        return "{";
    case TokenType::ClosedBraces:
        return "}";
    case TokenType::OpenSquareBracket:
        return "[";
    case TokenType::ClosedSquareBracket:
        return "]";
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
    case TokenType::Assign:
        return "=";
    case TokenType::ArrowRight:
        return "->";
    case TokenType::If:
        return "if";
    case TokenType::Elif:
        return "elif";
    case TokenType::Else:
        return "else";
    case TokenType::As:
        return "as";
    case TokenType::Let:
        return "let";
    case TokenType::Mutable:
        return "mut";
    case TokenType::Function:
        return "fn";
    case TokenType::BooleanType:
        return "bool";
    case TokenType::IntegerType:
        return "int";
    case TokenType::NumberType:
        return "num";
    case TokenType::StringType:
        return "str";
    }
}
} // namespace PExpr::internal
