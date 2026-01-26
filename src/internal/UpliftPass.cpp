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

        const auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);

        // If extern skip
        if (f->isExtern())
            continue;

        // collect captures from function body
        std::map<std::string, VariableDef> captured;

        if (f->closure()) {
            collectCapturesFromClosureBody(f->closure(), f->closure(), captured);

            // For each captured variable, add a new parameter at the end of the function's parameter list
            if (!captured.empty()) {
                // We need to create a new parameter list combining existing parameters + captured
                ParameterList newParams = f->parameters();
                for (const auto& kv : captured) {
                    const auto& v = kv.second;
                    newParams.push_back(Parameter{ v.name(), v.type() });
                }

                // Build new mangled name with new parameter types
                std::vector<Type> newParamTypes;
                newParamTypes.reserve(newParams.size());
                for (const auto& p : newParams)
                    newParamTypes.push_back(p.Type);

                const std::string oldMangled = f->mangledName();
                const std::string newMangled = makeMangledNameFromTypes(f->name(), newParamTypes, closure.get());

                // Update local symbol table: replace function entry
                const auto oldDef = FunctionDef(f->name(), oldMangled, f->parameters(), f->returnType(), f->isExtern(), f->hasSideEffects());
                const auto newDef = FunctionDef(f->name(), newMangled, newParams, f->returnType(), f->isExtern(), f->hasSideEffects());

                closure->symbols().removeFunction(oldDef);
                closure->symbols().replaceFunction(FunctionDef(newDef));

                // Replace the function declaration by constructing a new one and replacing in the closure
                auto newFunc = std::make_shared<FunctionDeclarationStatement>(f->location(), f->name(), newParams, f->closure(), f->returnType(), newMangled, f->hasSideEffects());

                PEXPR_ASSERT(newFunc->name() == f->name(), "Name of function should stay the same after uplift");
                PEXPR_ASSERT(newFunc->returnType() == f->returnType(), "Return type of function should stay the same after uplift");

                // Replace in closure statements:
                closure->replaceStatement(stmt, std::move(newFunc));

                // Now update all calls in this closure's scope to pass the captured variables
                // We'll append arguments corresponding to the captured variables in the same order they were added.
                // We need to determine the expressions to pass: these are variable expressions referencing the captured names
                // We'll traverse the closure (top-level closure) and update calls.
                updateCallsInClosure(closure, oldDef, newDef);
            }

            // Recurse into nested closure body
            processClosure(f->closure());
        }
    }

    // Finally, process nested closures that are not function declarations (i.e., closure expressions inside top-level expr)
    if (closure->expression() && closure->expression()->type() == ExpressionType::Closure) {
        const auto cexpr = std::reinterpret_pointer_cast<ClosureExpression>(closure->expression());
        processClosure(cexpr->closure());
    }
}

void UpliftPass::collectCapturesFromClosureBody(const Ptr<Closure>& funcClosure, const Ptr<Closure>& closure, std::map<std::string, VariableDef>& outCaptured)
{
    // Now traverse statements expressions
    for (const auto& stmt : closure->statements()) {
        // For variable decl/assign and function decl bodies, inspect expressions
        if (stmt->type() == StatementType::VariableDeclaration) {
            collectCapturesFromExpression(funcClosure, closure, std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt)->expression(), outCaptured);
        } else if (stmt->type() == StatementType::VariableAssignment) {
            collectCapturesFromExpression(funcClosure, closure, std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt)->expression(), outCaptured);
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            const auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            if (!f->isExtern())
                collectCapturesFromClosureBody(funcClosure, f->closure(), outCaptured); //< TODO: Really?
        }
    }

    // Finally check the final expression
    if (closure->expression())
        collectCapturesFromExpression(funcClosure, closure, closure->expression(), outCaptured);
}

void UpliftPass::collectCapturesFromExpression(const Ptr<Closure>& funcClosure, const Ptr<Closure>& closure, const Ptr<Expression>& expr, std::map<std::string, VariableDef>& outCaptured)
{
    if (!expr)
        return;

    switch (expr->type()) {
    case ExpressionType::Variable: {
        const auto v           = std::reinterpret_pointer_cast<VariableExpression>(expr);
        const SymbolTable* tbl = nullptr;
        if (auto def = closure->symbols().lookupVariable(v->location(), v->name(), &tbl); def.has_value()) {
            // Go up the ladder until we find the function closure or global
            while (tbl && tbl != &funcClosure->symbols())
                tbl = tbl->parent();

            if (!tbl) //< captured (above the function closure)
                outCaptured.emplace(def->name(), def.value());
        } else {
            mReporter.errorf(v->location(), "Unknown identifier '%s' found during uplift", v->name().c_str());
        }
    } break;
    case ExpressionType::Literal:
        break;
    case ExpressionType::Unary: {
        const auto u = std::reinterpret_pointer_cast<UnaryExpression>(expr);
        collectCapturesFromExpression(funcClosure, closure, u->inner(), outCaptured);
    } break;
    case ExpressionType::Binary: {
        const auto b = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        collectCapturesFromExpression(funcClosure, closure, b->left(), outCaptured);
        collectCapturesFromExpression(funcClosure, closure, b->right(), outCaptured);
    } break;
    case ExpressionType::Call: {
        const auto c = std::reinterpret_pointer_cast<CallExpression>(expr);
        for (const auto& p : c->parameters())
            collectCapturesFromExpression(funcClosure, closure, p, outCaptured);
    } break;
    case ExpressionType::Swizzle: {
        const auto a = std::reinterpret_pointer_cast<SwizzleExpression>(expr);
        collectCapturesFromExpression(funcClosure, closure, a->inner(), outCaptured);
    } break;
    case ExpressionType::Access: {
        const auto a = std::reinterpret_pointer_cast<AccessExpression>(expr);
        collectCapturesFromExpression(funcClosure, closure, a->inner(), outCaptured);
    } break;
    case ExpressionType::Cast: {
        const auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        collectCapturesFromExpression(funcClosure, closure, c->inner(), outCaptured);
    } break;
    case ExpressionType::Vector: {
        const auto v = std::reinterpret_pointer_cast<VectorExpression>(expr);
        for (const auto& e : v->entries())
            collectCapturesFromExpression(funcClosure, closure, e, outCaptured);
    } break;
    case ExpressionType::Closure: {
        const auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        collectCapturesFromClosureBody(funcClosure, c->closure(), outCaptured);
    } break;
    case ExpressionType::Branch: {
        const auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        collectCapturesFromExpression(funcClosure, br->elseClosure(), br->elseClosure()->expression(), outCaptured);
        for (const auto& b : br->branches()) {
            collectCapturesFromExpression(funcClosure, closure, b.Condition, outCaptured);
            collectCapturesFromClosureBody(funcClosure, b.Body, outCaptured);
        }
    } break;
    default:
        break;
    }
}

