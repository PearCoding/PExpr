#pragma once

#include "SymbolTable.h"
#include "ast/Closure.h"
#include "ast/Expression.h"
#include "ast/Statement.h"
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
    Type handleNode(const Ptr<ast::Closure>& closure);

    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::Expression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::ClosureExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::BranchExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::VariableExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::LiteralExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::UnaryExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::BinaryExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::CallExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::SwizzleExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::AccessExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::CastExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::TupleExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::AssignmentExpression>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::VariableDeclarationStatement>& expr);
    Type handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::FunctionDeclarationStatement>& expr);

    [[nodiscard]] Ptr<ast::Expression> injectCastIfNeeded(const Ptr<ast::Expression>& origExpression, const Type& toType, bool* hadError);

    utils::Reporter& mReporter;
};

} // namespace PExpr::type
