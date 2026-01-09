#pragma once

#include "Expression.h"
#include "Parameter.h"

namespace PExpr {

class Statement {
    friend internal::TypeChecker;
    friend class Environment;

public:
    Statement() = delete;

    /// The location this expression is associated with.
    [[nodiscard]] inline const Location& location() const { return mLocation; }

    [[nodiscard]] inline const std::string& name() const { return mName; }
    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }

    /// Replace the expression owned by this statement. Used by the TypeChecker to
    /// inject implicit CastExpression nodes for assignments and other contexts.
    inline void replaceExpression(const Ptr<Expression>& expr)
    {
        PEXPR_ASSERT(expr != nullptr, "Expected valid expression");
        mExpression = expr;
    }

    [[nodiscard]] inline StatementType type() const { return mType; }

protected:
    Statement(const Location& loc, const std::string& name, const Ptr<Expression>& expression, StatementType type)
        : mLocation(loc)
        , mName(name)
        , mExpression(expression)
        , mType(type)
    {
    }

private:
    const Location mLocation;
    const std::string mName;
    Ptr<Expression> mExpression;
    const StatementType mType;
};

class VariableDeclarationStatement : public Statement {
public:
    // declaredType may be ElementaryType::Unspecified when no explicit type is given.
    VariableDeclarationStatement(bool mutable_, const Location& loc, const std::string& name, const Ptr<Expression>& expression, ElementaryType declaredType = ElementaryType::Unspecified)
        : Statement(loc, name, expression, StatementType::VariableDeclaration)
        , mIsMutable(mutable_)
        , mDeclaredType(declaredType)
    {
    }

    [[nodiscard]] inline bool isMutable() const { return mIsMutable; }
    [[nodiscard]] inline ElementaryType declaredType() const { return mDeclaredType; }

private:
    const bool mIsMutable;
    const ElementaryType mDeclaredType;
};

class VariableAssignmentStatement : public Statement {
public:
    VariableAssignmentStatement(const Location& loc, const std::string& name, const Ptr<Expression>& expression)
        : Statement(loc, name, expression, StatementType::VariableAssignment)
    {
    }
};

class FunctionDeclarationStatement : public Statement {
public:
    FunctionDeclarationStatement(const Location& loc, const std::string& name, const ParameterList& parameters, const Ptr<Expression>& expression, ElementaryType returnType, const std::string& mangledName)
        : Statement(loc, name, expression, StatementType::FunctionDeclaration)
        , mParameters(parameters)
        , mReturnType(returnType)
        , mMangledName(mangledName)
    {
    }

    [[nodiscard]] inline const std::string& mangledName() const { return mMangledName; }

    inline const ParameterList& parameters() const { return mParameters; }
    [[nodiscard]] inline bool isExtern() const { return this->expression() == nullptr; }

    /// Return the actual return type if unspecified or the specified version
    /// The return type must be specified if the function is declared extern
    [[nodiscard]] inline ElementaryType returnType() const
    {
        if (mReturnType == ElementaryType::Unspecified && !isExtern())
            return this->expression()->returnType();
        else
            return mReturnType;
    }

private:
    const ParameterList mParameters;
    const ElementaryType mReturnType;
    const std::string mMangledName;
};
} // namespace PExpr
