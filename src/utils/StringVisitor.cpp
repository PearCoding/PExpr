#include "StringVisitor.h"

#include <functional>

namespace PExpr::utils {
using namespace ast;

std::string StringVisitor::pad(size_t level)
{
    std::string str;
    for (size_t i = 0; i < level; ++i)
        str += "  ";
    return str;
}

std::string StringVisitor::visit(size_t level, const Ptr<Statement>& statement)
{
    switch (statement->type()) {
    case StatementType::VariableDeclaration:
        return dump(level, std::reinterpret_pointer_cast<VariableDeclarationStatement>(statement));
    case StatementType::VariableAssignment:
        return dump(level, std::reinterpret_pointer_cast<VariableAssignmentStatement>(statement));
    case StatementType::FunctionDeclaration:
        return dump(level, std::reinterpret_pointer_cast<FunctionDeclarationStatement>(statement));
    case StatementType::TypeAlias:
        return dump(level, std::reinterpret_pointer_cast<TypeAliasStatement>(statement));
    default:
        return "ERROR";
    }
}

std::string StringVisitor::visit(size_t level, const Ptr<Expression>& expr)
{
    switch (expr->type()) {
    case ExpressionType::Variable:
        return dump(level, std::reinterpret_pointer_cast<VariableExpression>(expr));
    case ExpressionType::Literal:
        return dump(level, std::reinterpret_pointer_cast<LiteralExpression>(expr));
    case ExpressionType::Unary:
        return dump(level, std::reinterpret_pointer_cast<UnaryExpression>(expr));
    case ExpressionType::Binary:
        return dump(level, std::reinterpret_pointer_cast<BinaryExpression>(expr));
    case ExpressionType::Call:
        return dump(level, std::reinterpret_pointer_cast<CallExpression>(expr));
    case ExpressionType::Swizzle:
        return dump(level, std::reinterpret_pointer_cast<SwizzleExpression>(expr));
    case ExpressionType::Access:
        return dump(level, std::reinterpret_pointer_cast<AccessExpression>(expr));
    case ExpressionType::Tuple:
        return dump(level, std::reinterpret_pointer_cast<TupleExpression>(expr));
    case ExpressionType::Cast:
        return dump(level, std::reinterpret_pointer_cast<CastExpression>(expr));
    case ExpressionType::Closure:
        return dump(level, std::reinterpret_pointer_cast<ClosureExpression>(expr));
    case ExpressionType::Branch:
        return dump(level, std::reinterpret_pointer_cast<BranchExpression>(expr));
    default:
        PEXPR_ASSERT(false, "Unhandled expression type");
        return "ERROR";
    }
};

std::string StringVisitor::dump(size_t level, const Ptr<Closure>& closure)
{
    std::stringstream stream;
    for (const auto& statements : closure->statements())
        stream << pad(level) << visit(level, statements) << std::endl;

    stream << pad(level) << visit(level, closure->expression());
    return stream.str();
}

std::string StringVisitor::dump(size_t level, const Ptr<VariableDeclarationStatement>& statement)
{
    const auto& pattern = statement->pattern();

    // Check if pattern is a single simple binding (i.e., "let a = ...")
    if (pattern->size() == 1 && pattern->elements()[0].isSimpleBinding()) {
        const auto& binding = pattern->elements()[0].simpleBinding();
        std::stringstream stream;
        stream << "let ";
        if (binding.isMutable)
            stream << "mut ";
        stream << binding.name;
        if (binding.declaredType.kind() != type::TypeKind::Unspecified)
            stream << ":" << binding.declaredType.toString();
        stream << " = " << visit(level, statement->expression()) << ";";
        return stream.str();
    }

    // Otherwise, treat as destructuring pattern
    std::stringstream stream;
    stream << "let ";

    // Helper function to recursively dump pattern elements
    std::function<void(const Pattern&)> dumpPattern = [&](const Pattern& pattern) {
        stream << "[";
        for (size_t i = 0; i < pattern.elements().size(); ++i) {
            const auto& elem = pattern.elements()[i];
            if (elem.isSimpleBinding()) {
                const auto& binding = elem.simpleBinding();
                if (binding.isMutable)
                    stream << "mut ";
                stream << binding.name;
                if (binding.declaredType.kind() != type::TypeKind::Unspecified)
                    stream << ":" << binding.declaredType.toString();
            } else {
                // Nested pattern
                dumpPattern(*elem.nestedPattern());
            }
            if (i < pattern.elements().size() - 1)
                stream << ", ";
        }
        stream << "]";
    };

    stream << "*";
    dumpPattern(*pattern);
    stream << " = " << visit(level, statement->expression()) << ";";
    return stream.str();
}

std::string StringVisitor::dump(size_t level, const Ptr<VariableAssignmentStatement>& statement)
{
    const auto& pattern = statement->pattern();

    // Check if pattern is a single simple binding (i.e., "a = ...")
    if (pattern->size() == 1 && pattern->elements()[0].isSimpleBinding()) {
        const auto& binding = pattern->elements()[0].simpleBinding();
        std::stringstream stream;
        stream << binding.name;
        stream << " = " << visit(level, statement->expression()) << ";";
        return stream.str();
    }

    // Otherwise, treat as destructuring pattern
    std::stringstream stream;

    // Helper function to recursively dump pattern elements
    std::function<void(const Pattern&)> dumpPattern = [&](const Pattern& pattern) {
        stream << "[";
        for (size_t i = 0; i < pattern.elements().size(); ++i) {
            const auto& elem = pattern.elements()[i];
            if (elem.isSimpleBinding()) {
                const auto& binding = elem.simpleBinding();
                stream << binding.name;
            } else {
                // Nested pattern
                dumpPattern(*elem.nestedPattern());
            }
            if (i < pattern.elements().size() - 1)
                stream << ", ";
        }
        stream << "]";
    };

    stream << "*";
    dumpPattern(*pattern);
    stream << " = " << visit(level, statement->expression()) << ";";
    return stream.str();
}

std::string StringVisitor::dump(size_t level, const Ptr<FunctionDeclarationStatement>& statement)
{
    std::stringstream stream;
    if (statement->isExtern()) {
        stream << "[[ extern";
        if (!statement->hasSideEffects())
            stream << ", pure";
        stream << " ]] ";
    }

    stream << "fn ";

    stream << statement->name() << "(";
    for (size_t i = 0; i < statement->parameters().size(); ++i) {
        if (i)
            stream << ", ";
        const auto param = statement->parameters().at(i);
        if (param.IsMutable)
            stream << "mut ";
        stream << param.Name << ":" << param.ParamType.toString();
    }

    stream << ") -> " << statement->returnType().toString();

    if (!statement->isExtern()) {
        stream << " = {" << std::endl
               << dump(level + 1, statement->closure()) << std::endl
               << "}";
    }

    stream << ";";
    return stream.str();
}

std::string StringVisitor::dump(size_t, const Ptr<VariableExpression>& expr)
{
    return expr->name();
}

std::string StringVisitor::dump(size_t, const Ptr<LiteralExpression>& expr)
{
    if (expr->returnType().kind() == type::TypeKind::Boolean)
        return expr->getBool() ? "true" : "false";
    if (expr->returnType().kind() == type::TypeKind::Integer)
        return std::to_string(expr->getInteger());
    if (expr->returnType().kind() == type::TypeKind::Number)
        return std::to_string(expr->getNumber());
    if (expr->returnType().kind() == type::TypeKind::String)
        return "\"" + expr->getString() + "\"";
    return "UNKNOWN";
}

std::string StringVisitor::dump(size_t level, const Ptr<UnaryExpression>& expr)
{
    return std::string(toString(expr->op())) + "(" + visit(level, expr->inner()) + ")";
}

std::string StringVisitor::dump(size_t level, const Ptr<BinaryExpression>& expr)
{
    return "(" + visit(level, expr->left()) + ")"
           + std::string(toString(expr->op()))
           + "(" + visit(level, expr->right()) + ")";
}
std::string StringVisitor::dump(size_t level, const Ptr<CallExpression>& expr)
{
    std::string str = expr->name() + "(";
    for (size_t i = 0; i < expr->parameters().size(); ++i) {
        str += visit(level, expr->parameters().at(i));
        if (i != expr->parameters().size() - 1)
            str += ",";
    }

    return str + ")";
}

std::string StringVisitor::dump(size_t level, const Ptr<SwizzleExpression>& expr)
{
    return "(" + visit(level, expr->inner()) + ")." + expr->swizzle();
}

std::string StringVisitor::dump(size_t level, const Ptr<AccessExpression>& expr)
{
    return "(" + visit(level, expr->inner()) + ")[" + std::to_string(expr->index()) + "]";
}

std::string StringVisitor::dump(size_t level, const Ptr<CastExpression>& expr)
{
    std::stringstream stream;
    stream << "(" << visit(level, expr->inner()) << " as " << expr->toType().toString() << ")";
    return stream.str();
}

std::string StringVisitor::dump(size_t level, const Ptr<ClosureExpression>& expr)
{
    return "{\n" + dump(level + 1, expr->closure()) + "\n}";
}

std::string StringVisitor::dump(size_t level, const Ptr<BranchExpression>& expr)
{
    std::stringstream stream;

    stream << "if " << visit(level, expr->branches().front().Condition) << " {" << std::endl
           << dump(level + 1, expr->branches().front().Body) << std::endl
           << " }";

    for (size_t i = 1; i < expr->branches().size(); ++i) {
        stream << " elif " << visit(level, expr->branches().at(i).Condition) << " {" << std::endl
               << dump(level + 1, expr->branches().at(i).Body) << std::endl
               << "}";
    }

    stream << " else {" << std::endl
           << dump(level + 1, expr->elseClosure()) << std::endl
           << "}";
    return stream.str();
}

std::string StringVisitor::dump(size_t level, const Ptr<TupleExpression>& expr)
{
    std::stringstream stream;

    stream << "[" << visit(level, expr->entries().front());

    for (size_t i = 1; i < expr->entries().size(); ++i)
        stream << ", " << visit(level, expr->entries().at(i));

    stream << "]";
    return stream.str();
}

std::string StringVisitor::dump(size_t, const Ptr<TypeAliasStatement>& statement)
{
    std::stringstream stream;
    stream << "using " << statement->name() << " = " << statement->aliasedType().toString() << ";";
    return stream.str();
}

} // namespace PExpr::utils