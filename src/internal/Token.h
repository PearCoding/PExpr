#pragma once

#include "../Location.h"

namespace PExpr::internal {
enum class TokenType {
    Error,
    Eof,              // End
    NumberLiteral,    // 123.45e-6 (Number)
    IntegerLiteral,   // 123 (Integer)
    StringLiteral,    // "abc" (String)
    Identifier,       // ABC (String)
    BooleanLiteral,   // true, false (Boolean)
    Plus,             // +
    Minus,            // -
    Mul,              // *
    Div,              // /
    Mod,              // %
    Pow,              // ^
    Dot,              // .
    Comma,            // ,
    ExclamationMark,  // !
    OpenParanthese,   // (
    ClosedParanthese, // )
    OpenBraces,       // {
    ClosedBraces,     // }
    And,              // &&
    Or,               // ||
    Less,             // <
    Greater,          // >
    LessEqual,        // <=
    GreaterEqual,     // >=
    Equal,            // ==
    NotEqual,         // !=
    Colon,            // :
    Semicolon,        // ;
    Assign,           // =

    If,   // if
    Elif, // elif
    Else, // else

    Mutable,  // mut
    Function, // fn

    BooleanType, // bool
    IntegerType, // int
    NumberType,  // num
    Vec2Type,    // vec2
    Vec3Type,    // vec3
    Vec4Type,    // vec4
    StringType,  // str
};

class Token {
public:
    inline Token()
        : Location(0)
        , Type(TokenType::Error)
        , Value{}
    {
    }

    inline Token(const Location& location, TokenType type)
        : Location(location)
        , Type(type)
        , Value{}
    {
    }

    Token& With(bool b)
    {
        PEXPR_ASSERT(Type != TokenType::Error, "Expected a valid constructor call");
        Value = b;
        return *this;
    }

    Token& With(Integer v)
    {
        PEXPR_ASSERT(Type != TokenType::Error, "Expected a valid constructor call");
        Value = v;
        return *this;
    }

    Token& With(Number v)
    {
        PEXPR_ASSERT(Type != TokenType::Error, "Expected a valid constructor call");
        Value = v;
        return *this;
    }

    Token& With(const std::string& str)
    {
        PEXPR_ASSERT(Type != TokenType::Error, "Expected a valid constructor call");
        Value = str;
        return *this;
    }

    PExpr::Location Location;
    TokenType Type;
    ValueVariant Value;

    static std::string_view toString(TokenType type);
};
} // namespace PExpr::internal