#include "Expression.h"
#include "Closure.h"

namespace PExpr::ast {

void ClosureExpression::forEachExpression(const std::function<void(const Expression*)>& visitor, bool recursive) const
{
    if (recursive)
        mClosure->forEachExpression(visitor, recursive);
}

void BranchExpression::forEachExpression(const std::function<void(const Expression*)>& visitor, bool recursive) const
{
    for (const auto& b : mBranches)
        visitor(b.Condition.get());

    if (!recursive)
        return;

    for (const auto& b : mBranches)
        b.Body->forEachExpression(visitor, recursive);
    mElseClosure->forEachExpression(visitor, recursive);
}

} // namespace PExpr::ast
