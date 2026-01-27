#pragma once

#include "ast/Closure.h"
#include "ast/Expression.h"
#include "ast/Statement.h"

namespace PExpr::utils {
/// Simple visitor which will construct a parsable representation of the given AST.
class StringVisitor {
public:
    static std::string visit(const Ptr<ast::Closure>& closure)
    {
        return dump(closure);
    }

    static std::string visit(const Ptr<ast::Statement>& statement);
    static std::string visit(const Ptr<ast::Expression>& expr);

private:
    static std::string dump(const Ptr<ast::Closure>& closure);
    static std::string dump(const Ptr<ast::VariableDeclarationStatement>& statement);
    static std::string dump(const Ptr<ast::VariableAssignmentStatement>& statement);
    static std::string dump(const Ptr<ast::FunctionDeclarationStatement>& statement);
    static std::string dump(const Ptr<ast::VariableExpression>& expr);
    static std::string dump(const Ptr<ast::LiteralExpression>& expr);
    static std::string dump(const Ptr<ast::UnaryExpression>& expr);
    static std::string dump(const Ptr<ast::BinaryExpression>& expr);
    static std::string dump(const Ptr<ast::CallExpression>& expr);
    static std::string dump(const Ptr<ast::SwizzleExpression>& expr);
    static std::string dump(const Ptr<ast::AccessExpression>& expr);
    static std::string dump(const Ptr<ast::CastExpression>& expr);
    static std::string dump(const Ptr<ast::ClosureExpression>& expr);
    static std::string dump(const Ptr<ast::BranchExpression>& expr);
    static std::string dump(const Ptr<ast::TupleExpression>& expr);
    static std::string dump(const Ptr<ast::TypeAliasStatement>& statement);
};

} // namespace PExpr::utils