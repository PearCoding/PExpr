#pragma once

#include "Closure.h"
#include "Expression.h"
#include "Pattern.h"
#include "type/Type.h"

namespace PExpr::ast {
/// Unified declaration statement for both single variable and destructuring patterns
/// Example: "let a:num = 5;" or "let *[a:vec2, b, mut c:num] = [[2,4], true, 2.0];"
class VariableDeclarationStatement : public Expression {
public:
    VariableDeclarationStatement(const parser::Location& loc, const Ptr<Pattern>& pattern, const Ptr<Expression>& expr)
        : Expression(loc, ExpressionType::VariableDeclaration)
        , mPattern(pattern)
        , mExpression(expr)
    {
        setReturnType(type::Type::Void());
    }

    [[nodiscard]] inline Ptr<Pattern> pattern() const { return mPattern; }
    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }
    [[nodiscard]] inline Ptr<Expression>& expressionMut() & { return mExpression; }

private:
    Ptr<Pattern> mPattern;
    Ptr<Expression> mExpression;
};

/// Unified assignment statement for both single variable and destructuring patterns
/// Example: "a = 5;" or "*[c, d] = foo();"
class VariableAssignmentStatement : public Expression {
public:
    VariableAssignmentStatement(const parser::Location& loc, const Ptr<Pattern>& pattern, const Ptr<Expression>& expr)
        : Expression(loc, ExpressionType::VariableAssignment)
        , mPattern(pattern)
        , mExpression(expr)
    {
        setReturnType(type::Type::Void());
    }

    [[nodiscard]] inline Ptr<Pattern> pattern() const { return mPattern; }
    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }
    [[nodiscard]] inline Ptr<Expression>& expressionMut() & { return mExpression; }

private:
    Ptr<Pattern> mPattern;
    Ptr<Expression> mExpression;
};

class FunctionDeclarationStatement : public Expression {
public:
    FunctionDeclarationStatement(const parser::Location& loc, const std::string& name, const type::ParameterList& parameters,
                                 const Ptr<Closure>& closure, const type::Type& returnType, const std::string& mangledName, bool hasSideEffects)
        : Expression(loc, ExpressionType::FunctionDeclaration)
        , mParameters(parameters)
        , mFunctionReturnType(returnType)
        , mName(name)
        , mMangledName(mangledName)
        , mClosure(closure)
        , mHasSideEffects(hasSideEffects)
    {
        setReturnType(type::Type::Void());
    }

    [[nodiscard]] inline const std::string& name() const { return mName; }
    [[nodiscard]] inline const std::string& mangledName() const { return mMangledName; }

    [[nodiscard]] inline const type::ParameterList& parameters() const { return mParameters; }
    [[nodiscard]] inline bool isExtern() const { return mClosure == nullptr; }
    [[nodiscard]] inline bool hasSideEffects() const { return mHasSideEffects; }
    [[nodiscard]] inline Ptr<Closure> closure() const { return mClosure; }

    [[nodiscard]] inline const type::Type& functionReturnType() const { return mFunctionReturnType; }
    inline void setFunctionReturnType(const type::Type& type) { mFunctionReturnType = type; }
    [[nodiscard]] inline bool isUnspecified() const { return !mFunctionReturnType.isSpecified(); }

private:
    const type::ParameterList mParameters;
    type::Type mFunctionReturnType;
    const std::string mName;
    const std::string mMangledName;
    const Ptr<Closure> mClosure;
    const bool mHasSideEffects;
};

class TypeAliasStatement : public Expression {
public:
    TypeAliasStatement(const parser::Location& loc, const std::string& name, const type::Type& aliasedType)
        : Expression(loc, ExpressionType::TypeAlias)
        , mName(name)
        , mAliasedType(aliasedType)
    {
        setReturnType(type::Type::Void());
    }

    [[nodiscard]] inline const std::string& name() const { return mName; }
    [[nodiscard]] inline const type::Type& aliasedType() const { return mAliasedType; }

private:
    const std::string mName;
    const type::Type mAliasedType;
};

} // namespace PExpr::ast