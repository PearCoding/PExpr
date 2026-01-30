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
        if (expr->type() != ExpressionType::Variable)
            return;

        const auto v           = dynamic_cast<const VariableExpression*>(expr);
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

    auto captureMutable = [&](const Closure* closure, const Statement* stmt) {
        if (stmt->type() == StatementType::VariableAssignment) {
            const auto assignStmt = dynamic_cast<const VariableAssignmentStatement*>(stmt);
            collectMutableAssignmentsFromPattern(focusedClosure, closure, assignStmt->pattern(), outCapturedUsage, outCapturedMutable);
        }
    };
    if (mCaptureModification)
        Visitor::forEachStatement(focusedClosure, captureMutable);
}

void ClosureAnalyzer::collectMutableAssignmentsFromPattern(const Closure* focusedClosure, const Closure* currentClosure,
                                                           const Ptr<Pattern>& pattern,
                                                           std::map<std::string, Ptr<VariableDef>>& outCapturedUsage,
                                                           std::map<std::string, Ptr<VariableDef>>& outCapturedMutable)
{
    if (!pattern)
        return;

    for (const auto& elem : pattern->elements()) {
        if (elem.isSimpleBinding()) {
            const auto binding     = elem.simpleBinding();
            const SymbolTable* tbl = nullptr;
            if (auto def = currentClosure->symbols().lookupVariable(elem.location(), binding->name(), &tbl); def && def->isMutable()) {
                // Go up the ladder until we find the top focusedClosure or end up in global
                while (tbl && tbl != &focusedClosure->symbols())
                    tbl = tbl->parent();

                if (!tbl) { //< captured (above the focusedClosure)
                    if (mCaptureUsage)
                        outCapturedUsage.emplace(binding->name(), binding);
                    if (mCaptureModification)
                        outCapturedMutable.emplace(binding->name(), binding);
                }
            } else {
                mReporter.errorf(elem.location(), "Unknown variable '%s' found during capture mutable detection", binding->name().c_str());
            }
        } else {
            // Recursively traverse nested patterns
            collectMutableAssignmentsFromPattern(focusedClosure, currentClosure, elem.nestedPattern(), outCapturedUsage, outCapturedMutable);
        }
    }
}

} // namespace PExpr::type