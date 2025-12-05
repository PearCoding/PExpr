#pragma once

#include "Expression.h"

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
    const Ptr<Expression> mExpression;
    const StatementType mType;
};

class VariableDeclarationStatement : public Statement {
public:
    VariableDeclarationStatement(bool mutable_, const Location& loc, const std::string& name, const Ptr<Expression>& expression)
        : Statement(loc, name, expression, StatementType::VariableDeclaration)
        , mIsMutable(mutable_)
    {
    }

    [[nodiscard]] inline bool isMutable() const { return mIsMutable; }

private:
    const bool mIsMutable;
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
    struct Parameter {
        std::string Name;
        ElementaryType Type;
    };

    using ParameterList = std::vector<Parameter>;

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
