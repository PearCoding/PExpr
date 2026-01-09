#include "UpliftPass.h"
#include "Expression.h"
#include "Mangler.h"
#include "Statement.h"
#include "TypeChecker.h"

#include <algorithm>
#include <sstream>

namespace PExpr::internal {

UpliftPass::UpliftPass(const SymbolTable& globals, Reporter& reporter)
    : mGlobals(globals)
    , mReporter(reporter)
{
}

void UpliftPass::handle(const Ptr<Closure>& closure)
{
    // Start with top-level dynamic symbol table initialized from globals
    SymbolTable dyn(mGlobals);
    processClosure(closure, dyn);
}

void UpliftPass::processClosure(const Ptr<Closure>& closure, const SymbolTable& dynDefs)
{
    // Iterate statements and process functions and nested closures.
    // Build a mutable copy of dynDefs so we can register variables/functions as we go.
    SymbolTable local = dynDefs;

    // First pass: register top-level variables and function signatures (like TypeChecker does)
    for (const auto& stmt : closure->statements()) {
        switch (stmt->type()) {
        case StatementType::VariableDeclaration: {
            auto var = std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt);
            // register variable with its type (typechecker ran already)
            const ElementaryType t = var->expression()->returnType();
            local.addVariable(VariableDef(var->name(), t, var->isMutable()));
        } break;
        case StatementType::FunctionDeclaration: {
            auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            // pre-register function with its declared parameter names and types
            std::vector<ElementaryType> paramTypes;
            std::vector<std::string> paramNames;
            paramTypes.reserve(f->parameters().size());
            for (const auto& p : f->parameters()) {
                paramTypes.push_back(p.Type);
                paramNames.push_back(p.Name);
            }
            local.addFunction(FunctionDef(f->name(), f->mangledName(), paramTypes, paramNames, f->returnType(), f->isExtern()));
        } break;
        default:
            break;
        }
    }

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

        if (f->expression() && f->expression()->type() == ExpressionType::Closure) {
            // Extract closure ptr
            auto cexpr = std::reinterpret_pointer_cast<ClosureExpression>(f->expression());
            collectCapturesFromClosureBody(cexpr->closure(), local, captured);

            // For each captured variable, add a new parameter at the end of the function's parameter list
            if (!captured.empty()) {
                // We need to create a new parameter list combining existing parameters + captured
                FunctionDeclarationStatement::ParameterList newParams = f->parameters();
                for (const auto& kv : captured) {
                    const auto& v = kv.second;
                    FunctionDeclarationStatement::Parameter p{ v.name(), v.type() };
                    newParams.push_back(std::move(p));
                }

                // Build new mangled name with new parameter types
                std::vector<ElementaryType> newParamTypes;
                std::vector<std::string> newParamNames;
                newParamTypes.reserve(newParams.size());
                newParamNames.reserve(newParams.size());
                for (const auto& p : newParams) {
                    newParamTypes.push_back(p.Type);
                    newParamNames.push_back(p.Name);
                }

                const std::string newMangled = makeMangledNameFromTypes(f->name(), newParamTypes, nullptr);

                // Replace the function declaration by constructing a new one and replacing in the closure
                auto newFunc = std::make_shared<FunctionDeclarationStatement>(f->location(), f->name(), newParams, f->expression(), f->returnType(), newMangled);

                // Replace in closure statements:
                cexpr->closure()->parent()->replaceStatement(stmt, newFunc);

                // Update local symbol table: replace function entry
                local.replaceFunction(FunctionDef(newFunc->name(), newFunc->mangledName(), newParamTypes, newParamNames, newFunc->returnType(), newFunc->isExtern()));

                // Now update all calls in this closure's scope to pass the captured variables
                // We'll append arguments corresponding to the captured variables in the same order they were added.
                // We need to determine the expressions to pass: these are variable expressions referencing the captured names
                // We'll traverse the closure (top-level closure) and update calls.
                updateCallsInClosure(closure, local);
            }

            // Recurse into nested closure body
            processClosure(cexpr->closure(), local);
        }
    }

    // Finally, process nested closures that are not function declarations (i.e., closure expressions inside top-level expr)
    if (closure->expression() && closure->expression()->type() == ExpressionType::Closure) {
        auto cexpr = std::reinterpret_pointer_cast<ClosureExpression>(closure->expression());
        processClosure(cexpr->closure(), local);
    }
}

