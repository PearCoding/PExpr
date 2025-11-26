#pragma once

#include "../Closure.h"
#include "SymbolTable.h"

namespace PExpr::internal {
/// @brief Checks types and resolves symbol lookups
class TypeChecker {
public:
    explicit TypeChecker(const SymbolTable& defs);

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

    SymbolTable mDynamicDefinitions;
    const SymbolTable& mDefinitions;
};
} // namespace PExpr::internal