#pragma once

#include "Closure.h"
#include "Expression.h"
#include "Pattern.h"
#include "type/Type.h"

namespace PExpr::ast {

class Statement {
public:
    Statement() = delete;

    /// The location this statement is associated with.
    [[nodiscard]] inline const parser::Location& location() const { return mLocation; }
    [[nodiscard]] inline StatementType type() const { return mType; }

protected:
    Statement(const parser::Location& loc, StatementType type)
        : mLocation(loc)
        , mType(type)
    {
    }

private:
    const parser::Location mLocation;
    const StatementType mType;
};

/// Internal statement created when an unrecoverable error occured
class ErrorStatement : public Statement {
public:
    ErrorStatement(const parser::Location& loc)
        : Statement(loc, StatementType::Error)
    {
    }
};

/// Unified declaration statement for both single variable and destructuring patterns
/// Example: "let a:num = 5;" or "let *[a:vec2, b, mut c:num] = [[2,4], true, 2.0];"
class VariableDeclarationStatement : public Statement {
public:
    VariableDeclarationStatement(const parser::Location& loc, const Ptr<Pattern>& pattern, const Ptr<Expression>& expr)
        : Statement(loc, StatementType::VariableDeclaration)
        , mPattern(pattern)
        , mExpression(expr)
    {
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
class VariableAssignmentStatement : public Statement {
public:
    VariableAssignmentStatement(const parser::Location& loc, const Ptr<Pattern>& pattern, const Ptr<Expression>& expr)
        : Statement(loc, StatementType::VariableAssignment)
        , mPattern(pattern)
        , mExpression(expr)
    {
    }

    [[nodiscard]] inline Ptr<Pattern> pattern() const { return mPattern; }
    [[nodiscard]] inline Ptr<Expression> expression() const { return mExpression; }
    [[nodiscard]] inline Ptr<Expression>& expressionMut() & { return mExpression; }

private:
    Ptr<Pattern> mPattern;
    Ptr<Expression> mExpression;
};

class FunctionDeclarationStatement : public Statement {
public:
    FunctionDeclarationStatement(const parser::Location& loc, const std::string& name, const type::ParameterList& parameters, const Ptr<Closure>& closure, const type::Type& returnType, const std::string& mangledName, bool hasSideEffects)
        : Statement(loc, StatementType::FunctionDeclaration)
        , mParameters(parameters)
        , mReturnType(returnType)
        , mName(name)
        , mMangledName(mangledName)
        , mClosure(closure)
        , mHasSideEffects(hasSideEffects)
    {
    }

    [[nodiscard]] inline const std::string& name() const { return mName; }
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
    const std::string mName;
    const std::string mMangledName;
    const Ptr<Closure> mClosure;
    const bool mHasSideEffects;
};

class TypeAliasStatement : public Statement {
public:
    TypeAliasStatement(const parser::Location& loc, const std::string& name, const type::Type& aliasedType)
        : Statement(loc, StatementType::TypeAlias)
        , mName(name)
        , mAliasedType(aliasedType)
    {
    }

    [[nodiscard]] inline const std::string& name() const { return mName; }
    [[nodiscard]] inline const type::Type& aliasedType() const { return mAliasedType; }

private:
    const std::string mName;
    const type::Type mAliasedType;
};

} // namespace PExpr::ast