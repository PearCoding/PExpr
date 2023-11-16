#pragma once

#include "Expression.h"

namespace PExpr {

class Statement {
    friend internal::TypeChecker;
    friend class Environment;

public:
    Statement() = delete;

    /// The location this expression is assosciated with.
    inline const Location& location() const { return mLocation; }

    inline const std::string& name() const { return mName; }
    inline Ptr<Expression> expression() const { return mExpression; }

    inline StatementType type() const { return mType; }

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

class VariableStatement : public Statement {
public:
    VariableStatement(bool mutable_, const Location& loc, const std::string& name, const Ptr<Expression>& expression)
        : Statement(loc, name, expression, StatementType::Variable)
        , mIsMutable(mutable_)
    {
    }

    inline bool isMutable() const { return mIsMutable; }

private:
    const bool mIsMutable;
};

class FunctionStatement : public Statement {
public:
    struct Parameter {
        std::string Name;
        ElementaryType Type;
    };

    using ParameterList = std::vector<Parameter>;

    FunctionStatement(const Location& loc, const std::string& name, const ParameterList& parameters, const Ptr<Expression>& expression)
        : Statement(loc, name, expression, StatementType::Function)
        , mParameters(parameters)
    {
    }

    inline const ParameterList& parameters() const { return mParameters; }

private:
    const ParameterList mParameters;
};
} // namespace PExpr