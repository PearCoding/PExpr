#include "TypeChecker.h"
#include "Logger.h"

namespace PExpr {
inline bool isConvertible(ElementaryType from, ElementaryType to)
{
    if (from == ElementaryType::Unspecified || to == ElementaryType::Unspecified)
        return false;

    if (from == to)
        return true;

    // We only allow implicit conversion from integer to float, but not back!
    return from == ElementaryType::Integer && to == ElementaryType::Number;
}

inline bool isArray(ElementaryType type)
{
    return type == ElementaryType::Vec2 || type == ElementaryType::Vec3 || type == ElementaryType::Vec4;
}

inline bool isArithmetic(ElementaryType type)
{
    return type == ElementaryType::Integer || type == ElementaryType::Number || isArray(type);
}

inline void typeError(const Ptr<UnaryExpression>& expr, ElementaryType type)
{
    PEXPR_LOG(LogLevel::Error) << "At " << expr->location() << ": Can not use operator " << toString(expr->op())
                               << " with " << toString(type) << std::endl;
}

inline void typeError(const Ptr<BinaryExpression>& expr, ElementaryType left, ElementaryType right)
{
    PEXPR_LOG(LogLevel::Error) << "At " << expr->location() << ": Can not use operator " << toString(expr->op())
                               << " with " << toString(left) << " and " << toString(right) << std::endl;
}

TypeChecker::TypeChecker(const VariableContainer& variables)
    : mVariables(variables)
{
}

ElementaryType TypeChecker::handle(const Ptr<Expression>& expr)
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
        return ElementaryType::Unspecified;
    }
}

ElementaryType TypeChecker::handleNode(const Ptr<VariableExpression>& expr)
{
    if (mVariables.hasVariable(expr->name())) {
        auto type = mVariables.checkVariable(expr->name());

        expr->setReturnType(type);
        return type;
    } else {
        PEXPR_LOG(LogLevel::Error) << "At " << expr->location() << ": Unknown identifier '" << expr->name() << "' found" << std::endl;
        return ElementaryType::Unspecified;
    }
}

ElementaryType TypeChecker::handleNode(const Ptr<ConstExpression>& expr)
{
    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<UnaryExpression>& expr)
{
    auto innerType = handle(expr->inner());
    if (innerType == ElementaryType::Unspecified)
        return innerType; // Error was caught somewhere else

    switch (expr->op()) {
    case UnaryOperation::Neg:
        if (isConvertible(innerType, ElementaryType::Number))
            expr->setReturnType(innerType);
        break;
    case UnaryOperation::Not:
        if (isConvertible(innerType, ElementaryType::Boolean))
            expr->setReturnType(innerType);
        break;
    default:
        break;
    }

    if (expr->isUnspecified())
        typeError(expr, innerType);

    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<BinaryExpression>& expr)
{
    auto leftType = handle(expr->left());
    if (leftType == ElementaryType::Unspecified)
        return leftType; // Error was caught somewhere else
    auto rightType = handle(expr->right());
    if (rightType == ElementaryType::Unspecified)
        return rightType; // Error was caught somewhere else

    switch (expr->op()) {
    case BinaryOperation::Add:
    case BinaryOperation::Sub:
        if (isArithmetic(leftType) && isArithmetic(rightType)) {
            if (leftType == rightType)
                expr->setReturnType(leftType);
            if (isConvertible(leftType, rightType))
                expr->setReturnType(rightType);
            if (isConvertible(rightType, leftType))
                expr->setReturnType(leftType);
        }
        break;
    case BinaryOperation::Mul:
    case BinaryOperation::Div:
        if (isArithmetic(leftType) && isArithmetic(rightType)) {
            if (leftType == rightType)
                expr->setReturnType(leftType);
            if (isConvertible(leftType, rightType))
                expr->setReturnType(rightType);
            if (isConvertible(rightType, leftType))
                expr->setReturnType(leftType);
            if (isArray(leftType) && isConvertible(rightType, ElementaryType::Number))
                expr->setReturnType(leftType); // vec * f, vec / f
            if (expr->op() != BinaryOperation::Div && isArray(rightType) && isConvertible(leftType, ElementaryType::Number))
                expr->setReturnType(rightType); // f * vec
        }
        break;
    case BinaryOperation::Pow:
        if (isArithmetic(leftType) && isArithmetic(rightType)) {
            if (leftType == rightType && leftType == ElementaryType::Integer)
                expr->setReturnType(leftType); // i ^ i
            if (isConvertible(leftType, ElementaryType::Number) && isConvertible(rightType, ElementaryType::Number))
                expr->setReturnType(ElementaryType::Number); // f ^ f
            if (isArray(leftType) && isConvertible(rightType, ElementaryType::Number))
                expr->setReturnType(leftType); // vec ^ f
        }
        break;
    case BinaryOperation::Mod:
        if (isConvertible(leftType, ElementaryType::Integer) && isConvertible(rightType, ElementaryType::Integer))
            expr->setReturnType(ElementaryType::Integer); // i % i
        break;
    case BinaryOperation::And:
    case BinaryOperation::Or:
    case BinaryOperation::Less:
    case BinaryOperation::Greater:
    case BinaryOperation::LessEqual:
    case BinaryOperation::GreaterEqual:
    case BinaryOperation::Equal:
    case BinaryOperation::NotEqual:
        if (isConvertible(leftType, ElementaryType::Boolean) && isConvertible(rightType, ElementaryType::Boolean))
            expr->setReturnType(ElementaryType::Boolean);
        break;
    default:
        break;
    }

    if (expr->isUnspecified())
        typeError(expr, leftType, rightType);

    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<CallExpression>& expr)
{
    std::vector<ElementaryType> args;
    args.reserve(expr->parameters().size());

    for (size_t i = 0; i < expr->parameters().size(); ++i) {
        auto type = handle(expr->parameters().at(i));
        if (type == ElementaryType::Unspecified)
            return type; // Error was caught somewhere else
        args.push_back(type);
    }

    // TODO
    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<AccessExpression>& expr)
{
    auto innerType = handle(expr->inner());
    if (innerType == ElementaryType::Unspecified)
        return innerType; // Error was caught somewhere else

    if (isArray(innerType)) {
        const auto& swizzle = expr->swizzle();

        size_t vec_size = 2;
        if (innerType == ElementaryType::Vec3)
            vec_size = 3;
        if (innerType == ElementaryType::Vec4)
            vec_size = 4;

        PEXPR_ASSERT(swizzle.size() > 0, "Expected at least a single component");
        if (swizzle.size() > vec_size) {
            PEXPR_LOG(LogLevel::Error) << "At " << expr->location() << ": Expected a maximum of " << vec_size << " components but got " << swizzle.size() << std::endl;
        } else {
            switch (vec_size) {
            case 1:
                expr->setReturnType(ElementaryType::Number);
                break;
            case 2:
                expr->setReturnType(ElementaryType::Vec2);
                break;
            case 3:
                expr->setReturnType(ElementaryType::Vec3);
                break;
            case 4:
                expr->setReturnType(ElementaryType::Vec4);
                break;
            default:
                PEXPR_ASSERT(false, "Component count switch is invalid!"); // Should never happen
                break;
            }
        }
    } else {
        PEXPR_LOG(LogLevel::Error) << "At " << expr->location() << ": Access operator is only defined for vector types" << std::endl;
    }

    return expr->returnType();
}
} // namespace PExpr