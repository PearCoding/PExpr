#pragma once

#include "pexpr_config.h"
#include <string_view>

namespace PExpr {
enum class ElementaryType {
    Unspecified,
    Boolean,
    Integer,
    Number,
    Vec2,
    Vec3,
    Vec4,
    String
};

inline bool isConvertible(ElementaryType from, ElementaryType to)
{
    if (from == ElementaryType::Unspecified || to == ElementaryType::Unspecified)
        return false;

    if (from == to)
        return true;

    // We only allow implicit conversion from integer to float, but not back!
    return from == ElementaryType::Integer && to == ElementaryType::Number;
}

inline bool isArray(ElementaryType type)
{
    return type == ElementaryType::Vec2 || type == ElementaryType::Vec3 || type == ElementaryType::Vec4;
}

inline bool isArithmetic(ElementaryType type)
{
    return type == ElementaryType::Integer || type == ElementaryType::Number || isArray(type);
}

inline uint8 typeArraySize(ElementaryType type)
{
    switch (type) {
    default:
        return 1;
    case ElementaryType::Vec2:
        return 2;
    case ElementaryType::Vec3:
        return 3;
    case ElementaryType::Vec4:
        return 4;
    }
}

enum class UnaryOperation {
    Pos, // +
    Neg, // -
    Not, // !
};

enum class BinaryOperation {
    Add,          // +
    Sub,          // -
    Mul,          // *
    Div,          // /
    Pow,          // ^
    Mod,          // %
    And,          // &&
    Or,           // ||
    Less,         // <
    Greater,      // >
    LessEqual,    // <=
    GreaterEqual, // >=
    Equal,        // ==
    NotEqual,     // !=
};

enum class ExpressionType {
    Error,
    Variable,
    Const,
    Unary,
    Binary,
    Call,
    Access
};

std::string_view toString(ElementaryType type);
std::string_view toString(UnaryOperation op);
std::string_view toString(BinaryOperation op);
} // namespace PExpr