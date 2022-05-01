#pragma once

#include "DefContainer.h"
#include "../Expression.h"

namespace PExpr::internal {
class TypeChecker {
public:
    explicit TypeChecker(const DefContainer& defs);

    ElementaryType handle(const Ptr<Expression>& expr);

private:
    ElementaryType handleNode(const Ptr<VariableExpression>& expr);
    ElementaryType handleNode(const Ptr<LiteralExpression>& expr);
    ElementaryType handleNode(const Ptr<UnaryExpression>& expr);
    ElementaryType handleNode(const Ptr<BinaryExpression>& expr);
    ElementaryType handleNode(const Ptr<CallExpression>& expr);
    ElementaryType handleNode(const Ptr<AccessExpression>& expr);

    const DefContainer& mDefinitions;
};
} // namespace PExpr