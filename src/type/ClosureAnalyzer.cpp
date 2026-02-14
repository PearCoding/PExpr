#include "ClosureAnalyzer.h"
#include "ast/Expression.h"
#include "ast/Statement.h"
#include "ast/Visitor.h"

namespace PExpr::type {
using namespace ast;

ClosureAnalyzer::ClosureAnalyzer(utils::Reporter& reporter, bool captureUsage, bool captureModification)
    : mReporter(reporter)
    , mCaptureUsage(captureUsage)
    , mCaptureModification(captureModification)
{
}

void ClosureAnalyzer::analyzeClosure(const Closure* focusedClosure,
                                     std::map<std::string, Ptr<VariableDef>>& outCapturedUsage,
                                     std::map<std::string, Ptr<VariableDef>>& outCapturedMutable)
{
    auto captureUsage = [&](const Closure* closure, const Expression* expr) {
        const auto v = dynamic_cast<const VariableExpression*>(expr);
        if (!v)
            return;

        const SymbolTable* tbl = nullptr;
        if (auto def = closure->symbols().lookupVariable(v->location(), v->variable()->name(), &tbl)) {
            // Go up the ladder until we find the top focusedClosure or end up in global
            while (tbl && tbl != &focusedClosure->symbols())
                tbl = tbl->parent();

            if (!tbl) //< captured (above the focusedClosure)
                outCapturedUsage.emplace(def->name(), def);
        } else {
            mReporter.errorf(v->location(), "Unknown identifier '%s' found during capture usage detection", v->variable()->name().c_str());
        }
    };
    if (mCaptureUsage)
        Visitor::forEachExpression(focusedClosure, captureUsage);

    auto captureMutable = [&](const Closure* closure, const Expression* expr) {
        // Handle destructuring assignment expressions like [a, b] = [b, a]
        if (const auto assignExpr = dynamic_cast<const AssignmentExpression*>(expr))
            collectMutableAssignmentsFromExpression(focusedClosure, closure, assignExpr->lvalue(), outCapturedUsage, outCapturedMutable);
    };
    if (mCaptureModification)
        Visitor::forEachExpression(focusedClosure, captureMutable);
}

void ClosureAnalyzer::collectMutableAssignmentsFromExpression(const Closure* focusedClosure, const Closure* currentClosure,
                                                              const Ptr<Expression>& expr,
                                                              std::map<std::string, Ptr<VariableDef>>& outCapturedUsage,
                                                              std::map<std::string, Ptr<VariableDef>>& outCapturedMutable)
{
    if (!expr)
        return;

    if (const auto tupleExpr = std::dynamic_pointer_cast<const TupleExpression>(expr)) { //< Handle tuple expressions like [a, b]
        for (const auto& entry : tupleExpr->entries())
            collectMutableAssignmentsFromExpression(focusedClosure, currentClosure, entry, outCapturedUsage, outCapturedMutable);
    } else if (const auto varExpr = std::dynamic_pointer_cast<const VariableExpression>(expr)) { //< Handle variable expressions like a or b
        const SymbolTable* tbl = nullptr;
        if (auto def = currentClosure->symbols().lookupVariable(varExpr->location(), varExpr->variable()->name(), &tbl); def && def->isMutable()) {
            // Go up the ladder until we find the top focusedClosure or end up in global
            while (tbl && tbl != &focusedClosure->symbols())
                tbl = tbl->parent();

            if (!tbl) { //< captured (above the focusedClosure)
                if (mCaptureUsage)
                    outCapturedUsage.emplace(def->name(), def);
                if (mCaptureModification)
                    outCapturedMutable.emplace(def->name(), def);
            }
        } else {
            mReporter.errorf(varExpr->location(), "Unknown variable '%s' found during capture mutable detection", varExpr->variable()->name().c_str());
        }
    }
    // Other expression types don't represent mutable assignments
}

} // namespace PExpr::type
