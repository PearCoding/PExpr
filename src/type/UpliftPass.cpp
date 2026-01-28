#include "UpliftPass.h"
#include "ast/Expression.h"
#include "ast/Statement.h"
#include "type/Mangler.h"

#include <algorithm>
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
        std::map<std::string, VariableDef> captured;
        std::set<std::string> mutableAssignments;

        if (f->closure()) {
            collectCapturesFromClosureBody(f->closure(), f->closure(), captured, mutableAssignments);

            // Determine which captured variables are mutable and assigned inside the closure
            std::vector<std::string> mutableCaptures;
            for (const auto& kv : captured) {
                if (kv.second.isMutable() && mutableAssignments.count(kv.first) > 0)
                    mutableCaptures.push_back(kv.first);
            }

            // For each captured variable, add a new parameter at the end of the function's parameter list
            Ptr<Closure> newFuncClosure = f->closure();
            if (!captured.empty()) {
                // We need to create a new parameter list combining existing parameters + captured
                ParameterList newParams = f->parameters();
                for (const auto& kv : captured) {
                    const auto& v        = kv.second;
                    const bool isMutable = std::find(mutableCaptures.begin(), mutableCaptures.end(), v.name()) != mutableCaptures.end();
                    newParams.push_back(Parameter{ v.name(), v.type(), isMutable });
                }

                // Determine new return type
                Type newReturnType = f->returnType();
                if (!mutableCaptures.empty()) {
                    // Create tuple type: (original_return_type, mutable_var1_type, mutable_var2_type, ...)
                    std::vector<Type> tupleComponents;
                    tupleComponents.push_back(f->returnType());
                    for (const auto& mutVar : mutableCaptures)
                        tupleComponents.push_back(captured[mutVar].type());
                    newReturnType = Type(std::move(tupleComponents));
                }

                // Build new mangled name with new parameter types
                std::vector<Type> newParamTypes;
                newParamTypes.reserve(newParams.size());
                for (const auto& p : newParams)
                    newParamTypes.push_back(p.ParamType);

                const std::string oldMangled = f->mangledName();
                const std::string newMangled = makeMangledNameFromTypes(f->name(), newParamTypes, closure.get());

                // Update local symbol table: replace function entry
                const auto oldDef = FunctionDef(f->name(), oldMangled, f->parameters(), f->returnType(), f->isExtern(), f->hasSideEffects());
                const auto newDef = FunctionDef(f->name(), newMangled, newParams, newReturnType, f->isExtern(), f->hasSideEffects());

                closure->symbols().removeFunction(oldDef);
                closure->symbols().replaceFunction(FunctionDef(newDef));

                if (!mutableCaptures.empty()) {
                    // Create new closure with modified return expression
                    auto newClosure = std::make_shared<Closure>(f->closure()->location(), f->closure()->parent());
                    // Copy statements
                    for (const auto& s : f->closure()->statements())
                        newClosure->addStatement(s);

                    // Replace the final expression with a tuple that includes mutable captures
                    if (f->closure()->expression())
                        newClosure->expressionMut() = createReturnTuple(f->closure(),
                                                                        f->closure()->expression(),
                                                                        mutableCaptures);

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
                updateCallsInClosure(closure, oldDef, newDef, mutableCaptures);
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

void UpliftPass::collectCapturesFromClosureBody(const Ptr<Closure>& funcClosure, const Ptr<Closure>& closure,
                                                std::map<std::string, VariableDef>& outCaptured,
                                                std::set<std::string>& mutableAssignments)
{
    // Now traverse statements expressions
    for (const auto& stmt : closure->statements()) {
        // For variable decl/assign and function decl bodies, inspect expressions
        if (stmt->type() == StatementType::VariableDeclaration) {
            collectCapturesFromExpression(funcClosure, closure, std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt)->expression(), outCaptured, mutableAssignments);
        } else if (stmt->type() == StatementType::VariableAssignment) {
            const auto assignStmt = std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt);
            // Traverse the pattern to find all variables being assigned
            collectMutableAssignmentsFromPattern(funcClosure, closure, assignStmt->pattern(), mutableAssignments);
            collectCapturesFromExpression(funcClosure, closure, assignStmt->expression(), outCaptured, mutableAssignments);
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            const auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            if (!f->isExtern())
                collectCapturesFromClosureBody(funcClosure, f->closure(), outCaptured, mutableAssignments);
        }
    }

    // Finally check the final expression
    if (closure->expression())
        collectCapturesFromExpression(funcClosure, closure, closure->expression(), outCaptured, mutableAssignments);
}

void UpliftPass::collectCapturesFromExpression(const Ptr<Closure>& funcClosure, const Ptr<Closure>& closure,
                                               const Ptr<Expression>& expr,
                                               std::map<std::string, VariableDef>& outCaptured,
                                               std::set<std::string>& mutableAssignments)
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
        collectCapturesFromExpression(funcClosure, closure, u->inner(), outCaptured, mutableAssignments);
    } break;
    case ExpressionType::Binary: {
        const auto b = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        collectCapturesFromExpression(funcClosure, closure, b->left(), outCaptured, mutableAssignments);
        collectCapturesFromExpression(funcClosure, closure, b->right(), outCaptured, mutableAssignments);
    } break;
    case ExpressionType::Call: {
        const auto c = std::reinterpret_pointer_cast<CallExpression>(expr);
        for (const auto& p : c->parameters())
            collectCapturesFromExpression(funcClosure, closure, p, outCaptured, mutableAssignments);
    } break;
    case ExpressionType::Swizzle: {
        const auto a = std::reinterpret_pointer_cast<SwizzleExpression>(expr);
        collectCapturesFromExpression(funcClosure, closure, a->inner(), outCaptured, mutableAssignments);
    } break;
    case ExpressionType::Access: {
        const auto a = std::reinterpret_pointer_cast<AccessExpression>(expr);
        collectCapturesFromExpression(funcClosure, closure, a->inner(), outCaptured, mutableAssignments);
    } break;
    case ExpressionType::Cast: {
        const auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        collectCapturesFromExpression(funcClosure, closure, c->inner(), outCaptured, mutableAssignments);
    } break;
    case ExpressionType::Tuple: {
        const auto v = std::reinterpret_pointer_cast<TupleExpression>(expr);
        for (const auto& e : v->entries())
            collectCapturesFromExpression(funcClosure, closure, e, outCaptured, mutableAssignments);
    } break;
    case ExpressionType::Closure: {
        const auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        collectCapturesFromClosureBody(funcClosure, c->closure(), outCaptured, mutableAssignments);
    } break;
    case ExpressionType::Branch: {
        const auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        collectCapturesFromExpression(funcClosure, br->elseClosure(), br->elseClosure()->expression(), outCaptured, mutableAssignments);
        for (const auto& b : br->branches()) {
            collectCapturesFromExpression(funcClosure, closure, b.Condition, outCaptured, mutableAssignments);
            collectCapturesFromClosureBody(funcClosure, b.Body, outCaptured, mutableAssignments);
        }
    } break;
    default:
        PEXPR_ASSERT(false, "Non exhaustive ExpressionType check in UpliftPass");
        break;
    }
}

