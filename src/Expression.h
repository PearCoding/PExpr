#pragma once

#include "Enums.h"
#include "Location.h"

namespace PExpr {
namespace internal {
class TypeChecker;
}

/// Abstract expression. Can not be created directly.
class Expression {
    friend internal::TypeChecker;
    friend class Environment;

public:
    Expression() = delete;

    /// The location this expression is assosciated with.
    inline const Location& location() const { return mLocation; }

    /// The type of expression. Depending on this value it is safe to cast to other "child" classes.
    inline ExpressionType type() const { return mType; }

    /// The type this expression evaluates to. If no type checking is performed yet, this defaults to 'unspecified'.
    inline ElementaryType returnType() const { return mReturnType; }

    /// True if the type this expression evaluates to is yet 'unspecified'.
    inline bool isUnspecified() const { return mReturnType == ElementaryType::Unspecified; }

protected:
    inline Expression(const Location& loc, ExpressionType type)
        : mLocation(loc)
        , mType(type)
        , mReturnType(ElementaryType::Unspecified)
    {
    }

    inline void setReturnType(ElementaryType type) { mReturnType = type; }

private:
    Location mLocation;
    ExpressionType mType;
    ElementaryType mReturnType;
};

namespace internal {
/// Special expression used if an error occured while parsing.
class ErrorExpression : public Expression {
public:
    inline ErrorExpression(const Location& loc)
        : Expression(loc, ExpressionType::Error)
    {
    }
};
} // namespace internal

/// A simple access to a variable
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

/// A simple access to a literal
class LiteralExpression : public Expression {
public:
    inline LiteralExpression(const Location& loc, ElementaryType type, const ValueVariant& value)
        : Expression(loc, ExpressionType::Literal)
        , mValue(value)
    {
        PEXPR_ASSERT(type != ElementaryType::Unspecified, "Expected a specified type as a constant");
        setReturnType(type);
    }

    /// Return the literal value as 'bool'. Undefined behaviour if underlying literal is not a 'bool'.
    /// The type of this literal is given by returnType().
    inline bool getBool() const
    {
        PEXPR_ASSERT(returnType() == ElementaryType::Boolean, "Trying to get a constant which is not a boolean");
        return std::get<bool>(mValue);
    }

    /// Return the literal value as 'int'. Undefined behaviour if underlying literal is not an 'int'.
    /// The type of this literal is given by returnType().
    inline Integer getInteger() const
    {
        PEXPR_ASSERT(returnType() == ElementaryType::Integer, "Trying to get a constant which is not a integer");
        return std::get<Integer>(mValue);
    }

    /// Return the literal value as 'num'. Undefined behaviour if underlying literal is not a 'num'.
    /// The type of this literal is given by returnType().
    inline Number getNumber() const
    {
        PEXPR_ASSERT(returnType() == ElementaryType::Number, "Trying to get a constant which is not a number");
        return std::get<Number>(mValue);
    }

    /// Return the literal value as 'str'. Undefined behaviour if underlying literal is not a 'str'.
    /// The type of this literal is given by returnType().
    inline std::string getString() const
    {
        PEXPR_ASSERT(returnType() == ElementaryType::String, "Trying to get a constant which is not a string");
        return std::get<std::string>(mValue);
    }

private:
    ValueVariant mValue;
};

/// Call to an unary operation, like +a, -a, !a, etc.
class UnaryExpression : public Expression {
public:
    inline UnaryExpression(const Location& loc, UnaryOperation op, const Ptr<Expression>& expr)
        : Expression(loc, ExpressionType::Unary)
        , mOperation(op)
        , mExpr(expr)
    {
        PEXPR_ASSERT(expr != nullptr, "Expected valid pointer in unary expression");
    }

    /// The actual unary operation of this expression.
    inline UnaryOperation op() const { return mOperation; }
    /// The inner expression the unary operation is applied to.
    inline Ptr<Expression> inner() const { return mExpr; }

private:
    UnaryOperation mOperation;
    Ptr<Expression> mExpr;
};

/// Call to a binary operation, like a+b, a*b, a^b, etc.
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

    /// The actual binary operation of this expression.
    inline BinaryOperation op() const { return mOperation; }
    /// The left expression the binary operation is applied to.
    inline Ptr<Expression> left() const { return mLeft; }
    /// The right expression the binary operation is applied to.
    inline Ptr<Expression> right() const { return mRight; }

private:
    BinaryOperation mOperation;
    Ptr<Expression> mLeft;
    Ptr<Expression> mRight;
};

/// A simple function call.
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

    /// Name of the function.
    inline const std::string& name() const { return mName; }
    /// The parameters of the given function.
    inline const ParameterList& parameters() const { return mParameters; }

private:
    std::string mName;
    ParameterList mParameters;
};

/// A component access/swizzle expression.
class AccessExpression : public Expression {
public:
    inline AccessExpression(const Location& loc, const Ptr<Expression>& expr, const std::string& swizzle)
        : Expression(loc, ExpressionType::Access)
        , mExpr(expr)
        , mSwizzle(swizzle)
    {
        PEXPR_ASSERT(expr != nullptr, "Expected valid pointer in access expression");
    }

    /// The inner expression the access operation is applied to.
    inline Ptr<Expression> inner() const { return mExpr; }
    /// A character coded swizzle. E.g., xzy will return a 'vec3' with [x, z, y].
    inline const std::string& swizzle() const { return mSwizzle; }

private:
    Ptr<Expression> mExpr;
    std::string mSwizzle;
};

} // namespace PExpr