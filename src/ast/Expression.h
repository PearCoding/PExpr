#pragma once

#include "Enums.h"
#include "parser/Location.h"
#include "type/Type.h"

namespace PExpr::ast {
/// Abstract expression. Can not be created directly.
class Expression {
public:
    Expression() = delete;

    /// The location this expression is associated with.
    [[nodiscard]] inline const parser::Location& location() const { return mLocation; }

    /// The type of expression. Depending on this value it is safe to cast to other "child" classes.
    [[nodiscard]] inline ExpressionType type() const { return mType; }

    /// The type this expression evaluates to. If no type checking is performed yet, this defaults to 'unspecified'.
    [[nodiscard]] inline const type::Type& returnType() const { return mReturnType; }

    /// Update the return type. Used by the typechecker
    inline void setReturnType(const type::Type& type) { mReturnType = type; }

    /// True if the type this expression evaluates to is yet 'unspecified'.
    [[nodiscard]] inline bool isUnspecified() const { return mReturnType.kind() == type::TypeKind::Unspecified; }

protected:
    inline Expression(const parser::Location& loc, ExpressionType type)
        : mLocation(loc)
        , mType(type)
        , mReturnType(type::TypeKind::Unspecified)
    {
    }

private:
    parser::Location mLocation;
    ExpressionType mType;
    type::Type mReturnType;
};

/// A simple access to a variable
class VariableExpression : public Expression {
public:
    inline VariableExpression(const parser::Location& loc, const std::string& name)
        : Expression(loc, ExpressionType::Variable)
        , mName(name)
    {
    }

    [[nodiscard]] inline const std::string& name() const { return mName; }

private:
    std::string mName;
};

/// A simple access to a literal
class LiteralExpression : public Expression {
public:
    inline LiteralExpression(const parser::Location& loc, const type::Type& type, const ElementaryValueVariant& value)
        : Expression(loc, ExpressionType::Literal)
        , mValue(value)
    {
        PEXPR_ASSERT(type.kind() != type::TypeKind::Unspecified, "Expected a specified type as a constant");
        setReturnType(type);
    }

    /// Return the literal value as 'bool'. Undefined behaviour if underlying literal is not a 'bool'.
    /// The type of this literal is given by returnType().
    [[nodiscard]] inline bool getBool() const
    {
        PEXPR_ASSERT(returnType().kind() == type::TypeKind::Boolean, "Trying to get a constant which is not a boolean");
        return std::get<bool>(mValue);
    }

    /// Return the literal value as 'int'. Undefined behaviour if underlying literal is not an 'int'.
    /// The type of this literal is given by returnType().
    [[nodiscard]] inline Integer getInteger() const
    {
        PEXPR_ASSERT(returnType().kind() == type::TypeKind::Integer, "Trying to get a constant which is not a integer");
        return std::get<Integer>(mValue);
    }

    /// Return the literal value as 'num'. Undefined behaviour if underlying literal is not a 'num'.
    /// The type of this literal is given by returnType().
    [[nodiscard]] inline Number getNumber() const
    {
        PEXPR_ASSERT(returnType().kind() == type::TypeKind::Number, "Trying to get a constant which is not a number");
        return std::get<Number>(mValue);
    }

    /// Return the literal value as 'str'. Undefined behaviour if underlying literal is not a 'str'.
    /// The type of this literal is given by returnType().
    [[nodiscard]] inline std::string getString() const
    {
        PEXPR_ASSERT(returnType().kind() == type::TypeKind::String, "Trying to get a constant which is not a string");
        return std::get<std::string>(mValue);
    }

private:
    ElementaryValueVariant mValue;
};

/// Cast expression represents an explicit or implicit conversion to a target Type
class CastExpression : public Expression {
public:
    inline CastExpression(const parser::Location& loc, const type::Type& toType, const Ptr<Expression>& inner, bool explicitCast = true)
        : Expression(loc, ExpressionType::Cast)
        , mToType(toType)
        , mInner(inner)
        , mExplicit(explicitCast)
    {
        PEXPR_ASSERT(inner != nullptr, "Expected valid inner expression for cast");
        setReturnType(mToType);
    }

