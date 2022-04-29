#pragma once

#include "Enums.h"
#include "pexpr_config.h"

#include <variant>

namespace PExpr {
class Expression {
public:
    Expression() = delete;

    inline ExpressionType type() const { return mType; }

protected:
    inline explicit Expression(ExpressionType type)
        : mType(type)
    {
    }

private:
    ExpressionType mType;
};

// Special expression used if an error occured while parsing
class ErrorExpression : public Expression {
public:
    inline ErrorExpression()
        : Expression(ExpressionType::Error)
    {
    }
};

class VariableExpression : public Expression {
public:
    inline VariableExpression(const std::string& name)
        : Expression(ExpressionType::Variable)
        , mName(name)
        , mVariableType(ElementaryType::Unspecified)
    {
    }

    inline const std::string& name() const { return mName; }
    inline ElementaryType variableType() const { return mVariableType; }

    // Will be used in later stages
    inline bool isUnspecified() const { return mVariableType == ElementaryType::Unspecified; }
    inline void setVariableType(ElementaryType type) { mVariableType = type; }

private:
    std::string mName;
    ElementaryType mVariableType;
};

class ConstExpression : public Expression {
public:
    using ValueVariant = std::variant<bool, Integer, Number, std::string>;

    inline ConstExpression(ElementaryType type, const ValueVariant& value)
        : Expression(ExpressionType::Const)
        , mValueType(type)
        , mValue(value)
    {
        PEXPR_ASSERT(type != ElementaryType::Unspecified, "Expected a specified type as a constant");
    }

    inline ElementaryType valueType() const { return mValueType; }

    inline bool getBool() const
    {
        PEXPR_ASSERT(mValueType == ElementaryType::Boolean, "Trying to get a constant which is not a boolean");
        return std::get<bool>(mValue);
    }

    inline Integer getInteger() const
    {
        PEXPR_ASSERT(mValueType == ElementaryType::Integer, "Trying to get a constant which is not a integer");
        return std::get<Integer>(mValue);
    }

    inline Number getNumber() const
    {
        PEXPR_ASSERT(mValueType == ElementaryType::Number, "Trying to get a constant which is not a number");
        return std::get<Number>(mValue);
    }

    inline std::string getString() const
    {
        PEXPR_ASSERT(mValueType == ElementaryType::String, "Trying to get a constant which is not a string");
        return std::get<std::string>(mValue);
    }

private:
    ElementaryType mValueType;
    ValueVariant mValue;
};

class UnaryExpression : public Expression {
public:
    inline UnaryExpression(UnaryOperation op, const Ptr<Expression>& expr)
        : Expression(ExpressionType::Unary)
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
    inline BinaryExpression(BinaryOperation op, const Ptr<Expression>& left, const Ptr<Expression>& right)
        : Expression(ExpressionType::Binary)
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

    inline CallExpression(const std::string& name, const ParameterList& parameters)
        : Expression(ExpressionType::Call)
        , mName(name)
        , mParameters(parameters)
    {
    }

    inline CallExpression(const std::string& name, ParameterList&& parameters)
        : Expression(ExpressionType::Call)
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
    inline AccessExpression(const Ptr<Expression>& expr, const std::string& swizzle)
        : Expression(ExpressionType::Access)
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