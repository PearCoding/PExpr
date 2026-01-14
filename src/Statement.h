#pragma once

#include "Closure.h"
#include "Expression.h"
#include "Parameter.h"

namespace PExpr {

class Statement {
    friend internal::TypeChecker;
    friend class Environment;

public:
    Statement() = delete;

    /// The location this statement is associated with.
    [[nodiscard]] inline const Location& location() const { return mLocation; }
    [[nodiscard]] inline const std::string& name() const { return mName; }
    [[nodiscard]] inline StatementType type() const { return mType; }

protected:
    Statement(const Location& loc, const std::string& name, StatementType type)
        : mLocation(loc)
        , mName(name)
        , mType(type)
    {
    }

private:
    const Location mLocation;
    const std::string mName;
    const StatementType mType;
};

class VariableDeclarationStatement : public Statement {
public:
    // declaredType may be ElementaryType::Unspecified when no explicit type is given.
    VariableDeclarationStatement(bool mutable_, const Location& loc, const std::string& name, const Ptr<Expression>& expr, ElementaryType declaredType = ElementaryType::Unspecified)
        : Statement(loc, name, StatementType::VariableDeclaration)
        , mIsMutable(mutable_)
        , mDeclaredType(declaredType)
        , mExpression(expr)
    {
    }

    [[nodiscard]] inline bool isMutable() const { return mIsMutable; }
    [[nodiscard]] inline ElementaryType declaredType() const { return mDeclaredType; }
    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }
    inline void replaceExpression(const Ptr<Expression>& expr) { mExpression = expr; }

private:
    const bool mIsMutable;
    const ElementaryType mDeclaredType;
    Ptr<Expression> mExpression;
};

class VariableAssignmentStatement : public Statement {
public:
    VariableAssignmentStatement(const Location& loc, const std::string& name, const Ptr<Expression>& expr)
        : Statement(loc, name, StatementType::VariableAssignment)
        , mExpression(expr)
    {
    }

    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }
    inline void replaceExpression(const Ptr<Expression>& expr) { mExpression = expr; }

private:
    Ptr<Expression> mExpression;
};

class FunctionDeclarationStatement : public Statement {
public:
    FunctionDeclarationStatement(const Location& loc, const std::string& name, const ParameterList& parameters, const Ptr<Closure>& closure, ElementaryType returnType, const std::string& mangledName)
        : Statement(loc, name, StatementType::FunctionDeclaration)
        , mParameters(parameters)
        , mReturnType(returnType)
        , mMangledName(mangledName)
        , mClosure(closure)
    {
    }

    [[nodiscard]] inline const std::string& mangledName() const { return mMangledName; }

    [[nodiscard]] inline const ParameterList& parameters() const { return mParameters; }
    [[nodiscard]] inline bool isExtern() const { return mClosure == nullptr; }
    [[nodiscard]] inline Ptr<Closure> closure() const { return mClosure; }

    [[nodiscard]] inline ElementaryType returnType() const { return mReturnType; }
    inline void setReturnType(ElementaryType type) { mReturnType = type; }
    [[nodiscard]] inline bool isUnspecified() const { return mReturnType == ElementaryType::Unspecified; }

private:
    const ParameterList mParameters;
    ElementaryType mReturnType;
    const std::string mMangledName;
    const Ptr<Closure> mClosure;
};
} // namespace PExpr
