#pragma once

#include "Closure.h"
#include "Reporter.h"
#include "SymbolTable.h"

namespace PExpr::internal {
/// Checks types, resolves unspecified typing and resolves symbol lookups
class TypeChecker {
public:
    explicit TypeChecker(const SymbolTable& defs, Reporter& reporter);

    [[nodiscard]] ElementaryType handle(const Ptr<Closure>& closure);

private:
    [[nodiscard]] ElementaryType handleNode(const Ptr<Closure>& closure);
    void handleNode(const Ptr<Statement>& statement);

    [[nodiscard]] ElementaryType handleNode(const Ptr<Expression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<ClosureExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<BranchExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<VariableExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<LiteralExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<UnaryExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<BinaryExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<CallExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<AccessExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<CastExpression>& expr);
    [[nodiscard]] ElementaryType handleNode(const Ptr<VectorExpression>& expr);

    SymbolTable mDynamicDefinitions;
    const SymbolTable& mDefinitions;
    Reporter& mReporter;
};
} // namespace PExpr::internal
