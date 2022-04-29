#pragma once

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
} // namespace PExpr