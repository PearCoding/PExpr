#pragma once

#include "Expression.h"

namespace PExpr {
/// Simple visitor which will construct a parsable representation of the given AST.
class StringVisitor {
public:
    static std::string visit(const Ptr<Expression>& expr)
    {
        switch (expr->type()) {
        case ExpressionType::Variable:
            return dump(std::reinterpret_pointer_cast<VariableExpression>(expr));
        case ExpressionType::Literal:
            return dump(std::reinterpret_pointer_cast<LiteralExpression>(expr));
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

    static std::string dump(const Ptr<LiteralExpression>& expr)
    {
        if (expr->returnType() == ElementaryType::Boolean)
            return expr->getBool() ? "true" : "false";
        if (expr->returnType() == ElementaryType::Integer)
            return std::to_string(expr->getInteger());
        if (expr->returnType() == ElementaryType::Number)
            return std::to_string(expr->getNumber());
        if (expr->returnType() == ElementaryType::String)
            return "\"" + expr->getString() + "\"";
        return "UNKNOWN";
    }

    static std::string dump(const Ptr<UnaryExpression>& expr)
    {
        return std::string(toString(expr->op())) + "(" + visit(expr->inner()) + ")";
    }

    static std::string dump(const Ptr<BinaryExpression>& expr)
    {
        return "(" + visit(expr->left()) + ")"
               + std::string(toString(expr->op()))
               + "(" + visit(expr->right()) + ")";
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