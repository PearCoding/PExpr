#pragma once

#include "PExpr.h"

#include <functional>

namespace PExpr::ast {
class Closure;
class Expression;
class Statement;

/// Visitor for traversing AST nodes recursively
class Visitor {
public:
    /// Visit all expressions in a closure, including nested expressions.
    /// The callback receives the closure containing the expression and the expression itself.
    static void forEachExpression(const Closure* closure,
                                  const std::function<void(const Closure*, const Expression*)>& callback);

    /// Visit all expressions in a statement, including nested expressions.
    /// The callback receives the closure containing the expression and the expression itself.
    static void forEachExpression(const Statement* statement,
                                  const Closure* context,
                                  const std::function<void(const Closure*, const Expression*)>& callback);

    /// Visit all expressions in an expression, including nested expressions.
    /// The callback receives the closure containing the expression and the expression itself.
    static void forEachExpression(const Expression* expression,
                                  const Closure* context,
                                  const std::function<void(const Closure*, const Expression*)>& callback);

    /// Visit all statements in a closure, including statements in nested closures.
    /// The callback receives the closure containing the statement and the statement itself.
    static void forEachStatement(const Closure* closure,
                                 const std::function<void(const Closure*, const Statement*)>& callback);

    /// Visit all closures in a closure, including nested closures.
    /// The callback receives the closure itself.
    static void forEachClosure(const Closure* closure,
                               const std::function<void(const Closure*)>& callback);

private:
    // Helper methods for internal recursion
    static void forEachExpressionImpl(const Closure* closure,
                                      const std::function<void(const Closure*, const Expression*)>& callback);

    static void forEachExpressionImpl(const Statement* statement,
                                      const std::function<void(const Closure*, const Expression*)>& callback,
                                      const Closure* context);

    static void forEachExpressionImpl(const Expression* expression,
                                      const std::function<void(const Closure*, const Expression*)>& callback,
                                      const Closure* context);

    static void forEachStatementImpl(const Closure* closure,
                                     const std::function<void(const Closure*, const Statement*)>& callback);

    static void forEachClosureImpl(const Closure* closure,
                                   const std::function<void(const Closure*)>& callback,
                                   bool isRoot);
};

} // namespace PExpr::ast