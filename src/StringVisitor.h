#pragma once

#include "Closure.h"

namespace PExpr {
/// Simple visitor which will construct a parsable representation of the given AST.
class StringVisitor {
public:
    static std::string visit(const Ptr<Closure>& closure)
    {
        return dump(closure);
    }

    static std::string visit(const Ptr<Statement>& statement)
    {
        switch (statement->type()) {
        case StatementType::Variable:
            return dump(std::reinterpret_pointer_cast<VariableStatement>(statement));
        case StatementType::Function:
            return dump(std::reinterpret_pointer_cast<FunctionStatement>(statement));
        default:
            return "ERROR";
        }
    }

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
        case ExpressionType::Closure:
            return dump(std::reinterpret_pointer_cast<ClosureExpression>(expr));
        case ExpressionType::Branch:
            return dump(std::reinterpret_pointer_cast<BranchExpression>(expr));
        default:
            return "ERROR";
        }
    };

private:
    static std::string dump(const Ptr<Closure>& closure)
    {
        std::stringstream stream;
        for (const auto& statements : closure->statements())
            stream << visit(statements) << std::endl;

        stream << visit(closure->expression());
        return stream.str();
    }

    static std::string dump(const Ptr<VariableStatement>& statement)
    {
        std::stringstream stream;
        if (statement->isMutable())
            stream << "mut ";
        stream << statement->name() << " = " << visit(statement->expression()) << ";";
        return stream.str();
    }

    static std::string dump(const Ptr<FunctionStatement>& statement)
    {
        std::stringstream stream;
        stream << "fn " << statement->name() << "(";
        for (size_t i = 0; i < statement->parameters().size(); ++i) {
            const auto param = statement->parameters().at(i);
            stream << param.Name;
            if (param.Type != ElementaryType::Unspecified)
                stream << ":" << toString(param.Type);

            if (i < statement->parameters().size() - 1)
                stream << ", ";
        }

        stream << ") = " << visit(statement->expression()) << ";";
        return stream.str();
    }

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

    static std::string dump(const Ptr<ClosureExpression>& expr)
    {
        return "{ " + visit(expr->closure()) + " }";
    }

    static std::string dump(const Ptr<BranchExpression>& expr)
    {
        std::stringstream stream;

        stream << "if " << visit(expr->branches().front().Condition) << " { " << visit(expr->branches().front().Body) << " }";

        for (size_t i = 1; i < expr->branches().size(); ++i) {
            stream << "if " << visit(expr->branches().at(i).Condition) << " { " << visit(expr->branches().at(i).Body) << " }";
        }

        stream << "else { " << visit(expr->elseClosure()) << "}";
        return stream.str();
    }
};

} // namespace PExpr