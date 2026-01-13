#pragma once

#include "Closure.h"
#include "Expression.h"
#include "Statement.h"

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
        case StatementType::VariableDeclaration:
            return dump(std::reinterpret_pointer_cast<VariableDeclarationStatement>(statement));
        case StatementType::VariableAssignment:
            return dump(std::reinterpret_pointer_cast<VariableAssignmentStatement>(statement));
        case StatementType::FunctionDeclaration:
            return dump(std::reinterpret_pointer_cast<FunctionDeclarationStatement>(statement));
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
        case ExpressionType::Vector:
            return dump(std::reinterpret_pointer_cast<VectorExpression>(expr));
        case ExpressionType::Cast:
            return dump(std::reinterpret_pointer_cast<CastExpression>(expr));
        case ExpressionType::Closure:
            return dump(std::reinterpret_pointer_cast<ClosureExpression>(expr));
        case ExpressionType::Branch:
            return dump(std::reinterpret_pointer_cast<BranchExpression>(expr));
        default:
            PEXPR_ASSERT(false, "Unhandled expression type");
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

    static std::string dump(const Ptr<VariableDeclarationStatement>& statement)
    {
        std::stringstream stream;
        stream << "let ";
        if (statement->isMutable())
            stream << "mut ";
        stream << statement->name();
        if (!statement->expression()->isUnspecified())
            stream << ":" << toString(statement->expression()->returnType());

        stream << " = " << visit(statement->expression()) << ";";
        return stream.str();
    }

    static std::string dump(const Ptr<VariableAssignmentStatement>& statement)
    {
        std::stringstream stream;
        stream << statement->name() << " = " << visit(statement->expression()) << ";";
        return stream.str();
    }

    static std::string dump(const Ptr<FunctionDeclarationStatement>& statement)
    {
        std::stringstream stream;
        if (statement->isExtern())
            stream << "extern ";

        stream << "fn " << statement->name() << "(";
        for (size_t i = 0; i < statement->parameters().size(); ++i) {
            const auto param = statement->parameters().at(i);
            stream << param.Name;
            if (param.Type != ElementaryType::Unspecified)
                stream << ":" << toString(param.Type);

            if (i < statement->parameters().size() - 1)
                stream << ", ";
        }

        stream << ")";
        if (statement->returnType() != ElementaryType::Unspecified)
            stream << " -> " << toString(statement->returnType());

        if (!statement->isExtern())
            stream << " = { " << visit(statement->closure()) << " }";
        stream << ";";
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

    static std::string dump(const Ptr<CastExpression>& expr)
    {
        std::stringstream stream;
        stream << "(" << visit(expr->inner()) << " as " << toString(expr->toType()) << ")";
        return stream.str();
    }

    static std::string dump(const Ptr<ClosureExpression>& expr)
    {
        return "{\n" + visit(expr->closure()) + "\n}";
    }

    static std::string dump(const Ptr<BranchExpression>& expr)
    {
        std::stringstream stream;

        stream << "if " << visit(expr->branches().front().Condition) << " { " << visit(expr->branches().front().Body) << " }";

        for (size_t i = 1; i < expr->branches().size(); ++i) {
            stream << " elif " << visit(expr->branches().at(i).Condition) << " { " << visit(expr->branches().at(i).Body) << " }";
        }

        stream << " else { " << visit(expr->elseClosure()) << " }";
        return stream.str();
    }

    static std::string dump(const Ptr<VectorExpression>& expr)
    {
        std::stringstream stream;

        stream << "[" << visit(expr->entries().front());

        for (size_t i = 1; i < expr->entries().size(); ++i) {
            stream << ", " << visit(expr->entries().at(i));
        }

        stream << "]";
        return stream.str();
    }
};

} // namespace PExpr