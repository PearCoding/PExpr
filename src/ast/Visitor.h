#pragma once

#include "PExpr.h"

#include <functional>

namespace PExpr::ast {
class Closure;
class Expression;

/// Visitor for traversing AST nodes recursively
class Visitor {
public:
    // TODO: Extend this by having an additional "shouldEnter" kind of callback

    //-------------------------------------------------------------------------------------------------------

    /// Visit all expressions in a closure, including nested expressions.
    /// The callback receives the closure containing the expression and the expression itself.
    static void forEachExpression(const Closure* closure,
                                  const std::function<void(const Closure*, const Expression*)>& callback,
                                  bool visitFirst = true);

    /// Visit all expressions in an expression, including nested expressions.
    /// The callback receives the closure containing the expression and the expression itself.
    static void forEachExpression(const Expression* expression,
                                  const Closure* context,
                                  const std::function<void(const Closure*, const Expression*)>& callback,
                                  bool visitFirst = true);

    /// Visit all closures in a closure, including nested closures.
    /// The callback receives the closure itself.
    static void forEachClosure(const Closure* closure,
                               const std::function<void(const Closure*)>& callback,
                               bool visitFirst = true);

    //-------------------------------------------------------------------------------------------------------

    /// Visit all expressions in a closure, including nested expressions.
    /// The callback receives the closure containing the expression and the expression itself.
    static void forEachExpression(Closure* closure,
                                  const std::function<void(Closure*, Expression*)>& callback,
                                  bool visitFirst = true);

    /// Visit all expressions in an expression, including nested expressions.
    /// The callback receives the closure containing the expression and the expression itself.
    static void forEachExpression(Expression* expression,
                                  Closure* context,
                                  const std::function<void(Closure*, Expression*)>& callback,
                                  bool visitFirst = true);

    /// Visit all closures in a closure, including nested closures.
    /// The callback receives the closure itself.
    static void forEachClosure(Closure* closure,
                               const std::function<void(Closure*)>& callback,
                               bool visitFirst = true);
};

} // namespace PExpr::ast