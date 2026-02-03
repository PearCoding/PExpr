#pragma once

#include "PExpr.h"
#include <string_view>

namespace PExpr::ast {
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
    Error,               /// Internally used expression type.
    VariableDeclaration, /// New declaration of a variable (single or destructuring) [statement -> void]
    VariableAssignment,  /// Update of a variable (single or destructuring)          [statement -> void]
    FunctionDeclaration, /// New declaration of a function                           [statement -> void]
    TypeAlias,           /// Type alias declaration using 'using'                    [statement -> void]
    Variable,            /// A standard variable access.
    Literal,             /// A literal.
    Unary,               /// Unary operation.
    Binary,              /// Binary operation.
    Call,                /// Call to a function.
    Swizzle,             /// Component swizzle operation.
    Access,              /// Vector single component lookup
    Cast,                /// Implicit/explicit cast expression.
    Closure,             /// An enclosed closure
    Branch,              /// If, elif and else block
    Tuple,               /// Vector [x,y,z,w]
};

/// Returns printable representation of the given operation.
std::string_view toString(UnaryOperation op);
/// Returns printable representation of the given operation.
std::string_view toString(BinaryOperation op);

} // namespace PExpr::ast
