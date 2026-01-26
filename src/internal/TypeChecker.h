#pragma once

#include "PExpr.h"
#include "Reporter.h"
#include "internal/SymbolTable.h"

namespace PExpr::internal {

class TypeChecker {
public:
    explicit TypeChecker(Reporter& reporter)
        : mReporter(reporter)
    {
    }

    [[nodiscard]] Type handle(const Ptr<Closure>& closure);

private:
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure);
    void handleNode(const Ptr<Closure>& closure, const Ptr<Statement>& statement);

    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<Expression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<ClosureExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<BranchExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<VariableExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<LiteralExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<UnaryExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<BinaryExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<CallExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<SwizzleExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<AccessExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<CastExpression>& expr);
    [[nodiscard]] Type handleNode(const Ptr<Closure>& closure, const Ptr<TupleExpression>& expr);

    Reporter& mReporter;
};

} // namespace PExpr::internal
