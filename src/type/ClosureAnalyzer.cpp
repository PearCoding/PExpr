#include "ClosureAnalyzer.h"
#include "ast/Expression.h"
#include "ast/Statement.h"

namespace PExpr::type {
using namespace ast;

ClosureAnalyzer::ClosureAnalyzer(utils::Reporter& reporter, bool captureUsage, bool captureModification)
    : mReporter(reporter)
    , mCaptureUsage(captureUsage)
    , mCaptureModification(captureModification)
{
}

void ClosureAnalyzer::analyzeClosure(const Ptr<Closure>& focusedClosure, const Ptr<Closure>& currentClosure,
                                     std::map<std::string, VariableDef>& outCapturedUsage,
                                     std::map<std::string, VariableDef>& outCapturedMutable)
{
    collectCapturesFromClosureBody(focusedClosure, currentClosure, outCapturedUsage, outCapturedMutable);
}

void ClosureAnalyzer::collectCapturesFromClosureBody(const Ptr<Closure>& focusedClosure, const Ptr<Closure>& currentClosure,
                                                     std::map<std::string, VariableDef>& outCapturedUsage,
                                                     std::map<std::string, VariableDef>& outCapturedMutable)
{
    // Now traverse statements expressions
    for (const auto& stmt : currentClosure->statements()) {
        // For variable decl/assign and function decl bodies, inspect expressions
        if (stmt->type() == StatementType::VariableDeclaration) {
            collectCapturesFromExpression(focusedClosure, currentClosure, std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt)->expression(), outCapturedUsage, outCapturedMutable);
        } else if (stmt->type() == StatementType::VariableAssignment) {
            const auto assignStmt = std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt);
            // Traverse the pattern to find all variables being assigned
            if (mCaptureModification)
                collectMutableAssignmentsFromPattern(focusedClosure, currentClosure, assignStmt->pattern(), outCapturedUsage, outCapturedMutable);
            collectCapturesFromExpression(focusedClosure, currentClosure, assignStmt->expression(), outCapturedUsage, outCapturedMutable);
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            const auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            if (!f->isExtern())
                collectCapturesFromClosureBody(focusedClosure, f->closure(), outCapturedUsage, outCapturedMutable);
        }
    }

    // Finally check the final expression
    if (currentClosure->expression())
        collectCapturesFromExpression(focusedClosure, currentClosure, currentClosure->expression(), outCapturedUsage, outCapturedMutable);
}

