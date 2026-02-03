#include "Visitor.h"
#include "Closure.h"
#include "Expression.h"
#include "Statement.h"

namespace PExpr::ast {

// Helper implementations for Visitor class
template <bool CallFirst, bool IsConst>
class VisitorImpl {
public:
    using ClosureClass    = typename std::conditional_t<IsConst, const Closure*, Closure*>;
    using ExpressionClass = typename std::conditional_t<IsConst, const Expression*, Expression*>;

    template <typename T>
    using WrapConst = typename std::conditional_t<IsConst, typename std::add_const_t<typename std::remove_const_t<T>>, typename std::remove_const_t<T>>;

    static void forEachOnClosure(ClosureClass closure,
                                 const std::function<void(ClosureClass, ExpressionClass)>& callbackExpr,
                                 const std::function<void(ClosureClass)>& callbackClosure)
    {
        if (!closure)
            return;

        if constexpr (CallFirst) {
            for (const auto& expr : closure->expressions())
                callbackExpr(closure, expr.get());
        }

        // Visit all statements in the closure
        for (const auto& expr : closure->expressions())
            forEachOnExpression(expr.get(), closure, callbackExpr, callbackClosure);

        if constexpr (!CallFirst) {
            for (const auto& expr : closure->expressions())
                callbackExpr(closure, expr.get());
        }
    }

