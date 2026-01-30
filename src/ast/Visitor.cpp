#include "Visitor.h"
#include "Closure.h"
#include "Expression.h"
#include "Statement.h"

namespace PExpr::ast {

// Helper implementations for Visitor class

void Visitor::forEachExpressionImpl(const Closure* closure,
                                    const std::function<void(const Closure*, const Expression*)>& callback)
{
    if (!closure)
        return;

    // Visit the closure's expression if it exists
    if (closure->expression()) {
        callback(closure, closure->expression().get());
        forEachExpressionImpl(closure->expression().get(), callback, closure);
    }

    // Visit all statements in the closure
    for (const auto& stmt : closure->statements())
        forEachExpressionImpl(stmt.get(), callback, closure);
}

void Visitor::forEachExpressionImpl(const Statement* statement,
                                    const std::function<void(const Closure*, const Expression*)>& callback,
                                    const Closure* context)
{
    if (!statement)
        return;

    // Handle different statement types
    switch (statement->type()) {
    case StatementType::VariableDeclaration: {
        auto decl = dynamic_cast<const VariableDeclarationStatement*>(statement);
        if (decl && decl->expression()) {
            callback(context, decl->expression().get());
            forEachExpressionImpl(decl->expression().get(), callback, context);
        }
        break;
    }
    case StatementType::VariableAssignment: {
        auto assign = dynamic_cast<const VariableAssignmentStatement*>(statement);
        if (assign && assign->expression()) {
            callback(context, assign->expression().get());
            forEachExpressionImpl(assign->expression().get(), callback, context);
        }
        break;
    }
    case StatementType::FunctionDeclaration: {
        auto func = dynamic_cast<const FunctionDeclarationStatement*>(statement);
        if (func && func->closure()) {
            // For function declarations, visit the closure with the closure as context
            forEachExpressionImpl(func->closure().get(), callback);
        }
        break;
    }
    case StatementType::TypeAlias:
    case StatementType::Error:
        // No expressions in these statement types
        break;
    default:
        PEXPR_ASSERT(false, "Non exhaustive statement types");
        break;
    }
}

void Visitor::forEachExpressionImpl(const Expression* expression,
                                    const std::function<void(const Closure*, const Expression*)>& callback,
                                    const Closure* context)
{
    if (!expression)
        return;

    // Visit the expression itself (handled by the caller)
    // Then recursively visit its subexpressions based on type
    switch (expression->type()) {
    case ExpressionType::Cast: {
        auto cast = dynamic_cast<const CastExpression*>(expression);
        if (cast && cast->inner()) {
            callback(context, cast->inner().get());
            forEachExpressionImpl(cast->inner().get(), callback, context);
        }
        break;
    }
    case ExpressionType::Unary: {
        auto unary = dynamic_cast<const UnaryExpression*>(expression);
        if (unary && unary->inner()) {
            callback(context, unary->inner().get());
            forEachExpressionImpl(unary->inner().get(), callback, context);
        }
        break;
    }
    case ExpressionType::Binary: {
        auto binary = dynamic_cast<const BinaryExpression*>(expression);
        if (binary && binary->left() && binary->right()) {
            callback(context, binary->left().get());
            callback(context, binary->right().get());
            forEachExpressionImpl(binary->left().get(), callback, context);
            forEachExpressionImpl(binary->right().get(), callback, context);
        }
        break;
    }
    case ExpressionType::Call: {
        auto call = dynamic_cast<const CallExpression*>(expression);
        if (call) {
            for (const auto& param : call->parameters()) {
                callback(context, param.get());
                forEachExpressionImpl(param.get(), callback, context);
            }
        }
        break;
    }
    case ExpressionType::Swizzle: {
        auto swizzle = dynamic_cast<const SwizzleExpression*>(expression);
        if (swizzle && swizzle->inner()) {
            callback(context, swizzle->inner().get());
            forEachExpressionImpl(swizzle->inner().get(), callback, context);
        }
        break;
    }
    case ExpressionType::Access: {
        auto access = dynamic_cast<const AccessExpression*>(expression);
        if (access && access->inner()) {
            callback(context, access->inner().get());
            forEachExpressionImpl(access->inner().get(), callback, context);
        }
        break;
    }
    case ExpressionType::Closure: {
        auto closureExpr = dynamic_cast<const ClosureExpression*>(expression);
        if (closureExpr && closureExpr->closure()) {
            // Call callback for the closure expression itself with parent context
            // Then recursively visit expressions inside the closure
            forEachExpressionImpl(closureExpr->closure().get(), callback);
        }
        break;
    }
    case ExpressionType::Branch: {
        auto branch = dynamic_cast<const BranchExpression*>(expression);
        if (branch) {
            // Visit conditions
            for (const auto& b : branch->branches()) {
                if (b.Condition) {
                    callback(context, b.Condition.get());
                    forEachExpressionImpl(b.Condition.get(), callback, context);
                }
            }
            // Visit closures in branches
            for (const auto& b : branch->branches()) {
                if (b.Body) {
                    forEachExpressionImpl(b.Body.get(), callback);
                }
            }
            // Visit else closure
            if (branch->elseClosure()) {
                forEachExpressionImpl(branch->elseClosure().get(), callback);
            }
        }
        break;
    }
    case ExpressionType::Tuple: {
        auto tuple = dynamic_cast<const TupleExpression*>(expression);
        if (tuple) {
            for (const auto& entry : tuple->entries()) {
                callback(context, entry.get());
                forEachExpressionImpl(entry.get(), callback, context);
            }
        }
        break;
    }
    case ExpressionType::Variable:
    case ExpressionType::Literal:
    case ExpressionType::Error:
        // No subexpressions in these types
        break;
    default:
        PEXPR_ASSERT(false, "Non exhaustive expression types");
        break;
    }
}

void Visitor::forEachStatementImpl(const Closure* closure,
                                   const std::function<void(const Closure*, const Statement*)>& callback)
{
    if (!closure)
        return;

    // Visit all statements in this closure
    for (const auto& stmt : closure->statements())
        callback(closure, stmt.get());

    // Recursively visit statements in nested closures
    auto nestedVisitor = [&](const Closure* contextClosure, const Expression* expr) {
        PEXPR_UNUSED(contextClosure);

        if (expr->type() == ExpressionType::Closure) {
            auto closureExpr = dynamic_cast<const ClosureExpression*>(expr);
            if (closureExpr && closureExpr->closure()) {
                for (const auto& stmt : closureExpr->closure()->statements())
                    callback(closureExpr->closure().get(), stmt.get());
                // Recursively visit nested closures
                forEachStatementImpl(closureExpr->closure().get(), callback);
            }
        } else if (expr->type() == ExpressionType::Branch) {
            auto branch = dynamic_cast<const BranchExpression*>(expr);
            if (branch) {
                // Visit statements in branch bodies
                for (const auto& b : branch->branches()) {
                    if (b.Body) {
                        for (const auto& stmt : b.Body->statements())
                            callback(b.Body.get(), stmt.get());
                        forEachStatementImpl(b.Body.get(), callback);
                    }
                }
                // Visit statements in else closure
                if (branch->elseClosure()) {
                    for (const auto& stmt : branch->elseClosure()->statements()) 
                        callback(branch->elseClosure().get(), stmt.get());
                    forEachStatementImpl(branch->elseClosure().get(), callback);
                }
            }
        }
    };

    forEachExpressionImpl(closure, nestedVisitor);
}

void Visitor::forEachClosureImpl(const Closure* closure,
                                 const std::function<void(const Closure*)>& callback,
                                 bool isRoot)
{
    if (!closure)
        return;

    // Visit this closure if not root (root is the starting closure, don't callback for it unless specified)
    if (!isRoot)
        callback(closure);

    // Recursively visit nested closures
    auto nestedVisitor = [&](const Closure* contextClosure, const Expression* expr) {
        PEXPR_UNUSED(contextClosure);

        if (expr->type() == ExpressionType::Closure) {
            auto closureExpr = dynamic_cast<const ClosureExpression*>(expr);
            if (closureExpr && closureExpr->closure()) {
                callback(closureExpr->closure().get());
                forEachClosureImpl(closureExpr->closure().get(), callback, false);
            }
        } else if (expr->type() == ExpressionType::Branch) {
            auto branch = dynamic_cast<const BranchExpression*>(expr);
            if (branch) {
                // Visit closures in branches
                for (const auto& b : branch->branches()) {
                    if (b.Body) {
                        callback(b.Body.get());
                        forEachClosureImpl(b.Body.get(), callback, false);
                    }
                }
                // Visit else closure
                if (branch->elseClosure()) {
                    callback(branch->elseClosure().get());
                    forEachClosureImpl(branch->elseClosure().get(), callback, false);
                }
            }
        }
    };

    forEachExpressionImpl(closure, nestedVisitor);
}

// Public interface implementations

void Visitor::forEachExpression(const Closure* closure,
                                const std::function<void(const Closure*, const Expression*)>& callback)
{
    forEachExpressionImpl(closure, callback);
}

void Visitor::forEachExpression(const Statement* statement,
                                const Closure* context,
                                const std::function<void(const Closure*, const Expression*)>& callback)
{
    forEachExpressionImpl(statement, callback, context);
}

void Visitor::forEachExpression(const Expression* expression,
                                const Closure* context,
                                const std::function<void(const Closure*, const Expression*)>& callback)
{
    forEachExpressionImpl(expression, callback, context);
}

void Visitor::forEachStatement(const Closure* closure,
                               const std::function<void(const Closure*, const Statement*)>& callback)
{
    forEachStatementImpl(closure, callback);
}

void Visitor::forEachClosure(const Closure* closure,
                             const std::function<void(const Closure*)>& callback)
{
    forEachClosureImpl(closure, callback, true);
}

} // namespace PExpr::ast