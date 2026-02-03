#pragma once

#include "type/SymbolTable.h"

#include <functional>
#include <ranges>

namespace PExpr::ast {
class Expression;
class Closure {
public:
    using ExpressionList = std::vector<Ptr<Expression>>;

    explicit Closure(const parser::Location& loc, Closure* parent = nullptr)
        : mParent(parent)
        , mSymbols(type::SymbolTable::Connect(parent ? &parent->symbols() : nullptr))
        , mLocation(loc)
    {
    }

    /// The location this expression is associated with.
    [[nodiscard]] inline const parser::Location& location() const { return mLocation; }

    [[nodiscard]] inline bool isTranslationUnit() const { return mParent == nullptr; }
    [[nodiscard]] inline Closure* parent() const { return mParent; }
    inline void setParent(Closure* p)
    {
        mParent = p;
        mSymbols.setParent(p ? &p->symbols() : nullptr);
    }

    [[nodiscard]] inline const ExpressionList& expressions() const { return mExpressions; }
    [[nodiscard]] inline ExpressionList& expressions() { return mExpressions; }

    [[nodiscard]] inline auto expressionsWithoutFinal() const
    {
        if (hasFinalExpression())
            return std::ranges::subrange(mExpressions.begin(), mExpressions.end() - 1);
        else
            return std::ranges::subrange(mExpressions.begin(), mExpressions.end());
    }

    [[nodiscard]] bool hasFinalExpression() const;

    [[nodiscard]] inline Ptr<Expression> finalExpression() const
    {
        return mExpressions.back();
    }
    [[nodiscard]] inline Ptr<Expression>& finalExpressionMut() &
    {
        return mExpressions.back();
    }

    inline void addExpression(const Ptr<Expression>& statement)
    {
        PEXPR_ASSERT(statement != nullptr, "Expected valid statement");
        mExpressions.push_back(statement);
    }

    inline void replaceExpression(const Ptr<Expression>& oldStmt, const Ptr<Expression>& newStmt)
    {
        PEXPR_ASSERT(oldStmt != nullptr, "Expected valid original statement");
        PEXPR_ASSERT(newStmt != nullptr, "Expected valid new statement");
        for (auto& st : mExpressions) {
            if (st == oldStmt) {
                st = newStmt;
                break;
            }
        }
    }

    inline void replaceExpression(const Ptr<Expression>& oldStmt, Ptr<Expression>&& newStmt)
    {
        PEXPR_ASSERT(oldStmt != nullptr, "Expected valid original statement");
        PEXPR_ASSERT(newStmt != nullptr, "Expected valid new statement");
        for (auto& st : mExpressions) {
            if (st == oldStmt)
                st = std::move(newStmt);
        }
    }

    inline void replaceExpression(Expression* oldStmt, const Ptr<Expression>& newStmt)
    {
        PEXPR_ASSERT(oldStmt != nullptr, "Expected valid original statement");
        PEXPR_ASSERT(newStmt != nullptr, "Expected valid new statement");
        for (auto& st : mExpressions) {
            if (st.get() == oldStmt) {
                st = newStmt;
                break;
            }
        }
    }

    inline void replaceExpression(Expression* oldStmt, Ptr<Expression>&& newStmt)
    {
        PEXPR_ASSERT(oldStmt != nullptr, "Expected valid original statement");
        PEXPR_ASSERT(newStmt != nullptr, "Expected valid new statement");
        for (auto& st : mExpressions) {
            if (st.get() == oldStmt)
                st = std::move(newStmt);
        }
    }

    // Internal usage
    [[nodiscard]] inline const type::SymbolTable& symbols() const { return mSymbols; }
    [[nodiscard]] inline type::SymbolTable& symbols() { return mSymbols; }

private:
    Closure* mParent;
    type::SymbolTable mSymbols;

    parser::Location mLocation;
    ExpressionList mExpressions;
};
} // namespace PExpr::ast