    static void forEachOnExpression(ExpressionClass expression, ClosureClass context,
                                    const std::function<void(ClosureClass, ExpressionClass)>& callbackExpr,
                                    const std::function<void(ClosureClass)>& callbackClosure)
    {
        if (!expression)
            return;

        // Visit the expression itself (handled by the caller)
        // Then recursively visit its subexpressions based on type
        switch (expression->type()) {
        case ExpressionType::Cast: {
            auto cast = dynamic_cast<const CastExpression*>(expression);
            if (cast && cast->inner()) {
                if constexpr (CallFirst) {
                    if (callbackExpr)
                        callbackExpr(context, cast->inner().get());
                }
                forEachOnExpression(cast->inner().get(), context, callbackExpr, callbackClosure);
                if constexpr (!CallFirst) {
                    if (callbackExpr)
                        callbackExpr(context, cast->inner().get());
                }
            }
            break;
        }
        case ExpressionType::Unary: {
            auto unary = dynamic_cast<const UnaryExpression*>(expression);
            if (unary && unary->inner()) {
                if constexpr (CallFirst) {
                    if (callbackExpr)
                        callbackExpr(context, unary->inner().get());
                }
                forEachOnExpression(unary->inner().get(), context, callbackExpr, callbackClosure);
                if constexpr (!CallFirst) {
                    if (callbackExpr)
                        callbackExpr(context, unary->inner().get());
                }
            }
            break;
        }
        case ExpressionType::Binary: {
            auto binary = dynamic_cast<const BinaryExpression*>(expression);
            if (binary && binary->left() && binary->right()) {
                if constexpr (CallFirst) {
                    if (callbackExpr) {
                        callbackExpr(context, binary->left().get());
                        callbackExpr(context, binary->right().get());
                    }
                }
                forEachOnExpression(binary->left().get(), context, callbackExpr, callbackClosure);
                forEachOnExpression(binary->right().get(), context, callbackExpr, callbackClosure);
                if constexpr (!CallFirst) {
                    if (callbackExpr) {
                        callbackExpr(context, binary->left().get());
                        callbackExpr(context, binary->right().get());
                    }
                }
            }
            break;
        }
        case ExpressionType::Call: {
            auto call = dynamic_cast<const CallExpression*>(expression);
            if (call) {
                for (const auto& param : call->parameters()) {
                    if constexpr (CallFirst) {
                        if (callbackExpr)
                            callbackExpr(context, param.get());
                    }
                    forEachOnExpression(param.get(), context, callbackExpr, callbackClosure);
                    if constexpr (!CallFirst) {
                        if (callbackExpr)
                            callbackExpr(context, param.get());
                    }
                }
            }
            break;
        }
        case ExpressionType::Swizzle: {
            auto swizzle = dynamic_cast<const SwizzleExpression*>(expression);
            if (swizzle && swizzle->inner()) {
                if constexpr (CallFirst) {
                    if (callbackExpr)
                        callbackExpr(context, swizzle->inner().get());
                }
                forEachOnExpression(swizzle->inner().get(), context, callbackExpr, callbackClosure);
                if constexpr (!CallFirst) {
                    if (callbackExpr)
                        callbackExpr(context, swizzle->inner().get());
                }
            }
            break;
        }
        case ExpressionType::Access: {
            auto access = dynamic_cast<const AccessExpression*>(expression);
            if (access && access->inner()) {
                if constexpr (CallFirst) {
                    if (callbackExpr)
                        callbackExpr(context, access->inner().get());
                }
                forEachOnExpression(access->inner().get(), context, callbackExpr, callbackClosure);
                if constexpr (!CallFirst) {
                    if (callbackExpr)
                        callbackExpr(context, access->inner().get());
                }
            }
            break;
        }
        case ExpressionType::Closure: {
            auto closureExpr = dynamic_cast<const ClosureExpression*>(expression);
            if (closureExpr && closureExpr->closure()) {
                if constexpr (CallFirst) {
                    if (callbackClosure)
                        callbackClosure(closureExpr->closure().get());
                }
                forEachOnClosure(closureExpr->closure().get(), callbackExpr, callbackClosure);
                if constexpr (!CallFirst) {
                    if (callbackClosure)
                        callbackClosure(closureExpr->closure().get());
                }
            }
            break;
        }
        case ExpressionType::Branch: {
            auto branch = dynamic_cast<const BranchExpression*>(expression);
            if (branch) {
                if constexpr (CallFirst) {
                    for (const auto& b : branch->branches()) {
                        if (b.Condition) {
                            if (callbackExpr)
                                callbackExpr(context, b.Condition.get());
                        }
                        if (b.Body) {
                            if (callbackClosure)
                                callbackClosure(b.Body.get());
                        }
                    }
                    if (callbackClosure && branch->elseClosure())
                        callbackClosure(branch->elseClosure().get());
                }

                // Visit conditions and branches
                for (const auto& b : branch->branches()) {
                    if (b.Condition)
                        forEachOnExpression(b.Condition.get(), context, callbackExpr, callbackClosure);
                    if (b.Body)
                        forEachOnClosure(b.Body.get(), callbackExpr, callbackClosure);
                }

                // Visit else closure
                if (branch->elseClosure())
                    forEachOnClosure(branch->elseClosure().get(), callbackExpr, callbackClosure);

                if constexpr (!CallFirst) {
                    for (const auto& b : branch->branches()) {
                        if (b.Condition) {
                            if (callbackExpr)
                                callbackExpr(context, b.Condition.get());
                        }
                        if (b.Body) {
                            if (callbackClosure)
                                callbackClosure(b.Body.get());
                        }
                    }
                    if (callbackClosure && branch->elseClosure())
                        callbackClosure(branch->elseClosure().get());
                }
            }
            break;
        }
        case ExpressionType::Tuple: {
            auto tuple = dynamic_cast<const TupleExpression*>(expression);
            if (tuple) {
                for (const auto& entry : tuple->entries()) {
                    if constexpr (CallFirst) {
                        if (callbackExpr)
                            callbackExpr(context, entry.get());
                    }
                    forEachOnExpression(entry.get(), context, callbackExpr, callbackClosure);
                    if constexpr (!CallFirst) {
                        if (callbackExpr)
                            callbackExpr(context, entry.get());
                    }
                }
            }
            break;
        }
        case ExpressionType::VariableDeclaration: {
            auto decl = dynamic_cast<const VariableDeclarationStatement*>(expression);
            if (decl && decl->expression()) {
                if constexpr (CallFirst) {
                    if (callbackExpr)
                        callbackExpr(context, decl->expression().get());
                }
                forEachOnExpression(decl->expression().get(), context, callbackExpr, callbackClosure);
                if constexpr (!CallFirst)
                    if (callbackExpr)
                        callbackExpr(context, decl->expression().get());
            }
            break;
        }
        case ExpressionType::VariableAssignment: {
            auto assign = dynamic_cast<const VariableAssignmentStatement*>(expression);
            if (assign && assign->expression()) {
                if constexpr (CallFirst) {
                    if (callbackExpr)
                        callbackExpr(context, assign->expression().get());
                }
                forEachOnExpression(assign->expression().get(), context, callbackExpr, callbackClosure);
                if constexpr (!CallFirst) {
                    if (callbackExpr)
                        callbackExpr(context, assign->expression().get());
                }
            }
            break;
        }
        case ExpressionType::FunctionDeclaration: {
            auto func = dynamic_cast<const FunctionDeclarationStatement*>(expression);
            if (func && func->closure()) {
                // For function declarations, visit the closure with the closure as context
                if constexpr (CallFirst) {
                    if (callbackClosure)
                        callbackClosure(func->closure().get());
                }
                forEachOnClosure(func->closure().get(), callbackExpr, callbackClosure);
                if constexpr (!CallFirst) {
                    if (callbackClosure)
                        callbackClosure(func->closure().get());
                }
            }
            break;
        }
        case ExpressionType::TypeAlias:
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
};

// Public interface implementations

void Visitor::forEachExpression(const Closure* closure,
                                const std::function<void(const Closure*, const Expression*)>& callback,
                                bool visitFirst)
{
    if (visitFirst)
        VisitorImpl<true, true>::forEachOnClosure(closure, callback, nullptr);
    else
        VisitorImpl<false, true>::forEachOnClosure(closure, callback, nullptr);
}

void Visitor::forEachExpression(const Expression* expression,
                                const Closure* context,
                                const std::function<void(const Closure*, const Expression*)>& callback,
                                bool visitFirst)
{
    if (visitFirst)
        VisitorImpl<true, true>::forEachOnExpression(expression, context, callback, nullptr);
    else
        VisitorImpl<false, true>::forEachOnExpression(expression, context, callback, nullptr);
}

void Visitor::forEachClosure(const Closure* closure,
                             const std::function<void(const Closure*)>& callback,
                             bool visitFirst)
{
    if (visitFirst)
        VisitorImpl<true, true>::forEachOnClosure(closure, nullptr,  callback);
    else
        VisitorImpl<false, true>::forEachOnClosure(closure, nullptr,  callback);
}

void Visitor::forEachExpression(Closure* closure,
                                const std::function<void(Closure*, Expression*)>& callback,
                                bool visitFirst)
{
    if (visitFirst)
        VisitorImpl<true, false>::forEachOnClosure(closure, callback,  nullptr);
    else
        VisitorImpl<false, false>::forEachOnClosure(closure, callback, nullptr);
}

void Visitor::forEachExpression(Expression* expression,
                                Closure* context,
                                const std::function<void(Closure*, Expression*)>& callback,
                                bool visitFirst)
{
    if (visitFirst)
        VisitorImpl<true, false>::forEachOnExpression(expression, context, callback,  nullptr);
    else
        VisitorImpl<false, false>::forEachOnExpression(expression, context, callback,  nullptr);
}

void Visitor::forEachClosure(Closure* closure,
                             const std::function<void(Closure*)>& callback,
                             bool visitFirst)
{
    if (visitFirst)
        VisitorImpl<true, false>::forEachOnClosure(closure, nullptr, callback);
    else
        VisitorImpl<false, false>::forEachOnClosure(closure, nullptr, callback);
}

} // namespace PExpr::ast