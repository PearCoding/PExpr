#include "UpliftPass.h"
#include "ClosureAnalyzer.h"
#include "ast/Expression.h"
#include "ast/Statement.h"
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

void UpliftPass::handle(const Ptr<Closure>& closure)
{
    processClosure(closure);
}

void UpliftPass::processClosure(const Ptr<Closure>& closure)
{
    // For each function declaration, determine captured variables and
    // uplift them to parameters; also recursively process nested closures.
    for (const auto& stmt : closure->statements()) {
        if (stmt->type() != StatementType::FunctionDeclaration)
            continue;

        const auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);

        // If extern skip
        if (f->isExtern())
            continue;

        // collect captures from function body
        std::map<std::string, Ptr<VariableDef>> capturedUsage;
        std::map<std::string, Ptr<VariableDef>> capturedMutable;

        if (f->closure()) {
            ClosureAnalyzer analyzer(mReporter, true, true);
            analyzer.analyzeClosure(f->closure(), f->closure(), capturedUsage, capturedMutable);

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
                updateVariablesInClosure(f->closure(), capturedToParameter);

                // Determine new return type
                Type newReturnType = f->returnType();
                if (!capturedMutable.empty()) {
                    // Create tuple type: (original_return_type, mutable_var1_type, mutable_var2_type, ...)
                    std::vector<Type> tupleComponents;
                    tupleComponents.push_back(f->returnType());
                    for (const auto& mutVar : capturedMutable)
                        tupleComponents.push_back(mutVar.second->type());
                    newReturnType = Type(std::move(tupleComponents));
                }

                // Build new mangled name with new parameter types
                const std::string oldMangled = f->mangledName();
                const std::string newMangled = makeMangledNameFromTypes(f->name(), newParams, closure.get());

                // Update local symbol table: replace function entry
                const auto oldDef = FunctionDef(f->name(), oldMangled, f->parameters(), f->returnType(), f->isExtern(), f->hasSideEffects());
                const auto newDef = FunctionDef(f->name(), newMangled, newParams, newReturnType, f->isExtern(), f->hasSideEffects());

                closure->symbols().removeFunction(oldDef);
                closure->symbols().replaceFunction(FunctionDef(newDef));

                if (!capturedMutable.empty()) {
                    // Create new closure with modified return expression
                    auto newClosure = std::make_shared<Closure>(f->closure()->location(), f->closure()->parent());
                    // Copy statements
                    for (const auto& s : f->closure()->statements())
                        newClosure->addStatement(s);

                    // Replace the final expression with a tuple that includes mutable captures
                    if (f->closure()->expression())
                        newClosure->expressionMut() = createReturnTuple(f->closure(),
                                                                        f->closure()->expression(),
                                                                        capturedMutable);

                    // Copy symbol table
                    newClosure->symbols() = f->closure()->symbols();
                    newFuncClosure        = newClosure;
                }

                // Replace the function declaration by constructing a new one and replacing in the closure
                auto newFunc = std::make_shared<FunctionDeclarationStatement>(f->location(), f->name(), newParams, newFuncClosure, newReturnType, newMangled, f->hasSideEffects());

                PEXPR_ASSERT(newFunc->name() == f->name(), "Name of function should stay the same after uplift");

                // Replace in closure statements:
                closure->replaceStatement(stmt, std::move(newFunc));

                // Now update all calls in this closure's scope to pass the captured variables
                // We'll append arguments corresponding to the captured variables in the same order they were added.
                updateCallsInClosure(closure, oldDef, newDef, parameterToCaptured, capturedMutable);
            }

            // Recurse into nested closure body
            processClosure(newFuncClosure);
        }
    }

    // Finally, process nested closures that are not function declarations (i.e., closure expressions inside top-level expr)
    if (closure->expression() && closure->expression()->type() == ExpressionType::Closure) {
        const auto cexpr = std::reinterpret_pointer_cast<ClosureExpression>(closure->expression());
        processClosure(cexpr->closure());
    }
}

//-------------------------------------------------------------------------------------

