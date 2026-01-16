#pragma once

#include "PExpr_Config.h"
#include <string_view>

namespace PExpr {
/// Supported types.
enum class ElementaryType {
    Error,       /// Issue during type checking
    Unspecified, /// Will be used internally.
    Boolean,     /// 'bool' Represented as 'bool' internally.
    Integer,     /// 'int' Represented as 'int64' internally.
    Number,      /// 'num' Represented as 'double' internally.
    String,      /// 'str' Represented as 'std::string' internally.
    Vec1,        /// The vectors are listed after this Vec4 = Vec1 + 3
};

/// Checks if a conversion from one type to another is possible.
/// Currently only 'int' -> 'num' is defined (implicit conversion).
inline bool isConvertible(ElementaryType from, ElementaryType to)
{
    if (from == ElementaryType::Error || to == ElementaryType::Error)
        return false;

    if (from == ElementaryType::Unspecified || to == ElementaryType::Unspecified)
        return false;

    if (from == to)
        return true;

    // We only allow implicit conversion from integer to float, but not back!
    return from == ElementaryType::Integer && to == ElementaryType::Number;
}

/// Checks if an explicit conversion (cast) from one type to another is allowed.
/// This is a superset of isConvertible and includes conversions that require an
/// explicit cast by the user (for example num -> int).
inline bool isExplicitConvertible(ElementaryType from, ElementaryType to)
{
    // Implicit conversions are always allowed explicitly as well.
    if (isConvertible(from, to))
        return true;

    // Allow explicit narrowing from num to int.
    if (from == ElementaryType::Number && to == ElementaryType::Integer)
        return true;

    return false;
}

/// Checks if given type is a vector.
inline bool isArray(ElementaryType type)
{
    return type >= ElementaryType::Vec1;
}

/// Checks if given type is a 'int', 'num' or a vector.
inline bool isArithmetic(ElementaryType type)
{
    return type == ElementaryType::Integer || type == ElementaryType::Number || isArray(type);
}

/// Returns the number of components the type has.
inline size_t typeArraySize(ElementaryType type)
{
    if (isArray(type))
        return (size_t)type - (size_t)ElementaryType::Vec1 + 1;
    return 1;
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
    Access,   /// Component swizzle operation.
    Cast,     /// Implicit/explicit cast expression.
    Closure,  /// An enclosed closure
    Branch,   /// If, elif and else block
    Vector,   /// Vector [x,y,z,w]
};

enum class StatementType {
    Error,               /// Internally used statement type.
    VariableDeclaration, /// New declaration of a variable
    VariableAssignment,  /// Update of a variable
    FunctionDeclaration, /// New declaration of a function
};

/// Returns printable representation of the given type.
std::string toString(ElementaryType type);
/// Returns printable representation of the given operation.
std::string_view toString(UnaryOperation op);
/// Returns printable representation of the given operation.
std::string_view toString(BinaryOperation op);
} // namespace PExpr
