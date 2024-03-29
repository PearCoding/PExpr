#include "TypeChecker.h"
#include "Logger.h"

#include <algorithm>

namespace PExpr::internal {
inline void typeError(const Ptr<UnaryExpression>& expr, ElementaryType type)
{
    PEXPR_LOG(LogLevel::Error) << expr->location() << ": Can not use operator '" << toString(expr->op())
                               << "' with type '" << toString(type) << "'" << std::endl;
}

inline void typeError(const Ptr<BinaryExpression>& expr, ElementaryType left, ElementaryType right)
{
    PEXPR_LOG(LogLevel::Error) << expr->location() << ": Can not use operator '" << toString(expr->op())
                               << "' with types '" << toString(left) << "' and '" << toString(right) << "'" << std::endl;
}

TypeChecker::TypeChecker(const DefContainer& defs)
    : mDefinitions(defs)
{
}

ElementaryType TypeChecker::handle(const Ptr<Expression>& expr)
{
    switch (expr->type()) {
    case ExpressionType::Variable:
        return handleNode(std::reinterpret_pointer_cast<VariableExpression>(expr));
    case ExpressionType::Literal:
        return handleNode(std::reinterpret_pointer_cast<LiteralExpression>(expr));
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
    auto def = mDefinitions.lookupVariable(expr->location(), expr->name());
    if (def.has_value()) {
        expr->setReturnType(def.value().type());
        return def.value().type();
    } else {
        PEXPR_LOG(LogLevel::Error) << expr->location() << ": Unknown identifier '" << expr->name() << "' found" << std::endl;
        return ElementaryType::Unspecified;
    }
}

ElementaryType TypeChecker::handleNode(const Ptr<LiteralExpression>& expr)
{
    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<UnaryExpression>& expr)
{
    auto innerType = handle(expr->inner());
    if (innerType == ElementaryType::Unspecified)
        return innerType; // Error was caught somewhere else

    expr->setReturnType(ElementaryType::Unspecified);

    switch (expr->op()) {
    case UnaryOperation::Pos:
    case UnaryOperation::Neg:
        if (isArithmetic(innerType))
            expr->setReturnType(innerType);
        break;
    case UnaryOperation::Not:
        if (isConvertible(innerType, ElementaryType::Boolean))
            expr->setReturnType(ElementaryType::Boolean);
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
    auto leftType  = handle(expr->left());
    auto rightType = handle(expr->right());
    if (leftType == ElementaryType::Unspecified || rightType == ElementaryType::Unspecified)
        return rightType; // Error was caught somewhere else

    expr->setReturnType(ElementaryType::Unspecified);

    switch (expr->op()) {
    case BinaryOperation::Add:
    case BinaryOperation::Sub:
        if (isArithmetic(leftType) && isArithmetic(rightType)) {
            if (leftType == rightType)
                expr->setReturnType(leftType);
            else if (isConvertible(leftType, rightType))
                expr->setReturnType(rightType);
            else if (isConvertible(rightType, leftType))
                expr->setReturnType(leftType);
        }
        break;
    case BinaryOperation::Mul:
    case BinaryOperation::Div:
        if (isArithmetic(leftType) && isArithmetic(rightType)) {
            if (leftType == rightType)
                expr->setReturnType(leftType);
            else if (isConvertible(leftType, rightType))
                expr->setReturnType(rightType);
            else if (isConvertible(rightType, leftType))
                expr->setReturnType(leftType);
            else if (isArray(leftType) && isConvertible(rightType, ElementaryType::Number))
                expr->setReturnType(leftType); // vec * f, vec / f
            else if (expr->op() != BinaryOperation::Div && isArray(rightType) && isConvertible(leftType, ElementaryType::Number))
                expr->setReturnType(rightType); // f * vec
        }
        break;
    case BinaryOperation::Pow:
        if (isArithmetic(leftType) && isArithmetic(rightType)) {
            if (leftType == rightType && leftType == ElementaryType::Integer)
                expr->setReturnType(leftType); // i ^ i
            else if (isConvertible(leftType, ElementaryType::Number) && isConvertible(rightType, ElementaryType::Number))
                expr->setReturnType(ElementaryType::Number); // f ^ f
            else if (isArray(leftType) && isConvertible(rightType, ElementaryType::Number))
                expr->setReturnType(leftType); // vec ^ f
        }
        break;
    case BinaryOperation::Mod:
        if (isConvertible(leftType, ElementaryType::Integer) && isConvertible(rightType, ElementaryType::Integer))
            expr->setReturnType(ElementaryType::Integer); // i % i
        break;
    case BinaryOperation::And:
    case BinaryOperation::Or:
        if (isConvertible(leftType, ElementaryType::Boolean) && isConvertible(rightType, ElementaryType::Boolean))
            expr->setReturnType(ElementaryType::Boolean);
        break;
    case BinaryOperation::Less:
    case BinaryOperation::Greater:
    case BinaryOperation::LessEqual:
    case BinaryOperation::GreaterEqual:
        if (isConvertible(leftType, ElementaryType::Boolean) && isConvertible(rightType, ElementaryType::Boolean))
            expr->setReturnType(ElementaryType::Boolean);
        else if (isConvertible(leftType, ElementaryType::Number) && isConvertible(rightType, ElementaryType::Number))
            expr->setReturnType(ElementaryType::Boolean);
        break;
    case BinaryOperation::Equal:
    case BinaryOperation::NotEqual:
        if (isConvertible(leftType, rightType) || isConvertible(rightType, leftType))
            expr->setReturnType(ElementaryType::Boolean);
        break;
    default:
        break;
    }

    if (expr->isUnspecified())
        typeError(expr, leftType, rightType);

    return expr->returnType();
}

inline std::string printArgs(const std::vector<ElementaryType>& args)
{
    std::stringstream stream;

    for (size_t i = 0; i < args.size(); ++i) {
        stream << toString(args[i]);
        if (i != args.size() - 1)
            stream << ", ";
    }

    return stream.str();
}

ElementaryType TypeChecker::handleNode(const Ptr<CallExpression>& expr)
{
    std::vector<ElementaryType> fromArgs;
    fromArgs.reserve(expr->parameters().size());

    for (size_t i = 0; i < expr->parameters().size(); ++i) {
        auto type = handle(expr->parameters().at(i));
        if (type == ElementaryType::Unspecified)
            return ElementaryType::Unspecified; // Error was caught somewhere else
        fromArgs.push_back(type);
    }

    expr->setReturnType(ElementaryType::Unspecified);

    auto def = mDefinitions.lookupFunction(expr->location(), expr->name(), fromArgs);
    if (def.has_value())
        expr->setReturnType(def.value().returnType());
    else
        PEXPR_LOG(LogLevel::Error) << expr->location() << ": Function '" << expr->name() << "(" << printArgs(fromArgs) << ")' is unknown or ambigous" << std::endl;

    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<AccessExpression>& expr)
{
    auto innerType = handle(expr->inner());
    if (innerType == ElementaryType::Unspecified)
        return innerType; // Error was caught somewhere else

    expr->setReturnType(ElementaryType::Unspecified);

    // The access operator also allows expanding e.g., vec2.xyxy -> vec4 operations
    if (isArray(innerType)) {
        const auto& swizzle = expr->swizzle();

        size_t vec_size = 2;
        if (innerType == ElementaryType::Vec3)
            vec_size = 3;
        if (innerType == ElementaryType::Vec4)
            vec_size = 4;

        bool isValid = true;
        for (char c : swizzle) {
            isValid = (c == 'x' || c == 'r'
                       || c == 'y' || c == 'g'
                       || (vec_size > 2 && c == 'z') || (vec_size > 2 && c == 'b')
                       || (vec_size > 3 && c == 'w') || (vec_size > 3 && c == 'a'));

            if (!isValid)
                break;
        }

        PEXPR_ASSERT(swizzle.size() > 0, "Expected at least a single component");
        if (!isValid) {
            PEXPR_LOG(LogLevel::Error) << expr->location() << ": Invalid access components '" << swizzle << "' given" << std::endl;
        } else {
            switch (swizzle.size()) {
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
                PEXPR_LOG(LogLevel::Error) << expr->location() << ": Expected a maximum of 4 components but got " << swizzle.size() << std::endl;
                break;
            }
        }
    } else {
        PEXPR_LOG(LogLevel::Error) << expr->location() << ": Access operator is only defined for vector types" << std::endl;
    }

    return expr->returnType();
}
} // namespace PExpr::internal