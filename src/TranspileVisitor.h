#pragma once

#include "pexpr_config.h"

namespace PExpr {
/// Available relational operations.
enum class RelationalOp {
    Less,        /// <
    Greater,     /// >
    LessEqual,   /// <=
    GreaterEqual /// >=
};

/// Visitor used while transpiling the AST to another language.
/// Has to be fully implemented by the user.
template <typename Payload>
class TranspileVisitor {
public:
    /// Access to a variable.
    virtual Payload onVariable(const std::string& name, ElementaryType expectedType) = 0;

    /// An 'int' literal.
    virtual Payload onInteger(Integer v) = 0;

    /// A 'num' literal.
    virtual Payload onNumber(Number v) = 0;

    /// A 'bool' literal.
    virtual Payload onBool(bool v) = 0;

    /// A 'str' literal.
    virtual Payload onString(const std::string& v) = 0;

    /// Implicit casts. Currently only int -> num
    virtual Payload onCast(const Payload& v, ElementaryType fromType, ElementaryType toType) = 0;

    /// +a, -a. Only called for arithmetic types
    virtual Payload onPosNeg(bool isNeg, ElementaryType arithType, const Payload& v) = 0;

    /// !a. Only called for bool
    virtual Payload onNot(const Payload& v) = 0;

    /// a+b, a-b. Only called for arithmetic types. Both types are the same! Vectorized types should apply component wise
    virtual Payload onAddSub(bool isSub, ElementaryType arithType, const Payload& a, const Payload& b) = 0;

    /// a*b, a/b. Only called for arithmetic types. Both types are the same! Vectorized types should apply component wise
    virtual Payload onMulDiv(bool isDiv, ElementaryType arithType, const Payload& a, const Payload& b) = 0;

    /// a*f, f*a, a/f. A is an arithmetic type, f is 'num', except when a is 'int' then f is 'int' as well. Order of a*f or f*a does not matter
    virtual Payload onScale(bool isDiv, ElementaryType aType, const Payload& a, const Payload& f) = 0;

    /// a^f A is an arithmetic type, f is 'num', except when a is 'int' then f is 'int' as well. Vectorized types should apply component wise
    virtual Payload onPow(ElementaryType aType, const Payload& a, const Payload& f) = 0;

    /// a % b. Only called for int
    virtual Payload onMod(const Payload& a, const Payload& b) = 0;

    /// a&&b, a||b. Only called for bool types
    virtual Payload onAndOr(bool isOr, const Payload& a, const Payload& b) = 0;

    /// a < b... Boolean operation. a & b are of the same type. Only called for scaler arithmetic types (int, num)
    virtual Payload onRelOp(RelationalOp op, ElementaryType scalarArithType, const Payload& a, const Payload& b) = 0;

    /// a==b, a!=b. For vectorized types it should check that all equal componont wise. The negation a!=b should behave as !(a==b)
    virtual Payload onEqual(bool isNeg, ElementaryType type, const Payload& a, const Payload& b) = 0;

    /// name(...). Call to a function. Necessary casts are already handled.
    virtual Payload onFunctionCall(const std::string& name,
                                   ElementaryType returnType, const std::vector<ElementaryType>& argumentTypes,
                                   const std::vector<Payload>& argumentPayloads)
        = 0;

    /// a.xyz Access operator for vector types
    virtual Payload onAccess(const Payload& v, size_t inputSize, const std::vector<uint8>& outputPermutation) = 0;
};
} // namespace PExpr