void UpliftPass::collectCapturesFromClosureBody(const Ptr<Closure>& closure, const SymbolTable& funcDefs, std::map<std::string, VariableDef>& outCaptured)
{
    // Walk statements and expressions in closure and collect variable usages that resolve
    // to a different symbol table than funcDefs
    // First, set up a dynamic symbol table similar to TypeChecker
    SymbolTable dyn = funcDefs;

    // Register local variables and functions visible in this closure (top-level of closure)
    for (const auto& stmt : closure->statements()) {
        if (stmt->type() == StatementType::VariableDeclaration) {
            auto v = std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt);
            dyn.addVariable(VariableDef(v->name(), v->expression()->returnType(), v->isMutable()));
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            std::vector<ElementaryType> ptypes;
            std::vector<std::string> pnames;
            for (const auto& p : f->parameters()) {
                ptypes.push_back(p.Type);
                pnames.push_back(p.Name);
            }
            dyn.addFunction(FunctionDef(f->name(), f->mangledName(), ptypes, pnames, f->returnType(), f->isExtern()));
        }
    }

    // Now traverse statements expressions
    for (const auto& stmt : closure->statements()) {
        // For variable decl/assign and function decl bodies, inspect expressions
        if (stmt->type() == StatementType::VariableDeclaration || stmt->type() == StatementType::VariableAssignment) {
            collectCapturesFromExpression(stmt->expression(), dyn, outCaptured);
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            if (!f->isExtern() && f->expression()) {
                if (f->expression()->type() == ExpressionType::Closure) {
                    auto cexpr = std::reinterpret_pointer_cast<ClosureExpression>(f->expression());
                    collectCapturesFromClosureBody(cexpr->closure(), dyn, outCaptured);
                } else {
                    collectCapturesFromExpression(f->expression(), dyn, outCaptured);
                }
            }
        }
    }

    // Finally check the final expression
    if (closure->expression()) {
        if (closure->expression()->type() == ExpressionType::Closure) {
            auto cexpr = std::reinterpret_pointer_cast<ClosureExpression>(closure->expression());
            collectCapturesFromClosureBody(cexpr->closure(), dyn, outCaptured);
        } else {
            collectCapturesFromExpression(closure->expression(), dyn, outCaptured);
        }
    }
}

