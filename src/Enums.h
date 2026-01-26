#pragma once

#include "PExpr_Config.h"
#include <string_view>

namespace PExpr {
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
    Swizzle,  /// Component swizzle operation.
    Access,   /// Vector single component lookup
    Cast,     /// Implicit/explicit cast expression.
    Closure,  /// An enclosed closure
    Branch,   /// If, elif and else block
    Tuple,    /// Vector [x,y,z,w]
};

enum class StatementType {
    Error,               /// Internally used statement type.
    VariableDeclaration, /// New declaration of a variable
    VariableAssignment,  /// Update of a variable
    FunctionDeclaration, /// New declaration of a function
};

/// Returns printable representation of the given operation.
std::string_view toString(UnaryOperation op);
/// Returns printable representation of the given operation.
std::string_view toString(BinaryOperation op);

} // namespace PExpr
