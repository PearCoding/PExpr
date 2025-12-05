#pragma once

#include "Statement.h"
#include "internal/SymbolTable.h"

namespace PExpr {
class Closure {
public:
    using StatementList = std::vector<Ptr<Statement>>;

    explicit Closure(const Location& loc, Closure* parent = nullptr)
        : mParent(parent)
        , mSymbols(parent ? &parent->mSymbols : nullptr)
        , mLocation(loc)
    {
    }

    /// The location this expression is associated with.
    [[nodiscard]] inline const Location& location() const { return mLocation; }

    [[nodiscard]] inline bool isTranslationUnit() const { return mParent == nullptr; }
    [[nodiscard]] inline Closure* parent() const { return mParent; }

    [[nodiscard]] inline const StatementList& statements() const { return mStatement; }
    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }

    inline void addStatement(const Ptr<Statement>& statement)
    {
        PEXPR_ASSERT(statement != nullptr, "Expected valid statement");
        mStatement.push_back(statement);
    }

    inline void setExpression(const Ptr<Expression>& expr)
    {
        PEXPR_ASSERT(expr != nullptr, "Expected valid expression");
        PEXPR_ASSERT(mExpression == nullptr, "Expected closure expression to be set once");
        mExpression = expr;
    }

    /// Replace the existing expression inside the closure. This is used by the
    /// TypeChecker to inject CastExpression wrappers for implicit conversions.
    inline void replaceExpression(const Ptr<Expression>& expr)
    {
        PEXPR_ASSERT(expr != nullptr, "Expected valid expression");
        mExpression = expr;
    }

    // Internal usage
    [[nodiscard]] inline internal::SymbolTable& symbols() { return mSymbols; }

private:
    Closure* mParent;
    internal::SymbolTable mSymbols;

    Location mLocation;
    StatementList mStatement;
    Ptr<Expression> mExpression;
};
} // namespace PExpr
