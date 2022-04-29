#pragma once

namespace PExpr {
enum class Type {
    Boolean,
    Integer,
    Float,
    Vec2,
    Vec3,
    Vec4,
    String
};

enum class Operation {
    Add,          // +
    Sub,          // -
    Mul,          // *
    Div,          // /
    Pow,          // ^
    Mod,          // %
    Neg,          // -
    Not,          // !
    And,          // &&
    Or,           // ||
    Less,         // <
    Greater,      // >
    LessEqual,    // <=
    GreaterEqual, // >=
    Equal,        // ==
    NotEqual,     // !=
    Call,         // foo(...)
    Element       // foo.swizzle
};
}