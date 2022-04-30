#pragma once

#include "DefContainer.h"
#include "TranspileVisitor.h"

namespace PExpr::internal {
template <typename Payload>
class Transpiler {
public:
    using Visitor = TranspileVisitor<Payload>;

    inline explicit Transpiler(const DefContainer& defs, Visitor* visitor)
        : mDefinitions(defs)
        , mVisitor(visitor)
    {
        PEXPR_ASSERT(visitor != nullptr, "Expected a valid pointer to a visitor");
    }

    Payload handle(const Ptr<Expression>& expr)
    {
        switch (expr->type()) {
        case ExpressionType::Variable:
            return handleNode(std::reinterpret_pointer_cast<VariableExpression>(expr));
        case ExpressionType::Const:
            return handleNode(std::reinterpret_pointer_cast<ConstExpression>(expr));
        case ExpressionType::Unary:
            return handleNode(std::reinterpret_pointer_cast<UnaryExpression>(expr));
        case ExpressionType::Binary:
            return handleNode(std::reinterpret_pointer_cast<BinaryExpression>(expr));
        case ExpressionType::Call:
            return handleNode(std::reinterpret_pointer_cast<CallExpression>(expr));
        case ExpressionType::Access:
            return handleNode(std::reinterpret_pointer_cast<AccessExpression>(expr));
        default:
            PEXPR_ASSERT(false, "Unreachable code reached!");
            return Payload{};
        }
    }

private:
    Payload handleCast(const Payload& a, ElementaryType from, ElementaryType to)
    {
        if (from == to) {
            return a;
        } else {
            PEXPR_ASSERT(from == ElementaryType::Integer && to == ElementaryType::Number, "Currently only casting from integer to number is possible!");
            return mVisitor->onCast(a, from, to);
        }
    }

    Payload handleNode(const Ptr<VariableExpression>& expr)
    {
        auto p = mDefinitions.checkVariable(expr->name());

        if (p.has_value())
            return mVisitor->onVariable(p.value().name(), p.value().type());

        PEXPR_ASSERT(false, "Should have been caught by the typechecker!");
        return Payload{};
    }

    Payload handleNode(const Ptr<ConstExpression>& expr)
    {
        switch (expr->returnType()) {
        case ElementaryType::Boolean:
            return mVisitor->onBool(expr->getBool());
        case ElementaryType::Integer:
            return mVisitor->onInteger(expr->getInteger());
        case ElementaryType::Number:
            return mVisitor->onNumber(expr->getNumber());
        case ElementaryType::String:
            return mVisitor->onString(expr->getString());
        default:
            PEXPR_ASSERT(false, "Should have been caught by the typechecker!");
            return Payload{};
        }
    }

    Payload handleNode(const Ptr<UnaryExpression>& expr)
    {
        const auto A = handle(expr->inner());
        switch (expr->op()) {
        case UnaryOperation::Pos:
        case UnaryOperation::Neg: {
            bool isNeg = expr->op() == UnaryOperation::Neg;
            return mVisitor->onPosNeg(isNeg, expr->inner()->returnType(), A);
        } break;
        case UnaryOperation::Not:
            return mVisitor->onNot(A);
        default:
            PEXPR_ASSERT(false, "Should have been caught by the typechecker!");
            return Payload{};
        }
    }

    Payload handleAddSub(bool isSub, const Payload& a, ElementaryType atype, const Payload& b, ElementaryType btype)
    {
        if (atype != btype) {
            if (atype == ElementaryType::Integer && btype == ElementaryType::Number)
                return handleAddSub(isSub, handleCast(a, atype, ElementaryType::Number), ElementaryType::Number, b, btype);
            else if (atype == ElementaryType::Integer && btype == ElementaryType::Number)
                return handleAddSub(isSub, a, atype, handleCast(b, btype, ElementaryType::Number), ElementaryType::Number);

            PEXPR_ASSERT(false, "Should have been caught by the typechecker!");
            return Payload{};
        } else {
            return mVisitor->onAddSub(isSub, atype, a, b);
        }
    }