    [[nodiscard]] inline const type::Type& toType() const { return mToType; }
    [[nodiscard]] inline Ptr<Expression> inner() const { return mInner; }
    [[nodiscard]] inline Ptr<Expression>& innerMut() & { return mInner; }
    [[nodiscard]] inline bool isExplicit() const { return mExplicit; }

private:
    type::Type mToType;
    Ptr<Expression> mInner;
    bool mExplicit;
};

/// Call to an unary operation, like +a, -a, !a, etc.
class UnaryExpression : public Expression {
public:
    inline UnaryExpression(const parser::Location& loc, UnaryOperation op, const Ptr<Expression>& expr)
        : Expression(loc, ExpressionType::Unary)
        , mOperation(op)
        , mExpr(expr)
    {
        PEXPR_ASSERT(expr != nullptr, "Expected valid pointer in unary expression");
    }

    /// The actual unary operation of this expression.
    [[nodiscard]] inline UnaryOperation op() const { return mOperation; }
    /// The inner expression the unary operation is applied to.
    [[nodiscard]] inline Ptr<Expression> inner() const { return mExpr; }
    [[nodiscard]] inline Ptr<Expression>& innerMut() & { return mExpr; }

private:
    UnaryOperation mOperation;
    Ptr<Expression> mExpr;
};

/// Call to a binary operation, like a+b, a*b, a^b, etc.
class BinaryExpression : public Expression {
public:
    inline BinaryExpression(const parser::Location& loc, BinaryOperation op, const Ptr<Expression>& left, const Ptr<Expression>& right)
        : Expression(loc, ExpressionType::Binary)
        , mOperation(op)
        , mLeft(left)
        , mRight(right)
    {
        PEXPR_ASSERT(left != nullptr && right != nullptr, "Expected valid pointer in binary expression");
    }

    /// The actual binary operation of this expression.
    [[nodiscard]] inline BinaryOperation op() const { return mOperation; }
    /// The left expression the binary operation is applied to.
    [[nodiscard]] inline Ptr<Expression> left() const { return mLeft; }
    [[nodiscard]] inline Ptr<Expression>& leftMut() & { return mLeft; }
    /// The right expression the binary operation is applied to.
    [[nodiscard]] inline Ptr<Expression> right() const { return mRight; }
    [[nodiscard]] inline Ptr<Expression>& rightMut() & { return mRight; }

private:
    BinaryOperation mOperation;
    Ptr<Expression> mLeft;
    Ptr<Expression> mRight;
};

/// A simple function call.
class CallExpression : public Expression {
public:
    using ParameterList = std::vector<Ptr<Expression>>;

    inline CallExpression(const parser::Location& loc, const std::string& name, const ParameterList& parameters)
        : Expression(loc, ExpressionType::Call)
        , mName(name)
        , mParameters(parameters)
    {
    }

    inline CallExpression(const parser::Location& loc, const std::string& name, ParameterList&& parameters)
        : Expression(loc, ExpressionType::Call)
        , mName(name)
        , mParameters(std::move(parameters))
    {
    }

    /// Name of the function (user visible).
    [[nodiscard]] inline const std::string& name() const { return mName; }
    /// The parameters of the given function.
    [[nodiscard]] inline const ParameterList& parameters() const { return mParameters; }
    [[nodiscard]] inline ParameterList& parameters() { return mParameters; }

    /// Replace a parameter expression (used by the typechecker to inject implicit casts).
    inline void replaceParameter(size_t idx, const Ptr<Expression>& expr)
    {
        PEXPR_ASSERT(idx < mParameters.size(), "Parameter index out of range");
        mParameters[idx] = expr;
    }

    /// Append a parameter expression (used by transformation passes like UpliftPass).
    inline void appendParameter(const Ptr<Expression>& expr)
    {
        PEXPR_ASSERT(expr != nullptr, "Expected valid expression");
        mParameters.push_back(expr);
    }

    /// The typechecker-provided mangled name.
    [[nodiscard]] inline const std::string& mangledName() const { return mMangledName; }
    inline void setMangledName(const std::string& name) { mMangledName = name; }

private:
    std::string mName;
    std::string mMangledName;
    ParameterList mParameters;
};

/// A component swizzle expression.
class SwizzleExpression : public Expression {
public:
    inline SwizzleExpression(const parser::Location& loc, const Ptr<Expression>& expr, const std::string& swizzle)
        : Expression(loc, ExpressionType::Swizzle)
        , mExpr(expr)
        , mSwizzle(swizzle)
    {
        PEXPR_ASSERT(expr != nullptr, "Expected valid pointer in swizzle expression");
        PEXPR_ASSERT(swizzle.size() > 0 && swizzle.size() <= 4, "Only support swizzling up to 4 components");
    }

