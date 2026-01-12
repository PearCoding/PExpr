#include "UpliftPass.h"
#include "Expression.h"
#include "Mangler.h"
#include "Statement.h"
#include "TypeChecker.h"

#include <algorithm>
#include <sstream>

namespace PExpr::internal {

UpliftPass::UpliftPass(Reporter& reporter)
    : mReporter(reporter)
{
}

void UpliftPass::handle(const Ptr<Closure>& closure)
{
    processClosure(closure);
}

void UpliftPass::processClosure(const Ptr<Closure>& closure)
{
    // Second pass: for each function declaration, determine captured variables and
    // uplift them to parameters; also recursively process nested closures.
    for (const auto& stmt : closure->statements()) {
        if (stmt->type() != StatementType::FunctionDeclaration)
            continue;

        auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);

        // If extern skip
        if (f->isExtern())
            continue;

        // collect captures from function body
        std::map<std::string, VariableDef> captured;

        if (f->closure()) {
            collectCapturesFromClosureBody(f->closure(), captured);

            // For each captured variable, add a new parameter at the end of the function's parameter list
            if (!captured.empty()) {
                // We need to create a new parameter list combining existing parameters + captured
                ParameterList newParams = f->parameters();
                for (const auto& kv : captured) {
                    const auto& v = kv.second;
                    newParams.push_back(Parameter{ v.name(), v.type() });
                }

                // Build new mangled name with new parameter types
                std::vector<ElementaryType> newParamTypes;
                newParamTypes.reserve(newParams.size());
                for (const auto& p : newParams)
                    newParamTypes.push_back(p.Type);

                const std::string oldMangled = f->mangledName();
                const std::string newMangled = makeMangledNameFromTypes(f->name(), newParamTypes, closure.get());

                // Update local symbol table: replace function entry
                closure->symbols().removeFunction(FunctionDef(f->name(), oldMangled, f->parameters(), f->returnType(), f->isExtern()));
                closure->symbols().replaceFunction(FunctionDef(f->name(), newMangled, newParams, f->returnType(), f->isExtern()));

                // Replace the function declaration by constructing a new one and replacing in the closure
                auto newFunc = std::make_shared<FunctionDeclarationStatement>(f->location(), f->name(), newParams, f->closure(), f->returnType(), newMangled);

                PEXPR_ASSERT(newFunc->name() == f->name(), "Name of function should stay the same after uplift");
                PEXPR_ASSERT(newFunc->returnType() == f->returnType(), "Return type of function should stay the same after uplift");

                // Replace in closure statements:
                closure->replaceStatement(stmt, newFunc);

                // Now update all calls in this closure's scope to pass the captured variables
                // We'll append arguments corresponding to the captured variables in the same order they were added.
                // We need to determine the expressions to pass: these are variable expressions referencing the captured names
                // We'll traverse the closure (top-level closure) and update calls.
                updateCallsInClosure(closure);
            }

            // Recurse into nested closure body
            processClosure(f->closure());
        }
    }

    // Finally, process nested closures that are not function declarations (i.e., closure expressions inside top-level expr)
    if (closure->expression() && closure->expression()->type() == ExpressionType::Closure) {
        auto cexpr = std::reinterpret_pointer_cast<ClosureExpression>(closure->expression());
        processClosure(cexpr->closure());
    }
}

void UpliftPass::collectCapturesFromClosureBody(const Ptr<Closure>& closure, std::map<std::string, VariableDef>& outCaptured)
{
    // Now traverse statements expressions
    for (const auto& stmt : closure->statements()) {
        // For variable decl/assign and function decl bodies, inspect expressions
        if (stmt->type() == StatementType::VariableDeclaration) {
            collectCapturesFromExpression(closure, std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt)->expression(), outCaptured);
        } else if (stmt->type() == StatementType::VariableAssignment) {
            collectCapturesFromExpression(closure, std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt)->expression(), outCaptured);
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            if (!f->isExtern())
                collectCapturesFromClosureBody(f->closure(), outCaptured);
        }
    }

    // Finally check the final expression
    if (closure->expression())
        collectCapturesFromExpression(closure, closure->expression(), outCaptured);
}