void UpliftPass::updateCallsInExpression(const Ptr<Expression>& expr, const FunctionDef& oldDef, const FunctionDef& newDef)
{
    if (!expr)
        return;

    switch (expr->type()) {
    case ExpressionType::Call: {
        const auto c = std::reinterpret_pointer_cast<CallExpression>(expr);
        if (c->mangledName() == oldDef.mangledName()) {
            // If the function def has more parameters than provided, it means uplift added parameters
            const auto& fparams = newDef.parameters();
            if (fparams.size() > oldDef.parameters().size()) {
                // For each additional parameter, create a VariableExpression pointing to the captured variable name
                size_t existing = oldDef.parameters().size();
                for (size_t i = existing; i < fparams.size(); ++i) {
                    const std::string capName = fparams[i].Name;
                    const auto vexpr          = std::make_shared<VariableExpression>(c->location(), capName);
                    // Set the expression return type to the declared parameter type so downstream passes (SSA) see correct types
                    vexpr->setReturnType(fparams[i].Type);
                    c->appendParameter(vexpr);
                }

                // update mangled name to the new one (function may have been replaced)
                c->setMangledName(newDef.mangledName());
            }
        }

        // Recurse into parameters
        for (const auto& p : c->parameters())
            updateCallsInExpression(p, oldDef, newDef);

    } break;
    case ExpressionType::Unary: {
        const auto u = std::reinterpret_pointer_cast<UnaryExpression>(expr);
        updateCallsInExpression(u->inner(), oldDef, newDef);
    } break;
    case ExpressionType::Binary: {
        const auto b = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        updateCallsInExpression(b->left(), oldDef, newDef);
        updateCallsInExpression(b->right(), oldDef, newDef);
    } break;
    case ExpressionType::Swizzle: {
        const auto a = std::reinterpret_pointer_cast<SwizzleExpression>(expr);
        updateCallsInExpression(a->inner(), oldDef, newDef);
    } break;
    case ExpressionType::Access: {
        const auto a = std::reinterpret_pointer_cast<AccessExpression>(expr);
        updateCallsInExpression(a->inner(), oldDef, newDef);
    } break;
    case ExpressionType::Cast: {
        const auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        updateCallsInExpression(c->inner(), oldDef, newDef);
    } break;
    case ExpressionType::Vector: {
        const auto v = std::reinterpret_pointer_cast<VectorExpression>(expr);
        for (const auto& e : v->entries())
            updateCallsInExpression(e, oldDef, newDef);
    } break;
    case ExpressionType::Closure: {
        const auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        updateCallsInClosure(c->closure(), oldDef, newDef);
    } break;
    case ExpressionType::Branch: {
        const auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        updateCallsInClosure(br->elseClosure(), oldDef, newDef);
        for (const auto& b : br->branches()) {
            updateCallsInExpression(b.Condition, oldDef, newDef);
            updateCallsInClosure(b.Body, oldDef, newDef);
        }
    } break;
    default:
        break;
    }
}

void UpliftPass::updateCallsInClosure(const Ptr<Closure>& closure, const FunctionDef& oldDef, const FunctionDef& newDef)
{
    // Update calls inside statements
    for (const auto& stmt : closure->statements()) {
        // For variable decl/assign and function decl bodies, inspect expressions
        if (stmt->type() == StatementType::VariableDeclaration) {
            updateCallsInExpression(std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt)->expression(), oldDef, newDef);
        } else if (stmt->type() == StatementType::VariableAssignment) {
            updateCallsInExpression(std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt)->expression(), oldDef, newDef);
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            const auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            if (!f->isExtern())
                updateCallsInClosure(f->closure(), oldDef, newDef);
        }
    }

    // Update final expression
    if (closure->expression()) {
        if (closure->expression()->type() == ExpressionType::Closure)
            updateCallsInClosure(std::reinterpret_pointer_cast<ClosureExpression>(closure->expression())->closure(), oldDef, newDef);
        else
            updateCallsInExpression(closure->expression(), oldDef, newDef);
    }
}

} // namespace PExpr::internal
