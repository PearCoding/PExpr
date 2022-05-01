#pragma once

#include "PExpr_Config.h"
#include <string_view>

namespace PExpr {
/// Supported types.
enum class ElementaryType {
    Unspecified, /// Will be used internally.
    Boolean,     /// 'bool' Represented as 'bool' internally.
    Integer,     /// 'int' Represented as 'int64' internally.
    Number,      /// 'num' Represented as 'double' internally.
    Vec2,        /// 'vec2' Represented as two 'double's internally.
    Vec3,        /// 'vec3' Represented as three 'double's internally.
    Vec4,        /// 'vec4' Represented as four 'double's internally.
    String       /// 'vec2' Represented as 'std::string' internally.
};

/// Checks if a conversion from one type to another is possible.
/// Currently only 'int' -> 'num' is defined.
inline bool isConvertible(ElementaryType from, ElementaryType to)
{
    if (from == ElementaryType::Unspecified || to == ElementaryType::Unspecified)
        return false;

    if (from == to)
        return true;

    // We only allow implicit conversion from integer to float, but not back!
    return from == ElementaryType::Integer && to == ElementaryType::Number;
}

/// Checks if given type is one of 'vec2', 'vec3' or 'vec4'.
inline bool isArray(ElementaryType type)
{
    return type == ElementaryType::Vec2 || type == ElementaryType::Vec3 || type == ElementaryType::Vec4;
}

/// Checks if given type is one of 'int', 'num', 'vec2', 'vec3' or 'vec4'.
inline bool isArithmetic(ElementaryType type)
{
    return type == ElementaryType::Integer || type == ElementaryType::Number || isArray(type);
}

/// Returns the number of components the type has.
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

/// Supported unary operations.
enum class UnaryOperation {
    Pos, // +
    Neg, // -
    Not, // !
};

/// Supported binary operations.
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
    Error,    /// Internally used expression type.
    Variable, /// A standard variable access.
    Literal,  /// A literal.
    Unary,    /// Unary operation.
    Binary,   /// Binary operation.
    Call,     /// Call to a function.
    Access    /// Component swizzle operation.
};

/// Returns printable representation of the given type.
std::string_view toString(ElementaryType type);
/// Returns printable representation of the given operation.
std::string_view toString(UnaryOperation op);
/// Returns printable representation of the given operation.
std::string_view toString(BinaryOperation op);
} // namespace PExpr