void UpliftPass::collectCapturesFromExpression(const Ptr<Closure>& closure, const Ptr<Expression>& expr, std::map<std::string, VariableDef>& outCaptured)
{
    if (!expr)
        return;

    switch (expr->type()) {
    case ExpressionType::Variable: {
        auto v                 = std::reinterpret_pointer_cast<VariableExpression>(expr);
        const SymbolTable* tbl = nullptr;
        if (auto def = closure->symbols().lookupVariable(v->location(), v->name(), &tbl); def.has_value()) {
            if (tbl != &closure->symbols()) {
                // captured
                outCaptured.emplace(def->name(), def.value());
            }
        } else {
            mReporter.errorf(v->location(), "Unknown identifier '%s' found during uplift", v->name().c_str());
        }
    } break;
    case ExpressionType::Literal:
        break;
    case ExpressionType::Unary: {
        auto u = std::reinterpret_pointer_cast<UnaryExpression>(expr);
        collectCapturesFromExpression(closure, u->inner(), outCaptured);
    } break;
    case ExpressionType::Binary: {
        auto b = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        collectCapturesFromExpression(closure, b->left(), outCaptured);
        collectCapturesFromExpression(closure, b->right(), outCaptured);
    } break;
    case ExpressionType::Call: {
        auto c = std::reinterpret_pointer_cast<CallExpression>(expr);
        for (const auto& p : c->parameters())
            collectCapturesFromExpression(closure, p, outCaptured);
    } break;
    case ExpressionType::Access: {
        auto a = std::reinterpret_pointer_cast<AccessExpression>(expr);
        collectCapturesFromExpression(closure, a->inner(), outCaptured);
    } break;
    case ExpressionType::Cast: {
        auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        collectCapturesFromExpression(closure, c->inner(), outCaptured);
    } break;
    case ExpressionType::Vector: {
        auto v = std::reinterpret_pointer_cast<VectorExpression>(expr);
        for (const auto& e : v->entries())
            collectCapturesFromExpression(closure, e, outCaptured);
    } break;
    case ExpressionType::Closure: {
        auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        collectCapturesFromClosureBody(c->closure(), outCaptured);
    } break;
    case ExpressionType::Branch: {
        auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        collectCapturesFromExpression(closure, br->elseClosure()->expression(), outCaptured);
        for (const auto& b : br->branches()) {
            collectCapturesFromExpression(closure, b.Condition, outCaptured);
            collectCapturesFromClosureBody(b.Body, outCaptured);
        }
    } break;
    default:
        break;
    }
}

void UpliftPass::updateCallsInExpression(const Ptr<Closure>& closure, const Ptr<Expression>& expr)
{
    if (!expr)
        return;

    switch (expr->type()) {
    case ExpressionType::Call: {
        auto c = std::reinterpret_pointer_cast<CallExpression>(expr);
        // Lookup function in currentDefs to find its parameter list
        std::vector<ElementaryType> argTypes;
        for (const auto& p : c->parameters())
            argTypes.push_back(p->returnType());

        if (auto def = closure->symbols().lookupFunction(c->location(), c->name(), argTypes, false); def.has_value()) {
            // If the function def has more parameters than provided, it means uplift added parameters
            const auto& fparams = def->parameters();
            if (fparams.size() > c->parameters().size()) {
                // For each additional parameter, create a VariableExpression pointing to the captured variable name
                size_t existing = c->parameters().size();
                for (size_t i = existing; i < fparams.size(); ++i) {
                    const std::string capName = fparams[i].Name;
                    auto vexpr                = std::make_shared<VariableExpression>(c->location(), capName);
                    // Set the expression return type to the declared parameter type so downstream passes (SSA) see correct types
                    vexpr->setReturnType(fparams[i].Type);
                    c->appendParameter(vexpr);
                }
                // update mangled name to the new one (function may have been replaced)
                c->setMangledName(def->mangledName());
            }
        }

        // Recurse into parameters
        for (const auto& p : c->parameters())
            updateCallsInExpression(closure, p);

    } break;
    case ExpressionType::Unary: {
        auto u = std::reinterpret_pointer_cast<UnaryExpression>(expr);
        updateCallsInExpression(closure, u->inner());
    } break;
    case ExpressionType::Binary: {
        auto b = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        updateCallsInExpression(closure, b->left());
        updateCallsInExpression(closure, b->right());
    } break;
    case ExpressionType::Access: {
        auto a = std::reinterpret_pointer_cast<AccessExpression>(expr);
        updateCallsInExpression(closure, a->inner());
    } break;
    case ExpressionType::Cast: {
        auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        updateCallsInExpression(closure, c->inner());
    } break;
    case ExpressionType::Vector: {
        auto v = std::reinterpret_pointer_cast<VectorExpression>(expr);
        for (const auto& e : v->entries())
            updateCallsInExpression(closure, e);
    } break;
    case ExpressionType::Closure: {
        auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        updateCallsInClosure(c->closure());
    } break;
    case ExpressionType::Branch: {
        auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        updateCallsInClosure(br->elseClosure());
        for (const auto& b : br->branches()) {
            updateCallsInExpression(closure, b.Condition);
            updateCallsInClosure(b.Body);
        }
    } break;
    default:
        break;
    }
}

void UpliftPass::updateCallsInClosure(const Ptr<Closure>& closure)
{
    // Update calls inside statements
    for (const auto& stmt : closure->statements()) {
        // For variable decl/assign and function decl bodies, inspect expressions
        if (stmt->type() == StatementType::VariableDeclaration) {
            updateCallsInExpression(closure, std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt)->expression());
        } else if (stmt->type() == StatementType::VariableAssignment) {
            updateCallsInExpression(closure, std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt)->expression());
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            if (!f->isExtern())
                updateCallsInClosure(f->closure());
        }
    }

    // Update final expression
    if (closure->expression()) {
        if (closure->expression()->type() == ExpressionType::Closure)
            updateCallsInClosure(std::reinterpret_pointer_cast<ClosureExpression>(closure->expression())->closure());
        else
            updateCallsInExpression(closure, closure->expression());
    }
}

} // namespace PExpr::internal
