#pragma once

#include "SymbolTable.h"
#include "ast/Closure.h"
#include "ast/Expression.h"
#include "utils/Reporter.h"

namespace PExpr::type {

class TypeChecker {
public:
    explicit TypeChecker(utils::Reporter& reporter)
        : mReporter(reporter)
    {
    }

    [[nodiscard]] Type handle(const Ptr<ast::Closure>& closure);

private:
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure);
    void handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::Statement>& statement);

    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::Expression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::ClosureExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::BranchExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::VariableExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::LiteralExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::UnaryExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::BinaryExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::CallExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::SwizzleExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::AccessExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::CastExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::TupleExpression>& expr);

    utils::Reporter& mReporter;
};

} // namespace PExpr::type
