#include "Closure.h"
#include "Expression.h"
#include "Statement.h"

namespace PExpr::ast {
void Closure::forEachExpression(const std::function<void(const Expression*)>& visitor, bool recursive) const
{
    visitor(mExpression.get());
    if (recursive) {
        for (const auto& stmt : mStatement)
            stmt->forEachExpression(visitor, recursive);

        mExpression->forEachExpression(visitor, recursive);
    }
};
} // namespace PExpr::ast
