#include "Closure.h"
#include "Expression.h"
#include "Statement.h"

namespace PExpr::ast {
bool Closure::hasFinalExpression() const
{
    if (mExpressions.empty())
        return false;

    if (mExpressions.back() == nullptr)
        return false;

    if (mExpressions.back()->isVoid() || mExpressions.back()->isError())
        return false;

    return true;
}
} // namespace PExpr::ast
