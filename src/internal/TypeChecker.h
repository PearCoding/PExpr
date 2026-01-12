#pragma once

#include "Closure.h"
#include "Expression.h"
#include "Reporter.h"
#include "Statement.h"
#include "SymbolTable.h"

#include <unordered_set>

namespace PExpr::internal {
/// Checks types, resolves unspecified typing and resolves symbol lookups
class TypeChecker {
public:
    explicit TypeChecker(Reporter& reporter);

    [[nodiscard]] ElementaryType handle(const Ptr<Closure>& closure);

private:
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure);
    void handleNode(const Ptr<Closure>& closure, const Ptr<Statement>& statement);

    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure, const Ptr<Expression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure, const Ptr<ClosureExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure, const Ptr<BranchExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure, const Ptr<VariableExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure, const Ptr<LiteralExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure, const Ptr<UnaryExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure, const Ptr<BinaryExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure, const Ptr<CallExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure, const Ptr<AccessExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure, const Ptr<CastExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure, const Ptr<VectorExpression>& expr);

    Reporter& mReporter;
};
} // namespace PExpr::internal