    /// The inner expression the access operation is applied to.
    [[nodiscard]] inline Ptr<Expression> inner() const { return mExpr; }
    [[nodiscard]] inline Ptr<Expression>& innerMut() & { return mExpr; }
    /// A character coded swizzle. E.g., xzy will return a 'vec3' with [x, z, y].
    [[nodiscard]] inline const std::string& swizzle() const { return mSwizzle; }

private:
    Ptr<Expression> mExpr;
    std::string mSwizzle;
};

/// A component access expression.
class AccessExpression : public Expression {
public:
    inline AccessExpression(const parser::Location& loc, const Ptr<Expression>& expr, size_t index)
        : Expression(loc, ExpressionType::Access)
        , mExpr(expr)
        , mIndex(index)
    {
        PEXPR_ASSERT(expr != nullptr, "Expected valid pointer in access expression");
    }

    /// The inner expression the access operation is applied to.
    [[nodiscard]] inline Ptr<Expression> inner() const { return mExpr; }
    [[nodiscard]] inline Ptr<Expression>& innerMut() & { return mExpr; }

    /// The index of the vector.
    [[nodiscard]] inline size_t index() const { return mIndex; }

private:
    Ptr<Expression> mExpr;
    size_t mIndex;
};

class Closure;
/// Basic closure embedded in braces {}
class ClosureExpression : public Expression {
public:
    inline ClosureExpression(const parser::Location& loc, const Ptr<Closure>& closure)
        : Expression(loc, ExpressionType::Closure)
        , mClosure(closure)
    {
        PEXPR_ASSERT(closure != nullptr, "Expected valid pointer for closure");
    }

    [[nodiscard]] inline Ptr<Closure> closure() const { return mClosure; }
    [[nodiscard]] inline Ptr<Closure>& closureMut() & { return mClosure; }

private:
    Ptr<Closure> mClosure;
};

/// Call to an if, elif and else block.
class BranchExpression : public Expression {
public:
    struct SingleBranch {
        Ptr<Expression> Condition;
        Ptr<Closure> Body;
    };
    using ClosureList = std::vector<SingleBranch>;
    inline BranchExpression(const parser::Location& loc, const ClosureList& branches, const Ptr<Closure>& else_closure)
        : Expression(loc, ExpressionType::Branch)
        , mBranches(branches)
        , mElseClosure(else_closure)
    {
        PEXPR_ASSERT(!branches.empty(), "Expected a minimum of one condition");
        PEXPR_ASSERT(else_closure != nullptr, "Expected valid pointer for else closure");
    }

    /// The actual unary operation of this expression.
    [[nodiscard]] inline const ClosureList& branches() const { return mBranches; }
    [[nodiscard]] inline ClosureList& branches() { return mBranches; }
    /// The else expression
    [[nodiscard]] inline Ptr<Closure> elseClosure() const { return mElseClosure; }
    [[nodiscard]] inline Ptr<Closure>& elseClosureMut() & { return mElseClosure; }

private:
    ClosureList mBranches;
    Ptr<Closure> mElseClosure;
};

/// Basic vector construction [ a, b, c, d ]
class TupleExpression : public Expression {
public:
    inline TupleExpression(const parser::Location& loc, const std::vector<Ptr<Expression>>& entries)
        : Expression(loc, ExpressionType::Tuple)
        , mEntries(entries)
    {
        PEXPR_ASSERT(mEntries.size() >= 1, "Expected valid sized entries for tuple expression");
    }
    inline TupleExpression(const parser::Location& loc, std::vector<Ptr<Expression>>&& entries)
        : Expression(loc, ExpressionType::Tuple)
        , mEntries(std::move(entries))
    {
        PEXPR_ASSERT(mEntries.size() >= 1, "Expected valid sized entries for tuple expression");
    }

    [[nodiscard]] inline const std::vector<Ptr<Expression>> entries() const { return mEntries; }
    [[nodiscard]] inline std::vector<Ptr<Expression>>& entries() { return mEntries; }

private:
    std::vector<Ptr<Expression>> mEntries;
};
} // namespace PExpr::ast
