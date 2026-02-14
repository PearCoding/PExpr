#pragma once

#include "ast/Closure.h"
#include "ast/Expression.h"
#include "ast/Statement.h"

namespace PExpr::utils {
/// Simple visitor which will construct a parsable representation of the given AST.
class StringVisitor {
public:
    [[nodiscard]] inline static std::string visit(const Ptr<ast::Closure>& closure)
    {
        return dump(0, closure);
    }

    [[nodiscard]] inline static std::string visit(const Ptr<ast::Expression>& expr)
    {
        return visit(0, expr);
    }

private:
    [[nodiscard]] static std::string visit(size_t level, const Ptr<ast::Expression>& expr);

    [[nodiscard]] static std::string pad(size_t level);

    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::Closure>& closure);

    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::VariableDeclarationStatement>& statement);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::FunctionDeclarationStatement>& statement);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::TypeAliasStatement>& statement);

    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::VariableExpression>& expr);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::LiteralExpression>& expr);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::UnaryExpression>& expr);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::BinaryExpression>& expr);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::CallExpression>& expr);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::SwizzleExpression>& expr);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::AccessExpression>& expr);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::CastExpression>& expr);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::ClosureExpression>& expr);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::BranchExpression>& expr);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::TupleExpression>& expr);
    [[nodiscard]] static std::string dump(size_t level, const Ptr<ast::AssignmentExpression>& expr);
};

} // namespace PExpr::utils