    Payload handleMulDiv(bool isDiv, const Payload& a, ElementaryType atype, const Payload& b, ElementaryType btype)
    {
        if (atype != btype) {
            if (atype == ElementaryType::Integer && btype == ElementaryType::Number)
                return handleMulDiv(isDiv, handleCast(a, atype, ElementaryType::Number), ElementaryType::Number, b, btype);
            else if (atype == ElementaryType::Integer && btype == ElementaryType::Number)
                return handleMulDiv(isDiv, a, atype, handleCast(b, btype, ElementaryType::Number), ElementaryType::Number);

            PEXPR_ASSERT(false, "Should have been caught by the typechecker!");
            return Payload{};
        } else {
            // i/i behaves like f/f (no integer division)
            if (atype == ElementaryType::Integer)
                return handleMulDiv(isDiv, handleCast(a, atype, ElementaryType::Number), ElementaryType::Number, handleCast(b, btype, ElementaryType::Number), ElementaryType::Number);
            else
                return mVisitor->onMulDiv(isDiv, atype, a, b);
        }
    }

    Payload handleScale(bool isDiv, const Payload& a, ElementaryType atype, const Payload& b, ElementaryType btype)
    {
        if (atype != btype) {
            if (atype != ElementaryType::Integer && btype == ElementaryType::Integer)
                return handleScale(isDiv, a, atype, handleCast(b, btype, ElementaryType::Number), ElementaryType::Number);
            else if (atype == ElementaryType::Integer && btype != ElementaryType::Number)
                return handleScale(isDiv, handleCast(a, atype, ElementaryType::Number), ElementaryType::Number, b, btype);
        }
        return mVisitor->onScale(isDiv, atype, a, b);
    }

    Payload handlePow(const Payload& a, ElementaryType atype, const Payload& b, ElementaryType btype)
    {
        if (atype != btype) {
            if (atype != ElementaryType::Integer && btype == ElementaryType::Integer)
                return handlePow(a, atype, handleCast(b, btype, ElementaryType::Number), ElementaryType::Number);
            else if (atype == ElementaryType::Integer && btype != ElementaryType::Number)
                return handlePow(handleCast(a, atype, ElementaryType::Number), ElementaryType::Number, b, btype);
        }
        return mVisitor->onPow(atype, a, b);
    }

    Payload handleRelOp(RelationalOp op, const Payload& a, ElementaryType atype, const Payload& b, ElementaryType btype)
    {
        if (atype != btype) {
            if (atype == ElementaryType::Integer && btype == ElementaryType::Number)
                return handleRelOp(op, handleCast(a, atype, ElementaryType::Number), ElementaryType::Number, b, btype);
            else if (atype == ElementaryType::Number && btype == ElementaryType::Integer)
                return handleRelOp(op, a, atype, handleCast(b, btype, ElementaryType::Number), ElementaryType::Number);

            PEXPR_ASSERT(false, "Should have been caught by the typechecker!");
            return Payload{};
        } else {
            return mVisitor->onRelOp(op, atype, a, b);
        }
    }

    Payload handleNode(const Ptr<BinaryExpression>& expr)
    {
        const auto A     = handle(expr->left());
        const auto AType = expr->left()->returnType();
        const auto B     = handle(expr->right());
        const auto BType = expr->right()->returnType();

        switch (expr->op()) {
        case BinaryOperation::Add:
        case BinaryOperation::Sub:
            return handleAddSub(expr->op() == BinaryOperation::Sub, A, AType, B, BType);
        case BinaryOperation::Mul:
            if (isConvertible(BType, ElementaryType::Number) && isArray(AType))
                return handleScale(false, A, AType, B, BType);
            else if (isConvertible(AType, ElementaryType::Number) && isArray(BType))
                return handleScale(false, B, BType, A, AType);
            else
                return handleMulDiv(false, A, AType, B, BType);
        case BinaryOperation::Div:
            if (isConvertible(BType, ElementaryType::Number) && isArray(AType))
                return handleScale(true, A, AType, B, BType);
            else
                return handleMulDiv(true, A, AType, B, BType);
        case BinaryOperation::Pow:
            return handlePow(A, AType, B, BType);
        case BinaryOperation::Mod:
            return mVisitor->onMod(A, B);
        case BinaryOperation::And:
            return mVisitor->onAndOr(false, A, B);
        case BinaryOperation::Or:
            return mVisitor->onAndOr(true, A, B);
        case BinaryOperation::Less:
            return handleRelOp(RelationalOp::Less, A, AType, B, BType);
        case BinaryOperation::Greater:
            return handleRelOp(RelationalOp::Greater, A, AType, B, BType);
        case BinaryOperation::LessEqual:
            return handleRelOp(RelationalOp::LessEqual, A, AType, B, BType);
        case BinaryOperation::GreaterEqual:
            return handleRelOp(RelationalOp::GreaterEqual, A, AType, B, BType);
        case BinaryOperation::Equal:
            return mVisitor->onEqual(false, AType, A, B);
        case BinaryOperation::NotEqual:
            return mVisitor->onEqual(true, AType, A, B);
        }

        PEXPR_ASSERT(false, "Unreachable code reached!");
        return Payload{};
    }