void UpliftPass::updateCallsInExpression(Ptr<Expression>& expr, const FunctionDef& oldDef, const FunctionDef& newDef,
                                         const std::vector<std::string>& mutableCaptures)
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
                    const std::string capName = fparams[i].Name;
                    const auto vexpr          = std::make_shared<VariableExpression>(c->location(), capName);
                    // Set the expression return type to the declared parameter type so downstream passes (SSA) see correct types
                    vexpr->setReturnType(fparams[i].ParamType);
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

                auto closure = std::make_shared<ast::Closure>(c->location(), nullptr);

                // Create a pattern for destructuring the tuple
                std::vector<ast::PatternElement> patternElements;

                // First element is the result (use a unique name)
                patternElements.push_back(ast::PatternElement::makeSimple(c->location(), "__result", newDef.returnType().components()[0], false));

                // Subsequent elements are the mutable captures
                for (size_t i = 0; i < mutableCaptures.size(); ++i) {
                    const auto& varName = mutableCaptures[i];
                    // Get the type from the new return type components
                    const auto& type = newDef.returnType().components().at(i + 1);
                    patternElements.push_back(ast::PatternElement::makeSimple(c->location(), varName + "__new", type, false));
                }

                auto pattern = std::make_shared<ast::Pattern>(c->location(), patternElements);

                // Create a destructuring variable declaration
                auto varDecl = std::make_shared<ast::VariableDeclarationStatement>(c->location(), pattern, c);

                closure->addStatement(varDecl);

                // Add assignments to update the mutable variables
                for (size_t i = 0; i < mutableCaptures.size(); ++i) {
                    const auto& varName = mutableCaptures[i];

                    // Create assignment pattern
                    auto assignPattern = std::make_shared<ast::Pattern>(c->location(), std::vector<ast::PatternElement>{ ast::PatternElement::makeSimple(c->location(), varName, newDef.returnType().components()[i + 1], false) });

                    // Create variable expression for the new value
                    auto newValueExpr = std::make_shared<ast::VariableExpression>(c->location(), varName + "__new");
                    newValueExpr->setReturnType(newDef.returnType().components().at(i + 1));

                    // Create assignment statement
                    auto assignStmt = std::make_shared<ast::VariableAssignmentStatement>(c->location(), assignPattern, newValueExpr);

                    closure->addStatement(assignStmt);
                }

                // Set the closure's expression to return the original result
                auto resultExpr = std::make_shared<ast::VariableExpression>(c->location(), "__result");
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
            updateCallsInExpression(p, oldDef, newDef, mutableCaptures);

    } break;
    case ExpressionType::Unary: {
        const auto u = std::reinterpret_pointer_cast<UnaryExpression>(expr);
        updateCallsInExpression(u->innerMut(), oldDef, newDef, mutableCaptures);
    } break;
    case ExpressionType::Binary: {
        const auto b = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        updateCallsInExpression(b->leftMut(), oldDef, newDef, mutableCaptures);
        updateCallsInExpression(b->rightMut(), oldDef, newDef, mutableCaptures);
    } break;
    case ExpressionType::Swizzle: {
        const auto a = std::reinterpret_pointer_cast<SwizzleExpression>(expr);
        updateCallsInExpression(a->innerMut(), oldDef, newDef, mutableCaptures);
    } break;
    case ExpressionType::Access: {
        const auto a = std::reinterpret_pointer_cast<AccessExpression>(expr);
        updateCallsInExpression(a->innerMut(), oldDef, newDef, mutableCaptures);
    } break;
    case ExpressionType::Cast: {
        const auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        updateCallsInExpression(c->innerMut(), oldDef, newDef, mutableCaptures);
    } break;
    case ExpressionType::Tuple: {
        const auto v = std::reinterpret_pointer_cast<TupleExpression>(expr);
        for (auto& e : v->entries())
            updateCallsInExpression(e, oldDef, newDef, mutableCaptures);
    } break;
    case ExpressionType::Closure: {
        const auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        updateCallsInClosure(c->closure(), oldDef, newDef, mutableCaptures);
    } break;
    case ExpressionType::Branch: {
        const auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        updateCallsInClosure(br->elseClosure(), oldDef, newDef, mutableCaptures);
        for (auto& b : br->branches()) {
            updateCallsInExpression(b.Condition, oldDef, newDef, mutableCaptures);
            updateCallsInClosure(b.Body, oldDef, newDef, mutableCaptures);
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

void UpliftPass::collectMutableAssignmentsFromPattern(const Ptr<ast::Closure>& funcClosure, const Ptr<ast::Closure>& closure,
                                                      const Ptr<Pattern>& pattern, std::set<std::string>& mutableAssignments)
{
    if (!pattern)
        return;

    for (const auto& elem : pattern->elements()) {
        if (elem.isSimpleBinding()) {
            const auto& binding    = elem.simpleBinding();
            const SymbolTable* tbl = nullptr;
            if (auto def = closure->symbols().lookupVariable(elem.location(), binding.name, &tbl); def.has_value() && def->isMutable()) {
                // Go up the ladder until we find the function closure or global
                while (tbl && tbl != &funcClosure->symbols())
                    tbl = tbl->parent();

                if (!tbl) //< captured (above the function closure)
                    mutableAssignments.insert(binding.name);
            } else {
                mReporter.errorf(elem.location(), "Unknown variable '%s' found during uplift", binding.name.c_str());
            }
        } else {
            // Recursively traverse nested patterns
            collectMutableAssignmentsFromPattern(funcClosure, closure, elem.nestedPattern(), mutableAssignments);
        }
    }
}

Ptr<ast::Expression> UpliftPass::createReturnTuple(const Ptr<ast::Closure>& funcClosure,
                                                   const Ptr<ast::Expression>& originalReturnExpr,
                                                   const std::vector<std::string>& mutableCaptures)
{
    std::vector<Ptr<Expression>> tupleEntries;
    tupleEntries.push_back(originalReturnExpr);

    // Add variable expressions for each mutable capture
    for (const auto& varName : mutableCaptures) {
        auto varExpr = std::make_shared<VariableExpression>(originalReturnExpr->location(), varName);
        // Look up the type from the symbol table
        if (auto def = funcClosure->symbols().lookupVariable(originalReturnExpr->location(), varName); def.has_value())
            varExpr->setReturnType(def->type());
        tupleEntries.push_back(varExpr);
    }

    auto tupleExpr = std::make_shared<TupleExpression>(originalReturnExpr->location(), tupleEntries);

    // Set the return type to the tuple type
    std::vector<Type> tupleTypes;
    tupleTypes.push_back(originalReturnExpr->returnType());
    for (const auto& varName : mutableCaptures) {
        if (auto def = funcClosure->symbols().lookupVariable(originalReturnExpr->location(), varName); def.has_value())
            tupleTypes.push_back(def->type());
    }
    tupleExpr->setReturnType(Type(std::move(tupleTypes)));

    return tupleExpr;
}

void UpliftPass::updateCallsInClosure(const Ptr<Closure>& closure, const FunctionDef& oldDef, const FunctionDef& newDef,
                                      const std::vector<std::string>& mutableCaptures)
{
    // Update calls inside statements
    for (const auto& stmt : closure->statements()) {
        // For variable decl/assign and function decl bodies, inspect expressions
        if (stmt->type() == StatementType::VariableDeclaration) {
            auto declStmt = std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt);
            updateCallsInExpression(declStmt->expressionMut(), oldDef, newDef, mutableCaptures);
        } else if (stmt->type() == StatementType::VariableAssignment) {
            auto assignStmt = std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt);
            updateCallsInExpression(assignStmt->expressionMut(), oldDef, newDef, mutableCaptures);
        } else if (stmt->type() == StatementType::FunctionDeclaration) {
            const auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
            if (!f->isExtern())
                updateCallsInClosure(f->closure(), oldDef, newDef, mutableCaptures);
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
            updateCallsInClosure(std::reinterpret_pointer_cast<ClosureExpression>(expr)->closure(), oldDef, newDef, mutableCaptures);
        else
            updateCallsInExpression(closure->expressionMut(), oldDef, newDef, mutableCaptures);
    }
}

} // namespace PExpr::type