void ClosureAnalyzer::collectCapturesFromExpression(const Ptr<Closure>& focusedClosure, const Ptr<Closure>& currentClosure,
                                                    const Ptr<Expression>& expr,
                                                    std::map<std::string, VariableDef>& outCapturedUsage,
                                                    std::map<std::string, VariableDef>& outCapturedMutable)
{
    if (!expr)
        return;

    switch (expr->type()) {
    case ExpressionType::Variable: {
        const auto v           = std::reinterpret_pointer_cast<VariableExpression>(expr);
        const SymbolTable* tbl = nullptr;
        if (auto def = currentClosure->symbols().lookupVariable(v->location(), v->name(), &tbl); def.has_value()) {

            // Go up the ladder until we find the top focusedClosure or end up in global
            while (tbl && tbl != &focusedClosure->symbols())
                tbl = tbl->parent();

            if (!tbl) { //< captured (above the focusedClosure)
                if (mCaptureUsage)
                    outCapturedUsage.emplace(def->name(), def.value());
            }
        } else {
            mReporter.errorf(v->location(), "Unknown identifier '%s' found during uplift", v->name().c_str());
        }
    } break;
    case ExpressionType::Literal:
        break;
    case ExpressionType::Unary: {
        const auto u = std::reinterpret_pointer_cast<UnaryExpression>(expr);
        collectCapturesFromExpression(focusedClosure, currentClosure, u->inner(), outCapturedUsage, outCapturedMutable);
    } break;
    case ExpressionType::Binary: {
        const auto b = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        collectCapturesFromExpression(focusedClosure, currentClosure, b->left(), outCapturedUsage, outCapturedMutable);
        collectCapturesFromExpression(focusedClosure, currentClosure, b->right(), outCapturedUsage, outCapturedMutable);
    } break;
    case ExpressionType::Call: {
        const auto c = std::reinterpret_pointer_cast<CallExpression>(expr);
        for (const auto& p : c->parameters())
            collectCapturesFromExpression(focusedClosure, currentClosure, p, outCapturedUsage, outCapturedMutable);
    } break;
    case ExpressionType::Swizzle: {
        const auto a = std::reinterpret_pointer_cast<SwizzleExpression>(expr);
        collectCapturesFromExpression(focusedClosure, currentClosure, a->inner(), outCapturedUsage, outCapturedMutable);
    } break;
    case ExpressionType::Access: {
        const auto a = std::reinterpret_pointer_cast<AccessExpression>(expr);
        collectCapturesFromExpression(focusedClosure, currentClosure, a->inner(), outCapturedUsage, outCapturedMutable);
    } break;
    case ExpressionType::Cast: {
        const auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        collectCapturesFromExpression(focusedClosure, currentClosure, c->inner(), outCapturedUsage, outCapturedMutable);
    } break;
    case ExpressionType::Tuple: {
        const auto v = std::reinterpret_pointer_cast<TupleExpression>(expr);
        for (const auto& e : v->entries())
            collectCapturesFromExpression(focusedClosure, currentClosure, e, outCapturedUsage, outCapturedMutable);
    } break;
    case ExpressionType::Closure: {
        const auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        collectCapturesFromClosureBody(focusedClosure, c->closure(), outCapturedUsage, outCapturedMutable);
    } break;
    case ExpressionType::Branch: {
        const auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        collectCapturesFromExpression(focusedClosure, br->elseClosure(), br->elseClosure()->expression(), outCapturedUsage, outCapturedMutable);
        for (const auto& b : br->branches()) {
            collectCapturesFromExpression(focusedClosure, currentClosure, b.Condition, outCapturedUsage, outCapturedMutable);
            collectCapturesFromClosureBody(focusedClosure, b.Body, outCapturedUsage, outCapturedMutable);
        }
    } break;
    default:
        PEXPR_ASSERT(false, "Non exhaustive ExpressionType check in ClosureAnalyzer");
        break;
    }
}

void ClosureAnalyzer::collectMutableAssignmentsFromPattern(const Ptr<Closure>& focusedClosure, const Ptr<Closure>& currentClosure,
                                                           const Ptr<Pattern>& pattern,
                                                           std::map<std::string, VariableDef>& outCapturedUsage,
                                                           std::map<std::string, VariableDef>& outCapturedMutable)
{
    if (!pattern)
        return;

    for (const auto& elem : pattern->elements()) {
        if (elem.isSimpleBinding()) {
            const auto& binding    = elem.simpleBinding();
            const SymbolTable* tbl = nullptr;
            if (auto def = currentClosure->symbols().lookupVariable(elem.location(), binding.name, &tbl); def.has_value() && def->isMutable()) {
                // Go up the ladder until we find the top focusedClosure or end up in global
                while (tbl && tbl != &focusedClosure->symbols())
                    tbl = tbl->parent();

                if (!tbl) { //< captured (above the focusedClosure)
                    if (mCaptureUsage)
                        outCapturedUsage.emplace(def->name(), def.value());
                    if (mCaptureModification)
                        outCapturedMutable.emplace(def->name(), def.value());
                }
            } else {
                mReporter.errorf(elem.location(), "Unknown variable '%s' found during currentClosure analyzis", binding.name.c_str());
            }
        } else {
            // Recursively traverse nested patterns
            collectMutableAssignmentsFromPattern(focusedClosure, currentClosure, elem.nestedPattern(), outCapturedUsage, outCapturedMutable);
        }
    }
}

} // namespace PExpr::type