    Payload handleNode(const Ptr<CallExpression>& expr)
    {
        const std::string& funcName = expr->name();

        std::vector<ElementaryType> types;
        std::vector<Payload> args;
        types.reserve(expr->parameters().size());
        args.reserve(expr->parameters().size());

        for (const auto& e : expr->parameters()) {
            types.push_back(e->returnType());
            args.push_back(handle(e));
        }

        auto funcs = mDefinitions.checkFunction(funcName);

        const FunctionDef* funcdef = nullptr;

        // First check for exact matches
        for (auto it = funcs.value().first; it != funcs.value().second; ++it) {
            const auto& toArgs = it->second.arguments();

            bool found = std::equal(types.begin(), types.end(), toArgs.begin());
            if (found) {
                funcdef = &it->second;
                break;
            }
        }

        // Second check (if failed) with conversions
        if (funcdef == nullptr) {
            for (auto it = funcs.value().first; it != funcs.value().second; ++it) {
                const auto& toArgs = it->second.arguments();

                bool found = std::equal(types.begin(), types.end(), toArgs.begin(),
                                        isConvertible);
                if (found) {
                    funcdef = &it->second;
                    break;
                }
            }
        }

        // Something is broken if we got so far, but got no function definition available
        if (funcdef == nullptr) {
            PEXPR_ASSERT(false, "Should have been caught by the typechecker!");
            return Payload{};
        }

        // Handle implicit casts
        for (size_t i = 0; i < args.size(); ++i) {
            ElementaryType fromType = types[i];
            ElementaryType toType   = funcdef->arguments().at(i);

            args[i] = handleCast(args[i], fromType, toType);
        }

        return mVisitor->onFunctionCall(funcName, funcdef->returnType(), funcdef->arguments(), args);
    }

    Payload handleNode(const Ptr<AccessExpression>& expr)
    {
        const auto A = handle(expr->inner());

        const auto charC = [](char c) -> uint8 {
            if (c == 'x' || c == 'r')
                return 0;
            if (c == 'y' || c == 'g')
                return 1;
            if (c == 'z' || c == 'b')
                return 2;
            else
                return 3;
        };

        const std::string& swizzle = expr->swizzle();
        std::vector<uint8> outputPermutation;
        if (swizzle.size() == 1) {
            outputPermutation = { charC(swizzle[0]) };
        } else if (swizzle.size() == 2) {
            outputPermutation = { charC(swizzle[0]),
                                  charC(swizzle[1]) };
        } else if (swizzle.size() == 3) {
            outputPermutation = { charC(swizzle[0]),
                                  charC(swizzle[1]),
                                  charC(swizzle[2]) };
        } else {
            outputPermutation = { charC(swizzle[0]),
                                  charC(swizzle[1]),
                                  charC(swizzle[2]),
                                  charC(swizzle[3]) };
        }

        const auto inputSize = typeArraySize(expr->inner()->returnType());
        PEXPR_ASSERT(inputSize > 1, "Access operator can only be used with vector types");

        return mVisitor->onAccess(A, inputSize, outputPermutation);
    }

    const DefContainer& mDefinitions;
    Visitor* mVisitor;
};

} // namespace PExpr::internal