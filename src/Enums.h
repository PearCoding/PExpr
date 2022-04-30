#pragma once

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