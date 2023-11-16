#pragma once

#include "../Closure.h"
#include "SymbolTable.h"

namespace PExpr::internal {
/// @brief Checks types and resolves symbol lookups
class TypeChecker {
public:
    explicit TypeChecker(const SymbolTable& defs);

    ElementaryType handle(const Ptr<Closure>& closure);

private:
    ElementaryType handleNode(const Ptr<Closure>& closure);
    ElementaryType handleNode(const Ptr<Statement>& statement);
    ElementaryType handleNode(const Ptr<Expression>& expr);
    ElementaryType handleNode(const Ptr<ClosureExpression>& expr);
    ElementaryType handleNode(const Ptr<BranchExpression>& expr);

    ElementaryType handleNode(const Ptr<VariableExpression>& expr);
    ElementaryType handleNode(const Ptr<LiteralExpression>& expr);
    ElementaryType handleNode(const Ptr<UnaryExpression>& expr);
    ElementaryType handleNode(const Ptr<BinaryExpression>& expr);
    ElementaryType handleNode(const Ptr<CallExpression>& expr);
    ElementaryType handleNode(const Ptr<AccessExpression>& expr);

    SymbolTable mDynamicDefinitions;
    const SymbolTable& mDefinitions;
};
} // namespace PExpr::internal