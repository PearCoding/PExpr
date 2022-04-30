#pragma once

#include "Expression.h"
#include "VariableContainer.h"

namespace PExpr {
class TypeChecker {
public:
    explicit TypeChecker(const VariableContainer& variables);

    ElementaryType handle(const Ptr<Expression>& expr);

private:
    ElementaryType handleNode(const Ptr<VariableExpression>& expr);
    ElementaryType handleNode(const Ptr<ConstExpression>& expr);
    ElementaryType handleNode(const Ptr<UnaryExpression>& expr);
    ElementaryType handleNode(const Ptr<BinaryExpression>& expr);
    ElementaryType handleNode(const Ptr<CallExpression>& expr);
    ElementaryType handleNode(const Ptr<AccessExpression>& expr);

    const VariableContainer& mVariables;
};
} // namespace PExpr