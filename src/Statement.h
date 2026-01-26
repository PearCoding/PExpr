#pragma once

#include "Closure.h"
#include "Expression.h"
#include "Parameter.h"
#include "Pattern.h"
#include "Type.h"

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
    // declaredType may be TypeKind::Unspecified when no explicit type is given.
    VariableDeclarationStatement(bool mutable_, const Location& loc, const std::string& name, const Ptr<Expression>& expr, const Type& declaredType = Type(TypeKind::Unspecified))
        : Statement(loc, name, StatementType::VariableDeclaration)
        , mIsMutable(mutable_)
        , mDeclaredType(declaredType)
        , mExpression(expr)
    {
    }

    [[nodiscard]] inline bool isMutable() const { return mIsMutable; }
    [[nodiscard]] inline const Type& declaredType() const { return mDeclaredType; }
    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }
    inline void replaceExpression(const Ptr<Expression>& expr) { mExpression = expr; }

private:
    const bool mIsMutable;
    const Type mDeclaredType;
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
    FunctionDeclarationStatement(const Location& loc, const std::string& name, const ParameterList& parameters, const Ptr<Closure>& closure, const Type& returnType, const std::string& mangledName, bool hasSideEffects)
        : Statement(loc, name, StatementType::FunctionDeclaration)
        , mParameters(parameters)
        , mReturnType(returnType)
        , mMangledName(mangledName)
        , mClosure(closure)
        , mHasSideEffects(hasSideEffects)
    {
    }

    [[nodiscard]] inline const std::string& mangledName() const { return mMangledName; }

    [[nodiscard]] inline const ParameterList& parameters() const { return mParameters; }
    [[nodiscard]] inline bool isExtern() const { return mClosure == nullptr; }
    [[nodiscard]] inline bool hasSideEffects() const { return mHasSideEffects; }
    [[nodiscard]] inline Ptr<Closure> closure() const { return mClosure; }

    [[nodiscard]] inline const Type& returnType() const { return mReturnType; }
    inline void setReturnType(const Type& type) { mReturnType = type; }
    [[nodiscard]] inline bool isUnspecified() const { return mReturnType.kind() == TypeKind::Unspecified; }

private:
    const ParameterList mParameters;
    Type mReturnType;
    const std::string mMangledName;
    const Ptr<Closure> mClosure;
    const bool mHasSideEffects;
};

class TypeAliasStatement : public Statement {
public:
    TypeAliasStatement(const Location& loc, const std::string& name, const Type& aliasedType)
        : Statement(loc, name, StatementType::TypeAlias)
        , mAliasedType(aliasedType)
    {
    }

    [[nodiscard]] inline const Type& aliasedType() const { return mAliasedType; }

private:
    const Type mAliasedType;
};

/// Declaration with destructuring pattern like "let *[a:vec2, b, mut c:num] = [[2,4], true, 2.0];"
class DestructuringDeclarationStatement : public Statement {
public:
    DestructuringDeclarationStatement(const Location& loc, const Ptr<Pattern>& pattern, const Ptr<Expression>& expr)
        : Statement(loc, "", StatementType::DestructuringDeclaration)
        , mPattern(pattern)
        , mExpression(expr)
    {
    }

    [[nodiscard]] inline const Ptr<Pattern>& pattern() const { return mPattern; }
    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }
    inline void replaceExpression(const Ptr<Expression>& expr) { mExpression = expr; }

private:
    Ptr<Pattern> mPattern;
    Ptr<Expression> mExpression;
};

/// Assignment with destructuring pattern like "*[c, d] = foo()"
class DestructuringAssignmentStatement : public Statement {
public:
    DestructuringAssignmentStatement(const Location& loc, const Ptr<Pattern>& pattern, const Ptr<Expression>& expr)
        : Statement(loc, "", StatementType::DestructuringAssignment)
        , mPattern(pattern)
        , mExpression(expr)
    {
    }

    [[nodiscard]] inline const Ptr<Pattern>& pattern() const { return mPattern; }
    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }
    inline void replaceExpression(const Ptr<Expression>& expr) { mExpression = expr; }

private:
    Ptr<Pattern> mPattern;
    Ptr<Expression> mExpression;
};

} // namespace PExpr
