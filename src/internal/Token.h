#pragma once

#include "Location.h"

namespace PExpr::internal {
enum class TokenType {
    Error,
    Eof,              // End
    Float,            // 123.45e-6 (Number)
    Integer,          // 123 (Integer)
    String,           // "abc" (String)
    Identifier,       // ABC (String)
    Boolean,          // true, false (Boolean)
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
    And,              // &&
    Or,               // ||
    Less,             // <
    Greater,          // >
    LessEqual,        // <=
    GreaterEqual,     // >=
    Equal,            // ==
    NotEqual,         // !=
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
        Value = b;
        return *this;
    }

    Token& With(Integer v)
    {
        Value = v;
        return *this;
    }

    Token& With(Number v)
    {
        Value = v;
        return *this;
    }

    Token& With(const std::string& str)
    {
        Value = str;
        return *this;
    }

    PExpr::Location Location;
    TokenType Type;
    ValueVariant Value;

    static std::string_view toString(TokenType type);
};
} // namespace PExpr