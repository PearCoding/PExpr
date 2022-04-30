#pragma once

#include "Enums.h"
#include "Location.h"

namespace PExpr {
class Expression {
public:
    Expression() = delete;

    inline const Location& location() const { return mLocation; }

    inline ExpressionType type() const { return mType; }
    inline ElementaryType returnType() const { return mReturnType; }
    inline void setReturnType(ElementaryType type) { mReturnType = type; }

    inline bool isUnspecified() const { return mReturnType == ElementaryType::Unspecified; }

protected:
    inline Expression(const Location& loc, ExpressionType type)
        : mLocation(loc)
        , mType(type)
        , mReturnType(ElementaryType::Unspecified)
    {
    }

private:
    Location mLocation;
    ExpressionType mType;
    ElementaryType mReturnType;
};

// Special expression used if an error occured while parsing
class ErrorExpression : public Expression {
public:
    inline ErrorExpression(const Location& loc)
        : Expression(loc, ExpressionType::Error)
    {
    }
};

class VariableExpression : public Expression {
public:
    inline VariableExpression(const Location& loc, const std::string& name)
        : Expression(loc, ExpressionType::Variable)
        , mName(name)
    {
    }

    inline const std::string& name() const { return mName; }

private:
    std::string mName;
};

class ConstExpression : public Expression {
public:
    inline ConstExpression(const Location& loc, ElementaryType type, const ValueVariant& value)
        : Expression(loc, ExpressionType::Const)
        , mValue(value)
    {
        PEXPR_ASSERT(type != ElementaryType::Unspecified, "Expected a specified type as a constant");
        setReturnType(type);
    }

    inline bool getBool() const
    {
        PEXPR_ASSERT(returnType() == ElementaryType::Boolean, "Trying to get a constant which is not a boolean");
        return std::get<bool>(mValue);
    }

    inline Integer getInteger() const
    {
        PEXPR_ASSERT(returnType() == ElementaryType::Integer, "Trying to get a constant which is not a integer");
        return std::get<Integer>(mValue);
    }

    inline Number getNumber() const
    {
        PEXPR_ASSERT(returnType() == ElementaryType::Number, "Trying to get a constant which is not a number");
        return std::get<Number>(mValue);
    }

    inline std::string getString() const
    {
        PEXPR_ASSERT(returnType() == ElementaryType::String, "Trying to get a constant which is not a string");
        return std::get<std::string>(mValue);
    }

private:
    ValueVariant mValue;
};

class UnaryExpression : public Expression {
public:
    inline UnaryExpression(const Location& loc, UnaryOperation op, const Ptr<Expression>& expr)
        : Expression(loc, ExpressionType::Unary)
        , mOperation(op)
        , mExpr(expr)
    {
        PEXPR_ASSERT(expr != nullptr, "Expected valid pointer in unary expression");
    }

    inline UnaryOperation op() const { return mOperation; }
    inline Ptr<Expression> inner() const { return mExpr; }

private:
    UnaryOperation mOperation;
    Ptr<Expression> mExpr;
};

class BinaryExpression : public Expression {
public:
    inline BinaryExpression(const Location& loc, BinaryOperation op, const Ptr<Expression>& left, const Ptr<Expression>& right)
        : Expression(loc, ExpressionType::Binary)
        , mOperation(op)
        , mLeft(left)
        , mRight(right)
    {
        PEXPR_ASSERT(left != nullptr && right != nullptr, "Expected valid pointer in binary expression");
    }

    inline BinaryOperation op() const { return mOperation; }
    inline Ptr<Expression> left() const { return mLeft; }
    inline Ptr<Expression> right() const { return mRight; }

private:
    BinaryOperation mOperation;
    Ptr<Expression> mLeft;
    Ptr<Expression> mRight;
};

class CallExpression : public Expression {
public:
    using ParameterList = std::vector<Ptr<Expression>>;

    inline CallExpression(const Location& loc, const std::string& name, const ParameterList& parameters)
        : Expression(loc, ExpressionType::Call)
        , mName(name)
        , mParameters(parameters)
    {
    }

    inline CallExpression(const Location& loc, const std::string& name, ParameterList&& parameters)
        : Expression(loc, ExpressionType::Call)
        , mName(name)
        , mParameters(std::move(parameters))
    {
    }

    inline const std::string& name() const { return mName; }
    inline const ParameterList& parameters() const { return mParameters; }

private:
    std::string mName;
    ParameterList mParameters;
};

class AccessExpression : public Expression {
public:
    inline AccessExpression(const Location& loc, const Ptr<Expression>& expr, const std::string& swizzle)
        : Expression(loc, ExpressionType::Access)
        , mExpr(expr)
        , mSwizzle(swizzle)
    {
        PEXPR_ASSERT(expr != nullptr, "Expected valid pointer in access expression");
    }

    inline Ptr<Expression> inner() const { return mExpr; }
    inline const std::string& swizzle() const { return mSwizzle; }

private:
    Ptr<Expression> mExpr;
    std::string mSwizzle;
};

} // namespace PExpr