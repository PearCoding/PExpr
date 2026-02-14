#include "UpliftPass.h"
#include "ClosureAnalyzer.h"
#include "ast/Expression.h"
#include "ast/Statement.h"
#include "ast/Visitor.h"
#include "type/Mangler.h"

#include <algorithm>
#include <functional>
#include <sstream>

namespace PExpr::type {
using namespace ast;

UpliftPass::UpliftPass(utils::Reporter& reporter)
    : mReporter(reporter)
{
}

void UpliftPass::handle(const Ptr<Closure>& closureRoot)
{
    auto handleExpression = [this](Closure* closure, Expression* expr) {
        const auto f = dynamic_cast<FunctionDeclarationStatement*>(expr);

        // If not a function declaration or extern skip
        if (!f || f->isExtern())
            return;

        PEXPR_ASSERT(f->closure(), "A non-external function needs a body");

        // collect captures from function body
        std::map<std::string, Ptr<VariableDef>> capturedUsage;
        std::map<std::string, Ptr<VariableDef>> capturedMutable;

        ClosureAnalyzer analyzer(mReporter, true, true);
        analyzer.analyzeClosure(f->closure().get(), capturedUsage, capturedMutable);

        // For each captured variable, add a new parameter at the end of the function's parameter list
        Ptr<Closure> newFuncClosure = f->closure();
        if (!capturedUsage.empty()) {
            std::unordered_map<Ptr<VariableDef>, Ptr<VariableDef>> capturedToParameter;
            std::unordered_map<Ptr<VariableDef>, Ptr<VariableDef>> parameterToCaptured;

            // We need to create a new parameter list combining existing parameters + captured
            ParameterList newParams = f->parameters();
            for (const auto& kv : capturedUsage) {
                auto v  = kv.second;
                auto pv = std::make_shared<VariableDef>(v->name(), v->type(), capturedMutable.contains(kv.first), f->location());

                capturedToParameter[v]  = pv;
                parameterToCaptured[pv] = v;

                newParams.push_back(pv);
                if (!f->closure()->symbols().addVariable(pv))
                    mReporter.errorf(f->location(), "Parameter '%s' for uplifted variable already exists in the current scope", pv->name().c_str());
            }

            // Update all captured variables such that the parameters are used instead
            updateVariables(f->closure().get(), capturedToParameter);

            // Determine new return type
            auto newReturnType = f->functionReturnType();
            if (!capturedMutable.empty()) {
                // Create tuple type: (original_return_type, mutable_var1_type, mutable_var2_type, ...)
                std::vector<Type> tupleComponents;
                if (!newReturnType.isVoid())
                    tupleComponents.push_back(newReturnType);
                for (const auto& mutVar : capturedMutable)
                    tupleComponents.push_back(mutVar.second->type());
                newReturnType = Type(std::move(tupleComponents));
            }

            // Build new mangled name with new parameter types
            const std::string oldMangled = f->mangledName();
            const std::string newMangled = makeMangledNameFromTypes(f->name(), newParams, closure);

            // Update local symbol table: replace function entry
            const auto oldDef = FunctionDef(f->name(), oldMangled, f->parameters(), f->functionReturnType(), f->isExtern(), f->hasSideEffects(), f->location());
            const auto newDef = FunctionDef(f->name(), newMangled, newParams, newReturnType, f->isExtern(), f->hasSideEffects(), f->location());

            closure->symbols().removeFunction(oldDef);
            closure->symbols().replaceFunction(FunctionDef(newDef));

            if (!capturedMutable.empty()) {
                // Create new closure with modified return expression
                auto newClosure = std::make_shared<Closure>(f->closure()->location(), f->closure()->parent());
                // Copy expressions
                for (const auto& s : f->closure()->expressions())
                    newClosure->addExpression(s);

                if (f->closure()->hasFinalExpression()) //< Replace the final expression with a tuple that includes mutable captures
                    newClosure->finalExpressionMut() = createReturnTuple(f->closure().get(), f->closure()->finalExpression(), capturedMutable);
                else //< If the function is void, add a new expression returning the mutables only
                    newClosure->addExpression(createReturnTuple(f->closure().get(), nullptr, capturedMutable));

                // Copy symbol table
                newClosure->symbols() = f->closure()->symbols();
                newFuncClosure        = newClosure;
            }

            // Replace the function declaration by constructing a new one and replacing in the closure
            auto newFunc = std::make_shared<FunctionDeclarationStatement>(f->location(), f->name(), newParams, newFuncClosure, newReturnType, newMangled, f->hasSideEffects());

            PEXPR_ASSERT(newFunc->name() == f->name(), "Name of function should stay the same after uplift");

            // Replace in closure statements:
            closure->replaceExpression(expr, std::move(newFunc));

            // Now update all calls in this closure's scope to pass the captured variables
            // We'll append arguments corresponding to the captured variables in the same order they were added.
            updateCallsInClosure(closure, oldDef, newDef, parameterToCaptured, capturedMutable);
        }
    };

    // For each function declaration, determine captured variables and
    // uplift them to parameters; also recursively process nested closures.
    Visitor::forEachExpression(closureRoot.get(), handleExpression, false /* Go from bottom to up*/);
}

//-------------------------------------------------------------------------------------

void UpliftPass::updateCallsInExpression(ast::Closure* currentClosure, Ptr<Expression>& expr,
                                         const FunctionDef& oldDef, const FunctionDef& newDef,
                                         const std::unordered_map<Ptr<VariableDef>, Ptr<VariableDef>>& parameterToCaptured,
                                         const std::map<std::string, Ptr<VariableDef>>& mutableCaptures)
{
    if (!expr)
        return;

    switch (expr->type()) {
    case ExpressionType::Call: {
        auto c = std::reinterpret_pointer_cast<CallExpression>(expr);
        if (c->mangledName() == oldDef.mangledName()) {
            // If the function def has more parameters than provided, it means uplift added parameters
            const auto& fparams = newDef.parameters();
            if (fparams.size() > oldDef.parameters().size()) {
                // For each additional parameter, create a VariableExpression pointing to the captured variable name
                size_t existing = oldDef.parameters().size();
                for (size_t i = existing; i < fparams.size(); ++i) {
                    const auto capVar = parameterToCaptured.at(fparams[i]);
                    const auto vexpr  = std::make_shared<VariableExpression>(c->location(), capVar);
                    // Set the expression return type to the declared parameter type so downstream passes (SSA) see correct types
                    vexpr->setReturnType(capVar->type());
                    c->appendParameter(vexpr);
                }

                // update mangled name to the new one (function may have been replaced)
                c->setMangledName(newDef.mangledName());
                // update return type to the new one (function may have captured mutables)
                c->setReturnType(newDef.returnType());
            }

            // If the function returns a tuple due to mutable captures, we need to wrap the call
            // in a closure that destructures the tuple and updates the mutable variables
            if (!mutableCaptures.empty()) {
                // Create a closure that:
                // 1. Calls the function with the captured variables
                // 2. Destructures the tuple result
                // 3. Updates the mutable variables
                // 4. Returns the original result

                auto closure = std::make_shared<ast::Closure>(c->location(), currentClosure);

                // Create a pattern for destructuring the tuple
                std::vector<ast::PatternElement> patternElements;

                const bool hasResultVar = mutableCaptures.size() < newDef.returnType().size();

                Ptr<VariableDef> resultVar;
                if (hasResultVar) {
                    // First element is the result
                    resultVar = std::make_shared<VariableDef>("__result", newDef.returnType().components()[0], false, c->location());
                    patternElements.push_back(ast::PatternElement::makeSimple(c->location(), resultVar));
                    if (!closure->symbols().addVariable(resultVar))
                        mReporter.errorf(c->location(), "Variable '%s' for uplifting already exists in the current scope", resultVar->name().c_str());
                }

                // Subsequent elements are the mutable captures
                for (const auto& kv : mutableCaptures) {
                    auto mutVar = std::make_shared<VariableDef>("__mut_" + kv.first, kv.second->type(), false, c->location());
                    patternElements.push_back(ast::PatternElement::makeSimple(c->location(), mutVar));
                    if (!closure->symbols().addVariable(mutVar))
                        mReporter.errorf(c->location(), "Variable '%s' for uplifting already exists in the current scope", mutVar->name().c_str());
                }

                auto pattern = std::make_shared<ast::Pattern>(c->location(), patternElements);

                // Create a destructuring variable declaration
                auto varDecl = std::make_shared<ast::VariableDeclarationStatement>(c->location(), pattern, c);

                closure->addExpression(varDecl);

                // Add assignments to update the mutable variables
                size_t i = hasResultVar ? 1 : 0;
                for (const auto& kv : mutableCaptures) {
                    const std::string varName = kv.first;

                    // Create assignment pattern
                    auto lhsValueExpr = std::make_shared<ast::VariableExpression>(c->location(), kv.second);

                    // Create variable expression for the new value
                    auto newValueExpr = std::make_shared<ast::VariableExpression>(c->location(), patternElements.at(i).simpleBinding());
                    newValueExpr->setReturnType(kv.second->type());

                    // Create assignment statement
                    auto assignExpr = std::make_shared<ast::AssignmentExpression>(c->location(), lhsValueExpr, newValueExpr);

                    closure->addExpression(assignExpr);
                    ++i;
                }

                if (resultVar) {
                    // Set the closure's expression to return the original result
                    auto resultExpr = std::make_shared<ast::VariableExpression>(c->location(), resultVar);
                    resultExpr->setReturnType(newDef.returnType().components()[0]);
                    closure->addExpression(resultExpr);
                }

                // Create a closure expression to replace the original call
                auto closureExpr = std::make_shared<ast::ClosureExpression>(c->location(), closure);
                closureExpr->setReturnType(hasResultVar ? newDef.returnType().components()[0] : Type::Void());

                // Replace the expression
                expr = closureExpr;
            }
        }

        // Recurse into parameters
        for (auto& p : c->parameters())
            updateCallsInExpression(currentClosure, p, oldDef, newDef, parameterToCaptured, mutableCaptures);

    } break;
    case ExpressionType::Unary: {
        const auto u = std::reinterpret_pointer_cast<UnaryExpression>(expr);
        updateCallsInExpression(currentClosure, u->innerMut(), oldDef, newDef, parameterToCaptured, mutableCaptures);
    } break;
    case ExpressionType::Binary: {
        const auto b = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        updateCallsInExpression(currentClosure, b->leftMut(), oldDef, newDef, parameterToCaptured, mutableCaptures);
        updateCallsInExpression(currentClosure, b->rightMut(), oldDef, newDef, parameterToCaptured, mutableCaptures);
    } break;
    case ExpressionType::Swizzle: {
        const auto a = std::reinterpret_pointer_cast<SwizzleExpression>(expr);
        updateCallsInExpression(currentClosure, a->innerMut(), oldDef, newDef, parameterToCaptured, mutableCaptures);
    } break;
    case ExpressionType::Access: {
        const auto a = std::reinterpret_pointer_cast<AccessExpression>(expr);
        updateCallsInExpression(currentClosure, a->innerMut(), oldDef, newDef, parameterToCaptured, mutableCaptures);
    } break;
    case ExpressionType::Cast: {
        const auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        updateCallsInExpression(currentClosure, c->innerMut(), oldDef, newDef, parameterToCaptured, mutableCaptures);
    } break;
    case ExpressionType::Tuple: {
        const auto v = std::reinterpret_pointer_cast<TupleExpression>(expr);
        for (auto& e : v->entries())
            updateCallsInExpression(currentClosure, e, oldDef, newDef, parameterToCaptured, mutableCaptures);
    } break;
    case ExpressionType::Closure: {
        const auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        updateCallsInClosure(c->closure().get(), oldDef, newDef, parameterToCaptured, mutableCaptures);
    } break;
    case ExpressionType::Branch: {
        const auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        updateCallsInClosure(br->elseClosure().get(), oldDef, newDef, parameterToCaptured, mutableCaptures);
        for (auto& b : br->branches()) {
            updateCallsInExpression(currentClosure, b.Condition, oldDef, newDef, parameterToCaptured, mutableCaptures);
            updateCallsInClosure(b.Body.get(), oldDef, newDef, parameterToCaptured, mutableCaptures);
        }
    } break;
    case ExpressionType::VariableDeclaration: {
        auto declStmt = std::reinterpret_pointer_cast<VariableDeclarationStatement>(expr);
        updateCallsInExpression(currentClosure, declStmt->expressionMut(), oldDef, newDef, parameterToCaptured, mutableCaptures);
    } break;
    case ExpressionType::Assignment: {
        auto assignExpr = std::reinterpret_pointer_cast<AssignmentExpression>(expr);
        updateCallsInExpression(currentClosure, assignExpr->lvalueMut(), oldDef, newDef, parameterToCaptured, mutableCaptures); //< Really the lvalue?
        updateCallsInExpression(currentClosure, assignExpr->rvalueMut(), oldDef, newDef, parameterToCaptured, mutableCaptures);
    } break;
    case ExpressionType::FunctionDeclaration: {
        const auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(expr);
        if (!f->isExtern())
            updateCallsInClosure(f->closure().get(), oldDef, newDef, parameterToCaptured, mutableCaptures);
    } break;
    case ExpressionType::Literal:
    case ExpressionType::Variable:
    case ExpressionType::TypeAlias:
        // Ignore
        break;
    default:
        PEXPR_ASSERT(false, "Non exhaustive ExpressionType check in UpliftPass");
        break;
    }
}

void UpliftPass::updateCallsInClosure(Closure* closure,
                                      const FunctionDef& oldDef, const FunctionDef& newDef,
                                      const std::unordered_map<Ptr<VariableDef>, Ptr<VariableDef>>& parameterToCaptured,
                                      const std::map<std::string, Ptr<VariableDef>>& mutableCaptures)
{
    // Update calls inside the closure
    for (auto& expr : closure->expressions()) {
        if (expr->type() == ExpressionType::Closure)
            updateCallsInClosure(std::reinterpret_pointer_cast<ClosureExpression>(expr)->closure().get(), oldDef, newDef, parameterToCaptured, mutableCaptures);
        else
            updateCallsInExpression(closure, expr, oldDef, newDef, parameterToCaptured, mutableCaptures);
    }
}

Ptr<ast::Expression> UpliftPass::createReturnTuple(const ast::Closure* funcClosure,
                                                   const Ptr<ast::Expression>& originalReturnExpr,
                                                   const std::map<std::string, Ptr<VariableDef>>& mutableCaptures)
{
    std::vector<Type> tupleTypes;
    std::vector<Ptr<Expression>> tupleEntries;

    if (originalReturnExpr) {
        tupleTypes.push_back(originalReturnExpr->returnType());
        tupleEntries.push_back(originalReturnExpr);
    }

    // Add variable expressions for each mutable capture
    for (const auto& kv : mutableCaptures) {
        // Look up the type from the symbol table
        if (auto def = funcClosure->symbols().lookupVariable(funcClosure->location(), kv.first)) {
            auto varExpr = std::make_shared<VariableExpression>(funcClosure->location(), def);
            varExpr->setReturnType(def->type());
            tupleEntries.push_back(std::move(varExpr));
            tupleTypes.push_back(def->type());
        } else {
            mReporter.errorf(funcClosure->location(), "Can not find the mutable variable '%s' in the uplifted function", kv.first.c_str());
        }
    }

    auto tupleExpr = std::make_shared<TupleExpression>(funcClosure->location(), tupleEntries);
    tupleExpr->setReturnType(Type(std::move(tupleTypes)));

    return tupleExpr;
}

//-------------------------------------------------------------------------------------

void UpliftPass::updateVariables(ast::Closure* closure,
                                 const std::unordered_map<Ptr<VariableDef>, Ptr<VariableDef>>& capturedToParameter)
{
    Visitor::forEachExpression(
        closure,
        [&](Closure* currentClosure, Expression* expr) {
            PEXPR_UNUSED(currentClosure);
            if (expr->type() == ExpressionType::Variable) {
                auto v = dynamic_cast<VariableExpression*>(expr);
                if (capturedToParameter.contains(v->variable()))
                    v->setVariable(capturedToParameter.at(v->variable()));
            }
        });
}
} // namespace PExpr::type
