#pragma once

#include "Closure.h"
#include "Expression.h"
#include "Pattern.h"
#include "type/Parameter.h"
#include "type/Type.h"

namespace PExpr::ast {

class Statement {
public:
    Statement() = delete;

    /// The location this statement is associated with.
    [[nodiscard]] inline const parser::Location& location() const { return mLocation; }
    [[nodiscard]] inline const std::string& name() const { return mName; }
    [[nodiscard]] inline StatementType type() const { return mType; }

protected:
    Statement(const parser::Location& loc, const std::string& name, StatementType type)
        : mLocation(loc)
        , mName(name)
        , mType(type)
    {
    }

private:
    const parser::Location mLocation;
    const std::string mName;
    const StatementType mType;
};

class VariableDeclarationStatement : public Statement {
public:
    // declaredType may be TypeKind::Unspecified when no explicit type is given.
    VariableDeclarationStatement(bool mutable_, const parser::Location& loc, const std::string& name, const Ptr<Expression>& expr, const type::Type& declaredType = type::Type(type::TypeKind::Unspecified))
        : Statement(loc, name, StatementType::VariableDeclaration)
        , mIsMutable(mutable_)
        , mDeclaredType(declaredType)
        , mExpression(expr)
    {
    }

    [[nodiscard]] inline bool isMutable() const { return mIsMutable; }
    [[nodiscard]] inline const type::Type& declaredType() const { return mDeclaredType; }
    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }
    inline void replaceExpression(const Ptr<Expression>& expr) { mExpression = expr; }

private:
    const bool mIsMutable;
    const type::Type mDeclaredType;
    Ptr<Expression> mExpression;
};

class VariableAssignmentStatement : public Statement {
public:
    VariableAssignmentStatement(const parser::Location& loc, const std::string& name, const Ptr<Expression>& expr)
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
    FunctionDeclarationStatement(const parser::Location& loc, const std::string& name, const type::ParameterList& parameters, const Ptr<Closure>& closure, const type::Type& returnType, const std::string& mangledName, bool hasSideEffects)
        : Statement(loc, name, StatementType::FunctionDeclaration)
        , mParameters(parameters)
        , mReturnType(returnType)
        , mMangledName(mangledName)
        , mClosure(closure)
        , mHasSideEffects(hasSideEffects)
    {
    }

    [[nodiscard]] inline const std::string& mangledName() const { return mMangledName; }

    [[nodiscard]] inline const type::ParameterList& parameters() const { return mParameters; }
    [[nodiscard]] inline bool isExtern() const { return mClosure == nullptr; }
    [[nodiscard]] inline bool hasSideEffects() const { return mHasSideEffects; }
    [[nodiscard]] inline Ptr<Closure> closure() const { return mClosure; }

    [[nodiscard]] inline const type::Type& returnType() const { return mReturnType; }
    inline void setReturnType(const type::Type& type) { mReturnType = type; }
    [[nodiscard]] inline bool isUnspecified() const { return mReturnType.kind() == type::TypeKind::Unspecified; }

private:
    const type::ParameterList mParameters;
    type::Type mReturnType;
    const std::string mMangledName;
    const Ptr<Closure> mClosure;
    const bool mHasSideEffects;
};

class TypeAliasStatement : public Statement {
public:
    TypeAliasStatement(const parser::Location& loc, const std::string& name, const type::Type& aliasedType)
        : Statement(loc, name, StatementType::TypeAlias)
        , mAliasedType(aliasedType)
    {
    }

    [[nodiscard]] inline const type::Type& aliasedType() const { return mAliasedType; }

private:
    const type::Type mAliasedType;
};

/// Declaration with destructuring pattern like "let *[a:vec2, b, mut c:num] = [[2,4], true, 2.0];"
class DestructuringDeclarationStatement : public Statement {
public:
    DestructuringDeclarationStatement(const parser::Location& loc, const Ptr<Pattern>& pattern, const Ptr<Expression>& expr)
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
    DestructuringAssignmentStatement(const parser::Location& loc, const Ptr<Pattern>& pattern, const Ptr<Expression>& expr)
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

} // namespace PExpr::ast
