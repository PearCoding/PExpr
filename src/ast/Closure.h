#pragma once

#include "type/SymbolTable.h"

namespace PExpr::ast {
class Expression;
class Statement;
class Closure {
public:
    using StatementList = std::vector<Ptr<Statement>>;

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

    [[nodiscard]] inline const StatementList& statements() const { return mStatement; }
    [[nodiscard]] inline StatementList& statements() { return mStatement; }
    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }
    [[nodiscard]] inline Ptr<Expression>& expressionMut() & { return mExpression; }

    inline void addStatement(const Ptr<Statement>& statement)
    {
        PEXPR_ASSERT(statement != nullptr, "Expected valid statement");
        mStatement.push_back(statement);
    }

    inline void replaceStatement(const Ptr<Statement>& oldStmt, const Ptr<Statement>& newStmt)
    {
        PEXPR_ASSERT(oldStmt != nullptr, "Expected valid original statement");
        PEXPR_ASSERT(newStmt != nullptr, "Expected valid new statement");
        for (auto& st : mStatement) {
            if (st == oldStmt) {
                st = newStmt;
                break;
            }
        }
    }

    inline void replaceStatement(const Ptr<Statement>& oldStmt, Ptr<Statement>&& newStmt)
    {
        PEXPR_ASSERT(oldStmt != nullptr, "Expected valid original statement");
        PEXPR_ASSERT(newStmt != nullptr, "Expected valid new statement");
        for (auto& st : mStatement) {
            if (st == oldStmt) {
                st = std::move(newStmt);
            }
        }
    }

    // Internal usage
    [[nodiscard]] inline type::SymbolTable& symbols() { return mSymbols; }

private:
    Closure* mParent;
    type::SymbolTable mSymbols;

    parser::Location mLocation;
    StatementList mStatement;
    Ptr<Expression> mExpression;
};
} // namespace PExpr::ast