void UpliftPass::updateCallsInExpression(const Ptr<ast::Closure>& currentClosure, Ptr<Expression>& expr,
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

                auto closure = std::make_shared<ast::Closure>(c->location(), currentClosure.get());

                // Create a pattern for destructuring the tuple
                std::vector<ast::PatternElement> patternElements;

                // First element is the result (use a unique name)
                auto resultVar = std::make_shared<VariableDef>("__result", newDef.returnType().components()[0], false, c->location());
                patternElements.push_back(ast::PatternElement::makeSimple(c->location(), resultVar));
                if (!closure->symbols().addVariable(resultVar))
                    mReporter.errorf(c->location(), "Variable '%s' for uplifting already exists in the current scope", resultVar->name().c_str());

                // Subsequent elements are the mutable captures
                for (const auto& kv : mutableCaptures)
                    patternElements.push_back(ast::PatternElement::makeSimple(c->location(), kv.second));

                auto pattern = std::make_shared<ast::Pattern>(c->location(), patternElements);

                // Create a destructuring variable declaration
                auto varDecl = std::make_shared<ast::VariableDeclarationStatement>(c->location(), pattern, c);

                closure->addStatement(varDecl);

                // Add assignments to update the mutable variables
                size_t i = 1;
                for (const auto& kv : mutableCaptures) {
                    const std::string varName = kv.first;

                    // Create assignment pattern
                    auto assignPattern = std::make_shared<ast::Pattern>(c->location(), std::vector<ast::PatternElement>{ ast::PatternElement::makeSimple(c->location(), patternElements.at(i).simpleBinding()) });

                    // Create variable expression for the new value
                    auto newValueExpr = std::make_shared<ast::VariableExpression>(c->location(), patternElements.at(i).simpleBinding());
                    newValueExpr->setReturnType(kv.second->type());

                    // Create assignment statement
                    auto assignStmt = std::make_shared<ast::VariableAssignmentStatement>(c->location(), assignPattern, newValueExpr);

                    closure->addStatement(assignStmt);
                    ++i;
                }

                // Set the closure's expression to return the original result
                auto resultExpr = std::make_shared<ast::VariableExpression>(c->location(), resultVar);
                resultExpr->setReturnType(newDef.returnType().components()[0]);
                closure->expressionMut() = resultExpr;

                // Create a closure expression to replace the original call
                auto closureExpr = std::make_shared<ast::ClosureExpression>(c->location(), closure);
                closureExpr->setReturnType(newDef.returnType().components()[0]);

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
        updateCallsInClosure(c->closure(), oldDef, newDef, parameterToCaptured, mutableCaptures);
    } break;
    case ExpressionType::Branch: {
        const auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        updateCallsInClosure(br->elseClosure(), oldDef, newDef, parameterToCaptured, mutableCaptures);
        for (auto& b : br->branches()) {
            updateCallsInExpression(currentClosure, b.Condition, oldDef, newDef, parameterToCaptured, mutableCaptures);
            updateCallsInClosure(b.Body, oldDef, newDef, parameterToCaptured, mutableCaptures);
        }
    } break;
    case ExpressionType::Literal:
    case ExpressionType::Variable:
        // Ignore
        break;
    default:
        PEXPR_ASSERT(false, "Non exhaustive ExpressionType check in UpliftPass");
        break;
    }
}

void UpliftPass::updateCallsInClosure(const Ptr<Closure>& closure,
                                      const FunctionDef& oldDef, const FunctionDef& newDef,
                                      const std::unordered_map<Ptr<VariableDef>, Ptr<VariableDef>>& parameterToCaptured,
                                      const std::map<std::string, Ptr<VariableDef>>& mutableCaptures)
{
    // Update calls inside statements
    for (const auto& stmt : closure->statements()) {
        // For variable decl/assign and function decl bodies, inspect expressions
        if (stmt->type() == StatementType::VariableDeclaration) {
            auto declStmt = std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt);
            updateCallsInExpression(closure, declStmt->expressionMut(), oldDef, newDef, parameterToCaptured, mutableCaptures);
        } else if (stmt->type() == StatementType::VariableAssignment) {
            auto assignStmt = std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt);
            updateCallsInExpression(closure, assignStmt->expressionMut(), oldDef, newDef, parameterToCaptured, mutableCaptures);
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            const auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            if (!f->isExtern())
                updateCallsInClosure(f->closure(), oldDef, newDef, parameterToCaptured, mutableCaptures);
        } else if (stmt->type() == StatementType::TypeAlias) {
            // Nothing to do
        } else {
            PEXPR_ASSERT(false, "Non exhaustive StatementType check in UpliftPass");
        }
    }

    // Update final expression
    if (closure->expression()) {
        auto expr = closure->expression();
        if (expr->type() == ExpressionType::Closure)
            updateCallsInClosure(std::reinterpret_pointer_cast<ClosureExpression>(expr)->closure(), oldDef, newDef, parameterToCaptured, mutableCaptures);
        else
            updateCallsInExpression(closure, closure->expressionMut(), oldDef, newDef, parameterToCaptured, mutableCaptures);
    }
}

Ptr<ast::Expression> UpliftPass::createReturnTuple(const Ptr<ast::Closure>& funcClosure,
                                                   const Ptr<ast::Expression>& originalReturnExpr,
                                                   const std::map<std::string, Ptr<VariableDef>>& mutableCaptures)
{
    std::vector<Type> tupleTypes;
    std::vector<Ptr<Expression>> tupleEntries;

    tupleTypes.push_back(originalReturnExpr->returnType());
    tupleEntries.push_back(originalReturnExpr);

    // Add variable expressions for each mutable capture
    for (const auto& kv : mutableCaptures) {
        // Look up the type from the symbol table
        if (auto def = funcClosure->symbols().lookupVariable(originalReturnExpr->location(), kv.first)) {
            auto varExpr = std::make_shared<VariableExpression>(originalReturnExpr->location(), def);
            varExpr->setReturnType(def->type());
            tupleEntries.push_back(varExpr);
            tupleTypes.push_back(def->type());
        } else {
            mReporter.errorf(originalReturnExpr->location(), "Can not find the mutable variable '%s' in the uplifted function", kv.first.c_str());
        }
    }

    auto tupleExpr = std::make_shared<TupleExpression>(originalReturnExpr->location(), tupleEntries);
    tupleExpr->setReturnType(Type(std::move(tupleTypes)));

    return tupleExpr;
}

