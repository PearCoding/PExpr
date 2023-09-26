#pragma once

#include "Expression.h"

namespace PExpr {

class Statement {
    friend internal::TypeChecker;
    friend class Environment;

public:
    using ParameterList = std::vector<ElementaryType>;

    Statement(const Location& loc, const std::string& name, const ParameterList& parameters, const Ptr<Expression>& expression)
        : mLocation(loc)
        , mName(name)
        , mParameters(parameters)
        , mExpression(expression)
        , mIsMutable(false)
    {
    }

    /// The location this expression is assosciated with.
    inline const Location& location() const { return mLocation; }

    inline const std::string& name() const { return mName; }
    inline const ParameterList& parameters() const { return mParameters; }
    inline Ptr<Expression> expression() const { return mExpression; }

    inline bool isVariable() const { return mParameters.empty(); }
    inline bool isMutable() const { return mIsMutable; }
    inline void makeMutable(bool b = true) { mIsMutable = b; }

private:
    const Location mLocation;
    const std::string mName;
    const ParameterList mParameters;
    const Ptr<Expression> mExpression;
    bool mIsMutable;
};
} // namespace PExpr