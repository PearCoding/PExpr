#pragma once

#include "../Location.h"

namespace PExpr::internal {
enum class TokenType {
    Error,
    Eof,                 // End
    NumberLiteral,       // 123.45e-6 (Number)
    IntegerLiteral,      // 123 (Integer)
    StringLiteral,       // "abc" (String)
    Identifier,          // ABC (String)
    BooleanLiteral,      // true, false (Boolean)
    Plus,                // +
    Minus,               // -
    Mul,                 // *
    Div,                 // /
    Mod,                 // %
    Pow,                 // ^
    Dot,                 // .
    Comma,               // ,
    ExclamationMark,     // !
    OpenParentheses,     // (
    ClosedParentheses,   // )
    OpenBraces,          // {
    ClosedBraces,        // }
    OpenSquareBracket,   // [
    ClosedSquareBracket, // ]
    And,                 // &&
    Or,                  // ||
    Less,                // <
    Greater,             // >
    LessEqual,           // <=
    GreaterEqual,        // >=
    Equal,               // ==
    NotEqual,            // !=
    Colon,               // :
    Semicolon,           // ;
    Assign,              // =

    ArrowRight, // ->

    If,   // if
    Elif, // elif
    Else, // else

    As,       // as
    Let,      // let
    Mutable,  // mut
    Function, // fn

    BooleanType, // bool
    IntegerType, // int
    NumberType,  // num
    StringType,  // str
    Vec1Type,    // vec1 -> vec4 = vec1 + 3
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
    ElementaryValueVariant Value;

    [[nodiscard]] inline static size_t arraySize(TokenType t)
    {
        if (t >= TokenType::Vec1Type)
            return (size_t)t - (size_t)TokenType::Vec1Type + 1;
        else
            return 1;
    }

    [[nodiscard]] inline size_t arraySize() const { return Token::arraySize(Type); }

    static std::string toString(TokenType type);
};
} // namespace PExpr::internal
