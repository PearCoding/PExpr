#pragma once

#include "Expression.h"

namespace PExpr {
class StringVisitor {
public:
    static std::string visit(const Ptr<Expression>& expr)
    {
        switch (expr->type()) {
        case ExpressionType::Variable:
            return dump(std::reinterpret_pointer_cast<VariableExpression>(expr));
        case ExpressionType::Const:
            return dump(std::reinterpret_pointer_cast<ConstExpression>(expr));
        case ExpressionType::Unary:
            return dump(std::reinterpret_pointer_cast<UnaryExpression>(expr));
        case ExpressionType::Binary:
            return dump(std::reinterpret_pointer_cast<BinaryExpression>(expr));
        case ExpressionType::Call:
            return dump(std::reinterpret_pointer_cast<CallExpression>(expr));
        case ExpressionType::Access:
            return dump(std::reinterpret_pointer_cast<AccessExpression>(expr));
        default:
            return "ERROR";
        }
    };

private:
    static std::string dump(const Ptr<VariableExpression>& expr)
    {
        return expr->name();
    }

    static std::string dump(const Ptr<ConstExpression>& expr)
    {
        if (expr->valueType() == ElementaryType::Boolean)
            return expr->getBool() ? "true" : "false";
        if (expr->valueType() == ElementaryType::Integer)
            return std::to_string(expr->getInteger());
        if (expr->valueType() == ElementaryType::Number)
            return std::to_string(expr->getNumber());
        if (expr->valueType() == ElementaryType::String)
            return "\"" + expr->getString() + "\"";
        return "UNKNOWN";
    }

    static std::string dump(const Ptr<UnaryExpression>& expr)
    {
        std::string inner = "(" + visit(expr->inner()) + ")";
        switch (expr->op()) {
        case UnaryOperation::Neg:
            return "-" + inner;
        case UnaryOperation::Not:
            return "!" + inner;
        default:
            return "Invalid";
        }
    }
    static std::string dump(const Ptr<BinaryExpression>& expr)
    {
        std::string left  = "(" + visit(expr->left()) + ")";
        std::string right = "(" + visit(expr->right()) + ")";
        switch (expr->op()) {
        case BinaryOperation::Add:
            return left + "+" + right;
        case BinaryOperation::Sub:
            return left + "-" + right;
        case BinaryOperation::Mul:
            return left + "*" + right;
        case BinaryOperation::Div:
            return left + "/" + right;
        case BinaryOperation::Pow:
            return left + "^" + right;
        case BinaryOperation::Mod:
            return left + "%" + right;
        case BinaryOperation::And:
            return left + "&&" + right;
        case BinaryOperation::Or:
            return left + "||" + right;
        case BinaryOperation::Less:
            return left + "<" + right;
        case BinaryOperation::Greater:
            return left + ">" + right;
        case BinaryOperation::LessEqual:
            return left + "<=" + right;
        case BinaryOperation::GreaterEqual:
            return left + ">=" + right;
        case BinaryOperation::Equal:
            return left + "==" + right;
        case BinaryOperation::NotEqual:
            return left + "!=" + right;
        default:
            return "Invalid";
        }
    }
    static std::string dump(const Ptr<CallExpression>& expr)
    {
        std::string str = expr->name() + "(";
        for (size_t i = 0; i < expr->parameters().size(); ++i) {
            str += visit(expr->parameters().at(i));
            if (i != expr->parameters().size() - 1)
                str += ",";
        }

        return str + ")";
    }

    static std::string dump(const Ptr<AccessExpression>& expr)
    {
        return "(" + visit(expr->inner()) + ")." + expr->swizzle();
    }
};

} // namespace PExpr