void UpliftPass::collectCapturesFromExpression(const Ptr<Expression>& expr, const SymbolTable& funcDefs, std::map<std::string, VariableDef>& outCaptured)
{
    if (!expr)
        return;

    switch (expr->type()) {
    case ExpressionType::Variable: {
        auto v                 = std::reinterpret_pointer_cast<VariableExpression>(expr);
        const SymbolTable* tbl = nullptr;
        if (auto def = funcDefs.lookupVariable(v->location(), v->name(), &tbl); def.has_value()) {
            if (tbl != &funcDefs) {
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
        collectCapturesFromExpression(u->inner(), funcDefs, outCaptured);
    } break;
    case ExpressionType::Binary: {
        auto b = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        collectCapturesFromExpression(b->left(), funcDefs, outCaptured);
        collectCapturesFromExpression(b->right(), funcDefs, outCaptured);
    } break;
    case ExpressionType::Call: {
        auto c = std::reinterpret_pointer_cast<CallExpression>(expr);
        for (const auto& p : c->parameters())
            collectCapturesFromExpression(p, funcDefs, outCaptured);
    } break;
    case ExpressionType::Access: {
        auto a = std::reinterpret_pointer_cast<AccessExpression>(expr);
        collectCapturesFromExpression(a->inner(), funcDefs, outCaptured);
    } break;
    case ExpressionType::Cast: {
        auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        collectCapturesFromExpression(c->inner(), funcDefs, outCaptured);
    } break;
    case ExpressionType::Vector: {
        auto v = std::reinterpret_pointer_cast<VectorExpression>(expr);
        for (const auto& e : v->entries())
            collectCapturesFromExpression(e, funcDefs, outCaptured);
    } break;
    case ExpressionType::Closure: {
        auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        collectCapturesFromClosureBody(c->closure(), funcDefs, outCaptured);
    } break;
    case ExpressionType::Branch: {
        auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        collectCapturesFromExpression(br->elseClosure()->expression(), funcDefs, outCaptured);
        for (const auto& b : br->branches()) {
            collectCapturesFromExpression(b.Condition, funcDefs, outCaptured);
            collectCapturesFromClosureBody(b.Body, funcDefs, outCaptured);
        }
    } break;
    default:
        break;
    }
}

void UpliftPass::updateCallsInExpression(const Ptr<Expression>& expr, const SymbolTable& currentDefs)
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

        if (auto def = currentDefs.lookupFunction(c->location(), c->name(), argTypes, false); def.has_value()) {
            // If the function def has more parameters than provided, it means uplift added parameters
            if (def->parameterTypes().size() > c->parameters().size()) {
                // For each additional parameter, create a VariableExpression pointing to the captured variable name
                size_t existing        = c->parameters().size();
                const auto& paramNames = def->parameterNames();
                for (size_t i = existing; i < def->parameterTypes().size(); ++i) {
                    const std::string capName = (i < paramNames.size()) ? paramNames[i] : std::string();
                    auto vexpr                = std::make_shared<VariableExpression>(c->location(), capName);
                    // Set the expression return type to the declared parameter type so downstream passes (SSA) see correct types
                    vexpr->setReturnType(def->parameterTypes()[i]);
                    // Append the parameter via public API
                    c->appendParameter(vexpr);
                }
                // update mangled name to the new one (function may have been replaced)
                c->setMangledName(def->mangledName());
            }
        }

        // Recurse into parameters
        for (const auto& p : c->parameters())
            updateCallsInExpression(p, currentDefs);

    } break;
    case ExpressionType::Unary: {
        auto u = std::reinterpret_pointer_cast<UnaryExpression>(expr);
        updateCallsInExpression(u->inner(), currentDefs);
    } break;
    case ExpressionType::Binary: {
        auto b = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        updateCallsInExpression(b->left(), currentDefs);
        updateCallsInExpression(b->right(), currentDefs);
    } break;
    case ExpressionType::Access: {
        auto a = std::reinterpret_pointer_cast<AccessExpression>(expr);
        updateCallsInExpression(a->inner(), currentDefs);
    } break;
    case ExpressionType::Cast: {
        auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        updateCallsInExpression(c->inner(), currentDefs);
    } break;
    case ExpressionType::Vector: {
        auto v = std::reinterpret_pointer_cast<VectorExpression>(expr);
        for (const auto& e : v->entries())
            updateCallsInExpression(e, currentDefs);
    } break;
    case ExpressionType::Closure: {
        auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        updateCallsInClosure(c->closure(), currentDefs);
    } break;
    case ExpressionType::Branch: {
        auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        updateCallsInExpression(br->elseClosure()->expression(), currentDefs);
        for (const auto& b : br->branches()) {
            updateCallsInExpression(b.Condition, currentDefs);
            updateCallsInClosure(b.Body, currentDefs);
        }
    } break;
    default:
        break;
    }
}

void UpliftPass::updateCallsInClosure(const Ptr<Closure>& closure, const SymbolTable& currentDefs)
{
    // Update calls inside statements
    for (const auto& stmt : closure->statements()) {
        if (stmt->type() == StatementType::VariableDeclaration || stmt->type() == StatementType::VariableAssignment) {
            updateCallsInExpression(stmt->expression(), currentDefs);
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            if (!f->isExtern() && f->expression()) {
                if (f->expression()->type() == ExpressionType::Closure) {
                    auto cexpr = std::reinterpret_pointer_cast<ClosureExpression>(f->expression());
                    updateCallsInClosure(cexpr->closure(), currentDefs);
                } else {
                    updateCallsInExpression(f->expression(), currentDefs);
                }
            }
        }
    }

    // Update final expression
    if (closure->expression()) {
        if (closure->expression()->type() == ExpressionType::Closure) {
            auto cexpr = std::reinterpret_pointer_cast<ClosureExpression>(closure->expression());
            updateCallsInClosure(cexpr->closure(), currentDefs);
        } else {
            updateCallsInExpression(closure->expression(), currentDefs);
        }
    }
}

} // namespace PExpr::internal