//-------------------------------------------------------------------------------------

void UpliftPass::updateVariablesInExpression(const Ptr<ast::Expression>& expr,
                                             const std::unordered_map<Ptr<VariableDef>, Ptr<VariableDef>>& capturedToParameter)
{
    if (!expr)
        return;

    switch (expr->type()) {
    case ExpressionType::Call: {
        auto c = std::reinterpret_pointer_cast<CallExpression>(expr);
        for (auto& p : c->parameters())
            updateVariablesInExpression(p, capturedToParameter);

    } break;
    case ExpressionType::Unary: {
        const auto u = std::reinterpret_pointer_cast<UnaryExpression>(expr);
        updateVariablesInExpression(u->inner(), capturedToParameter);
    } break;
    case ExpressionType::Binary: {
        const auto b = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        updateVariablesInExpression(b->left(), capturedToParameter);
        updateVariablesInExpression(b->right(), capturedToParameter);
    } break;
    case ExpressionType::Swizzle: {
        const auto a = std::reinterpret_pointer_cast<SwizzleExpression>(expr);
        updateVariablesInExpression(a->inner(), capturedToParameter);
    } break;
    case ExpressionType::Access: {
        const auto a = std::reinterpret_pointer_cast<AccessExpression>(expr);
        updateVariablesInExpression(a->inner(), capturedToParameter);
    } break;
    case ExpressionType::Cast: {
        const auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        updateVariablesInExpression(c->inner(), capturedToParameter);
    } break;
    case ExpressionType::Tuple: {
        const auto v = std::reinterpret_pointer_cast<TupleExpression>(expr);
        for (auto& e : v->entries())
            updateVariablesInExpression(e, capturedToParameter);
    } break;
    case ExpressionType::Closure: {
        const auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        updateVariablesInClosure(c->closure(), capturedToParameter);
    } break;
    case ExpressionType::Branch: {
        const auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        updateVariablesInClosure(br->elseClosure(), capturedToParameter);
        for (auto& b : br->branches()) {
            updateVariablesInExpression(b.Condition, capturedToParameter);
            updateVariablesInClosure(b.Body, capturedToParameter);
        }
    } break;
    case ExpressionType::Literal:
        // Ignore
        break;
    case ExpressionType::Variable: {
        auto v = std::reinterpret_pointer_cast<VariableExpression>(expr);
        if (capturedToParameter.contains(v->variable()))
            v->setVariable(capturedToParameter.at(v->variable()));
    } break;
    default:
        PEXPR_ASSERT(false, "Non exhaustive ExpressionType check in UpliftPass");
        break;
    }
}

void UpliftPass::updateVariablesInClosure(const Ptr<ast::Closure>& closure,
                                          const std::unordered_map<Ptr<VariableDef>, Ptr<VariableDef>>& capturedToParameter)
{
    // Update variables inside statements
    for (const auto& stmt : closure->statements()) {
        // For variable decl/assign and function decl bodies, inspect expressions
        if (stmt->type() == StatementType::VariableDeclaration) {
            auto declStmt = std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt);
            updateVariablesInExpression(declStmt->expression(), capturedToParameter);
        } else if (stmt->type() == StatementType::VariableAssignment) {
            auto assignStmt = std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt);

            // Update the assignment if it is on a captured one
            std::function<void(Pattern&)> processPattern =
                [&](Pattern& pattern) -> void {
                for (size_t i = 0; i < pattern.size(); ++i) {
                    auto& elem = pattern.elements()[i];

                    if (elem.isSimpleBinding()) {
                        if (capturedToParameter.contains(elem.simpleBinding()))
                            elem.setAsSimpleBinding(capturedToParameter.at(elem.simpleBinding()));
                    } else {
                        // Nested pattern - recurse
                        processPattern(*elem.nestedPattern());
                    }
                }
            };

            processPattern(*assignStmt->pattern());

            updateVariablesInExpression(assignStmt->expression(), capturedToParameter);
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            const auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            if (!f->isExtern())
                updateVariablesInClosure(f->closure(), capturedToParameter);
        } else if (stmt->type() == StatementType::TypeAlias) {
            // Nothing to do
        } else {
            PEXPR_ASSERT(false, "Non exhaustive StatementType check in UpliftPass");
        }
    }

    // Update final expression
    if (closure->expression()) {
        auto expr = closure->expression();
        if (expr->type() == ExpressionType::Closure)
            updateVariablesInClosure(std::reinterpret_pointer_cast<ClosureExpression>(expr)->closure(), capturedToParameter);
        else
            updateVariablesInExpression(closure->expression(), capturedToParameter);
    }
}
} // namespace PExpr::type
