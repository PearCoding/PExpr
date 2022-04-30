#pragma once

#include "pexpr_config.h"

namespace PExpr {
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

    inline Token(size_t location, TokenType type)
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

    size_t Location;
    TokenType Type;
    ValueVariant Value;

    static std::string_view toString(TokenType type);
};
} // namespace PExpr