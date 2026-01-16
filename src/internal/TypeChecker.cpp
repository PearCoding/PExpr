#include "TypeChecker.h"
#include "Definitions.h"
#include "Mangler.h"
#include "Reporter.h"

#include <algorithm>
#include <sstream>

namespace PExpr::internal {
inline void typeError(Reporter& rep, const Ptr<UnaryExpression>& expr, ElementaryType type)
{
    rep.errorf(expr->location(), "Can not use operator '%s' with type '%s'", toString(expr->op()).data(), toString(type).data());
}

inline void typeError(Reporter& rep, const Ptr<BinaryExpression>& expr, ElementaryType left, ElementaryType right)
{
    rep.errorf(expr->location(), "Can not use operator '%s' with types '%s' and '%s'", toString(expr->op()).data(), toString(left).data(), toString(right).data());
}

TypeChecker::TypeChecker(Reporter& reporter)
    : mReporter(reporter)
{
}

ElementaryType TypeChecker::handle(const Ptr<Closure>& closure)
{
    return handleNode(closure);
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure)
{
    // Preregister functions in this closure
    for (const auto& statement : closure->statements()) {
        if (statement->type() != StatementType::FunctionDeclaration)
            continue;

        const auto funcStmt = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(statement);

        // Pre-register a provisional function definition
        if (!closure->symbols().addFunction(FunctionDef(funcStmt->name(), funcStmt->mangledName(), funcStmt->parameters(), funcStmt->returnType(), funcStmt->isExtern())))
            mReporter.errorf(funcStmt->location(), "Function '%s' already defined in the current scope", funcStmt->name().c_str());
    }

    for (const auto& statement : closure->statements())
        handleNode(closure, statement);

    const ElementaryType type = handleNode(closure, closure->expression());
    closure->expression()->setReturnType(type);
    return type;
}

void TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<Statement>& statement)
{
    switch (statement->type()) {
    case StatementType::VariableDeclaration: {
        const auto varStmt = std::reinterpret_pointer_cast<VariableDeclarationStatement>(statement);

        // Type-check the initializer expression first.
        const auto type = handleNode(closure, varStmt->expression());
        if (type == ElementaryType::Error)
            return; // Error was caught somewhere else

        if (type == ElementaryType::Unspecified) {
            mReporter.errorf(varStmt->location(), "Can not determine type of variable '%s'", varStmt->name().c_str());
            return;
        }

        // If an explicit declared type is provided, validate / coerce the initializer.
        const auto declared = varStmt->declaredType();
        if (declared != ElementaryType::Unspecified && type != declared) {
            if (isConvertible(type, declared)) {
                // Insert an implicit (non-explicit) cast so downstream passes see an explicit cast node.
                const auto orig     = varStmt->expression();
                const auto castExpr = std::make_shared<CastExpression>(orig->location(), declared, orig, false);

                // Special case: `int` literal for a `num` variable
                if (varStmt->expression()->type() == ExpressionType::Literal && declared == ElementaryType::Number && type == ElementaryType::Integer) {
                    // Ignore warning
                } else {
                    mReporter.warningf(RT_WARNING_IMPLICIT_CAST, orig->location(), "Implicitly converting from '%s' to '%s' for variable '%s'", toString(type).data(), toString(declared).data(), varStmt->name().c_str());
                }

                varStmt->replaceExpression(castExpr);
            } else {
                mReporter.errorf(varStmt->location(), "Cannot implicitly convert initializer from '%s' to declared type '%s' for variable '%s'", toString(type).data(), toString(declared).data(), varStmt->name().c_str());
                return;
            }
        }

        // Register the variable
        if (!closure->symbols().addVariable(VariableDef(varStmt->name(), declared != ElementaryType::Unspecified ? declared : type, varStmt->isMutable())))
            mReporter.errorf(varStmt->location(), "New variable '%s' already exists in the current scope", varStmt->name().c_str());
    } break;
    case StatementType::VariableAssignment: {
        const auto varStmt = std::reinterpret_pointer_cast<VariableAssignmentStatement>(statement);
        const auto type    = handleNode(closure, varStmt->expression());
        if (type == ElementaryType::Error)
            return; // Error was caught somewhere else

        // Check if the variable exists and can be updated
        const SymbolTable* capturedTbl;
        const auto var = closure->symbols().lookupVariable(varStmt->location(), varStmt->name(), &capturedTbl);
        if (!var.has_value()) {
            mReporter.errorf(varStmt->location(), "Trying to assign a value to unknown variable '%s'", varStmt->name().c_str());
            return;
        }
        if (capturedTbl != &closure->symbols()) {
            mReporter.errorf(varStmt->location(), "Trying to reassign a value to variable '%s' defined in a different scope", varStmt->name().c_str());
            return;
        }
        if (!var->isMutable()) {
            mReporter.errorf(varStmt->location(), "Trying to reassign a value to constant variable '%s'", varStmt->name().c_str());
            return;
        }

        const auto declared = var->type();
        if (isConvertible(type, declared) && type != declared) {
            // Insert an implicit (non-explicit) cast so downstream passes see an explicit cast node.
            const auto orig     = varStmt->expression();
            const auto castExpr = std::make_shared<CastExpression>(orig->location(), declared, orig, false);

            // Special case: `int` literal for a `num` variable
            if (varStmt->expression()->type() == ExpressionType::Literal && declared == ElementaryType::Number && type == ElementaryType::Integer) {
                // Ignore warning
            } else {
                mReporter.warningf(RT_WARNING_IMPLICIT_CAST, orig->location(), "Implicitly converting from '%s' to '%s' for variable '%s'", toString(type).data(), toString(declared).data(), varStmt->name().c_str());
            }

            varStmt->replaceExpression(castExpr);
        } else if (type != declared) {
            mReporter.errorf(varStmt->location(), "Cannot implicitly convert from '%s' to declared type '%s' for variable '%s'", toString(type).data(), toString(declared).data(), varStmt->name().c_str());
        }
    } break;
    case StatementType::FunctionDeclaration: {
        const auto funcStmt = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(statement);

        // Add parameters to the symbol table
        if (funcStmt->closure()) {
            for (const auto& p : funcStmt->parameters()) {
                if (!funcStmt->closure()->symbols().addVariable(VariableDef(p.Name, p.Type, false))) //< TODO: Really non-mutable?
                    mReporter.errorf(funcStmt->location(), "Parameter '%s' already exists in the current scope", p.Name.c_str());
            }

            PEXPR_ASSERT(funcStmt->closure()->symbols().parent() == &closure->symbols(), "Invalid parent relationship");
        }

        // Type-check the function body to determine the return type
        const auto returnType = funcStmt->isExtern() ? funcStmt->returnType() : handleNode(funcStmt->closure());
        funcStmt->setReturnType(returnType);

        if (returnType == ElementaryType::Unspecified) {
            mReporter.errorf(funcStmt->location(), "Could not determine return type for function '%s'", funcStmt->name().c_str());
            return;
        }

        if (!funcStmt->isExtern())
            closure->symbols().replaceFunction(FunctionDef(funcStmt->name(), funcStmt->mangledName(), funcStmt->parameters(), returnType, funcStmt->isExtern()));

        const auto declared = funcStmt->returnType();
        if (isConvertible(returnType, declared) && returnType != declared) {
            // Insert an implicit (non-explicit) cast so downstream passes see an explicit cast node.
            const auto orig     = funcStmt->closure()->expression();
            const auto castExpr = std::make_shared<CastExpression>(orig->location(), declared, orig, false);

            // Special case: `int` literal for a `num` variable
            if (funcStmt->closure()->expression()->type() == ExpressionType::Literal && declared == ElementaryType::Number && returnType == ElementaryType::Integer) {
                // Ignore warning
            } else {
                mReporter.warningf(RT_WARNING_IMPLICIT_CAST, orig->location(), "Implicitly converting return value from '%s' to '%s' for function '%s'", toString(returnType).data(), toString(declared).data(), funcStmt->name().c_str());
            }

            funcStmt->closure()->replaceExpression(castExpr);
        } else if (returnType != declared) {
            mReporter.errorf(funcStmt->location(), "Cannot implicitly convert return value from '%s' to declared type '%s' for function '%s'", toString(returnType).data(), toString(declared).data(), funcStmt->name().c_str());
        }
    } break;
    default:
        break;
    }
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<Expression>& expr)
{
    switch (expr->type()) {
    case ExpressionType::Variable:
        return handleNode(closure, std::reinterpret_pointer_cast<VariableExpression>(expr));
    case ExpressionType::Literal:
        return handleNode(closure, std::reinterpret_pointer_cast<LiteralExpression>(expr));
    case ExpressionType::Unary:
        return handleNode(closure, std::reinterpret_pointer_cast<UnaryExpression>(expr));
    case ExpressionType::Binary:
        return handleNode(closure, std::reinterpret_pointer_cast<BinaryExpression>(expr));
    case ExpressionType::Call:
        return handleNode(closure, std::reinterpret_pointer_cast<CallExpression>(expr));
    case ExpressionType::Access:
        return handleNode(closure, std::reinterpret_pointer_cast<AccessExpression>(expr));
    case ExpressionType::Vector:
        return handleNode(closure, std::reinterpret_pointer_cast<VectorExpression>(expr));
    case ExpressionType::Cast:
        return handleNode(closure, std::reinterpret_pointer_cast<CastExpression>(expr));
    case ExpressionType::Closure:
        return handleNode(closure, std::reinterpret_pointer_cast<ClosureExpression>(expr));
    case ExpressionType::Branch:
        return handleNode(closure, std::reinterpret_pointer_cast<BranchExpression>(expr));
    default:
        PEXPR_ASSERT(false, "Unhandled expression type");
        return ElementaryType::Error;
    }
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<ClosureExpression>& expr)
{
    PEXPR_UNUSED(closure);
    PEXPR_ASSERT(expr->closure()->symbols().parent() == &closure->symbols(), "Invalid parent relationship");

    ElementaryType type = handleNode(expr->closure());
    expr->setReturnType(type);
    return type;
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<BranchExpression>& expr)
{
    ElementaryType returnType = handleNode(expr->elseClosure());
    if (returnType == ElementaryType::Error) // Error handled somewhere else
        return ElementaryType::Error;

    for (const auto& branch : expr->branches()) {
        const ElementaryType conditionType = handleNode(closure, branch.Condition);
        if (isConvertible(conditionType, ElementaryType::Boolean)) {
            branch.Condition->setReturnType(ElementaryType::Boolean);
        } else {
            mReporter.error(branch.Condition->location(), "Expected condition to evaluate to bool");
            return ElementaryType::Error;
        }

        const ElementaryType bodyType = handleNode(branch.Body);
        if (bodyType == ElementaryType::Error) // Error handled somewhere else
            return ElementaryType::Error;

        if (returnType == ElementaryType::Unspecified) {
            returnType = bodyType;
        } else if (bodyType != returnType && isConvertible(bodyType, returnType)) {
            // Inject a CastExpression so the branch body expression has the desired return type.
            // This ensures later stages (SSA mapper) see an explicit cast node rather than relying
            // on the mapper to insert SSA-level casts.
            auto origExpr = branch.Body->expression();
            auto castExpr = std::make_shared<CastExpression>(origExpr->location(), returnType, origExpr);
            branch.Body->replaceExpression(castExpr);

            // Special case: `int` literal for a `num` branch
            if (branch.Body->expression()->type() == ExpressionType::Literal && returnType == ElementaryType::Number && bodyType == ElementaryType::Integer) {
                // Ignore warning
            } else {
                mReporter.warningf(RT_WARNING_IMPLICIT_CAST, origExpr->location(), "Implicitly converting from '%s' to '%s' for conditional branch", toString(bodyType).data(), toString(returnType).data());
            }
        } else if (bodyType == returnType) {
            // matching type — nothing to do
        } else {
            mReporter.errorf(branch.Condition->location(), "Expected all branch bodies to evaluate to the type '%s'", toString(returnType).data());
            return ElementaryType::Error;
        }
    }

    expr->setReturnType(returnType);
    return returnType;
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<VariableExpression>& expr)
{
    if (const auto def = closure->symbols().lookupVariable(expr->location(), expr->name()); def.has_value()) {
        expr->setReturnType(def.value().type());
        return def.value().type();
    } else {
        mReporter.errorf(expr->location(), "Unknown identifier '%s' found", expr->name().c_str());
        return ElementaryType::Error;
    }
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<LiteralExpression>& expr)
{
    PEXPR_UNUSED(closure);
    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<UnaryExpression>& expr)
{
    auto innerType = handleNode(closure, expr->inner());
    if (innerType == ElementaryType::Error)
        return innerType; // Error was caught somewhere else

    expr->setReturnType(ElementaryType::Unspecified);

    switch (expr->op()) {
    case UnaryOperation::Pos:
    case UnaryOperation::Neg:
        if (isArithmetic(innerType))
            expr->setReturnType(innerType);
        break;
    case UnaryOperation::Not:
        if (isConvertible(innerType, ElementaryType::Boolean))
            expr->setReturnType(ElementaryType::Boolean);
        break;
    default:
        break;
    }

    if (expr->isUnspecified()) {
        typeError(mReporter, expr, innerType);
        return ElementaryType::Error;
    }

    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<BinaryExpression>& expr)
{
    auto leftType  = handleNode(closure, expr->left());
    auto rightType = handleNode(closure, expr->right());
    if (leftType == ElementaryType::Error || rightType == ElementaryType::Error)
        return ElementaryType::Error; // Error was caught somewhere else

    expr->setReturnType(ElementaryType::Unspecified);

    switch (expr->op()) {
    case BinaryOperation::Add:
    case BinaryOperation::Sub:
        if (isArithmetic(leftType) && isArithmetic(rightType)) {
            if (leftType == rightType)
                expr->setReturnType(leftType);
            else if (isConvertible(leftType, rightType))
                expr->setReturnType(rightType);
            else if (isConvertible(rightType, leftType))
                expr->setReturnType(leftType);
        }
        break;
    case BinaryOperation::Mul:
    case BinaryOperation::Div:
        if (isArithmetic(leftType) && isArithmetic(rightType)) {
            if (leftType == rightType)
                expr->setReturnType(leftType);
            else if (isConvertible(leftType, rightType))
                expr->setReturnType(rightType);
            else if (isConvertible(rightType, leftType))
                expr->setReturnType(leftType);
            else if (isArray(leftType) && isConvertible(rightType, ElementaryType::Number))
                expr->setReturnType(leftType); // vec * f, vec / f
            else if (expr->op() != BinaryOperation::Div && isArray(rightType) && isConvertible(leftType, ElementaryType::Number))
                expr->setReturnType(rightType); // f * vec
        }
        break;
    case BinaryOperation::Pow:
        if (isArithmetic(leftType) && isArithmetic(rightType)) {
            if (leftType == rightType && leftType == ElementaryType::Integer)
                expr->setReturnType(leftType); // i ^ i
            else if (isConvertible(leftType, ElementaryType::Number) && isConvertible(rightType, ElementaryType::Number))
                expr->setReturnType(ElementaryType::Number); // f ^ f
            else if (isArray(leftType) && isConvertible(rightType, ElementaryType::Number))
                expr->setReturnType(leftType); // vec ^ f
        }
        break;
    case BinaryOperation::Mod:
        if (isConvertible(leftType, ElementaryType::Integer) && isConvertible(rightType, ElementaryType::Integer))
            expr->setReturnType(ElementaryType::Integer); // i % i
        break;
    case BinaryOperation::And:
    case BinaryOperation::Or:
        if (isConvertible(leftType, ElementaryType::Boolean) && isConvertible(rightType, ElementaryType::Boolean))
            expr->setReturnType(ElementaryType::Boolean);
        break;
    case BinaryOperation::Less:
    case BinaryOperation::Greater:
    case BinaryOperation::LessEqual:
    case BinaryOperation::GreaterEqual:
        if (isConvertible(leftType, ElementaryType::Boolean) && isConvertible(rightType, ElementaryType::Boolean))
            expr->setReturnType(ElementaryType::Boolean);
        else if (isConvertible(leftType, ElementaryType::Number) && isConvertible(rightType, ElementaryType::Number))
            expr->setReturnType(ElementaryType::Boolean);
        break;
    case BinaryOperation::Equal:
    case BinaryOperation::NotEqual:
        if (isConvertible(leftType, rightType) || isConvertible(rightType, leftType))
            expr->setReturnType(ElementaryType::Boolean);
        break;
    default:
        break;
    }

    if (expr->isUnspecified()) {
        typeError(mReporter, expr, leftType, rightType);
        return ElementaryType::Error;
    }

    return expr->returnType();
}

inline std::string printArgs(const std::vector<ElementaryType>& args)
{
    std::stringstream stream;

    for (size_t i = 0; i < args.size(); ++i) {
        stream << toString(args[i]);
        if (i != args.size() - 1)
            stream << ", ";
    }

    return stream.str();
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<CallExpression>& expr)
{
    std::vector<ElementaryType> fromArgs;
    fromArgs.reserve(expr->parameters().size());

    // First, type-check arguments to obtain their types.
    for (size_t i = 0; i < expr->parameters().size(); ++i) {
        auto type = handleNode(closure, expr->parameters().at(i));
        if (type == ElementaryType::Error)
            return ElementaryType::Error; // Error was caught somewhere else
        fromArgs.push_back(type);
    }

    expr->setReturnType(ElementaryType::Unspecified);

    // Lookup the function (this allows matching with implicit convertible args)
    if (const auto def = closure->symbols().lookupFunction(expr->location(), expr->name(), fromArgs); def.has_value()) {
        // For any parameter where the actual type differs from the parameter type
        // and an implicit conversion exists, inject an implicit CastExpression
        // (explicit=false) so downstream passes see an explicit cast node.
        const auto& pList = def.value().parameters();
        for (size_t i = 0; i < expr->parameters().size() && i < pList.size(); ++i) {
            const ElementaryType desired = pList[i].Type;
            const ElementaryType actual  = fromArgs[i];
            if (actual != desired) {
                if (isConvertible(actual, desired)) {
                    auto original = expr->parameters().at(i);
                    auto castExpr = std::make_shared<CastExpression>(original->location(), desired, original, false);
                    expr->replaceParameter(i, castExpr);
                    fromArgs[i] = desired;

                    // Special case: `int` literal for a `num` parameter
                    if (original->type() == ExpressionType::Literal && desired == ElementaryType::Number && actual == ElementaryType::Integer) {
                        // Ignore warning
                    } else {
                        mReporter.warningf(RT_WARNING_IMPLICIT_CAST, original->location(), "Implicitly converting from '%s' to '%s' for function parameter %zu", toString(actual).data(), toString(desired).data(), i);
                    }
                } else {
                    mReporter.errorf(expr->parameters().at(i)->location(), "Cannot implicitly convert from '%s' to '%s' for function parameter %zu", toString(actual).data(), toString(desired).data(), i);
                    return ElementaryType::Error;
                }
            }
        }

        expr->setReturnType(def.value().returnType());
        expr->setMangledName(def->mangledName());
    } else {
        mReporter.errorf(expr->location(), "Function '%s(%s)' is unknown or ambiguous", expr->name().c_str(), printArgs(fromArgs).c_str());
        return ElementaryType::Error;
    }

    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<AccessExpression>& expr)
{
    auto innerType = handleNode(closure, expr->inner());
    if (innerType == ElementaryType::Error)
        return innerType; // Error was caught somewhere else

    expr->setReturnType(ElementaryType::Unspecified);

    // The access operator also allows expanding e.g., vec2.xyxy -> vec4 operations
    if (isArray(innerType)) {
        const auto& swizzle = expr->swizzle();

        size_t vec_size = 2;
        if (innerType == ElementaryType::Vec3)
            vec_size = 3;
        if (innerType == ElementaryType::Vec4)
            vec_size = 4;

        bool isValid = true;
        for (char c : swizzle) {
            isValid = (c == 'x' || c == 'r'
                       || c == 'y' || c == 'g'
                       || (vec_size > 2 && c == 'z') || (vec_size > 2 && c == 'b')
                       || (vec_size > 3 && c == 'w') || (vec_size > 3 && c == 'a'));

            if (!isValid)
                break;
        }

        PEXPR_ASSERT(swizzle.size() > 0, "Expected at least a single component");
        if (!isValid) {
            mReporter.errorf(expr->location(), "Invalid access components '%s' given", std::string(swizzle).c_str());
            return ElementaryType::Error;
        } else {
            switch (swizzle.size()) {
            case 1:
                expr->setReturnType(ElementaryType::Number);
                break;
            case 2:
                expr->setReturnType(ElementaryType::Vec2);
                break;
            case 3:
                expr->setReturnType(ElementaryType::Vec3);
                break;
            case 4:
                expr->setReturnType(ElementaryType::Vec4);
                break;
            default:
                mReporter.errorf(expr->location(), "Expected a maximum of 4 components but got %zu", swizzle.size());
                return ElementaryType::Error;
            }
        }
    } else {
        mReporter.errorf(expr->location(), "Access operator is only defined for vector types");
        return ElementaryType::Error;
    }

    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<VectorExpression>& expr)
{
    // Ensure each entry is type-checked and, if necessary, inject an implicit
    // CastExpression to Number so downstream passes (SSA) see explicit casts.
    for (size_t i = 0; i < expr->entries().size(); ++i) {
        auto orig        = expr->entries().at(i);
        const auto pType = handleNode(closure, orig);
        if (pType == ElementaryType::Error)
            return ElementaryType::Error; // Error handled somewhere else

        if (pType == ElementaryType::Number)
            continue;

        // Allow implicit conversion to Number (e.g. Integer -> Number) by injecting a cast.
        if (isConvertible(pType, ElementaryType::Number)) {
            // Special case: `int` literal for a `num` parameter
            if (orig->type() == ExpressionType::Literal && pType == ElementaryType::Integer) {
                // Ignore warning
            } else {
                mReporter.warningf(RT_WARNING_IMPLICIT_CAST, orig->location(), "Implicitly converting from '%s' to '%s' for vector parameter %zu", toString(pType).data(), toString(ElementaryType::Number).data(), i);
            }

            auto castExpr = std::make_shared<CastExpression>(orig->location(), ElementaryType::Number, orig, false);
            expr->replaceEntry(i, castExpr);
            // We don't need to update pType variable; CastExpression will report Number when type-checked later.
            continue;
        }

        mReporter.errorf(orig->location(), "Expected vector values to be convertible to '%s'", toString(ElementaryType::Number).data());
        return ElementaryType::Error;
    }

    ElementaryType type;
    switch (expr->entries().size()) {
    case 2:
        type = ElementaryType::Vec2;
        break;
    case 3:
        type = ElementaryType::Vec3;
        break;
    case 4:
        type = ElementaryType::Vec4;
        break;
    default:
        return ElementaryType::Error; // Should be caught somewhere else
    }
    expr->setReturnType(type);
    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<CastExpression>& expr)
{
    // Type-check inner expression first
    const ElementaryType innerType = handleNode(closure, expr->inner());
    if (innerType == ElementaryType::Error)
        return innerType; // Error was reported deeper

    // Validate allowed conversion depending on whether the cast is explicit or implicit
    if (expr->isExplicit()) {
        if (!isExplicitConvertible(innerType, expr->toType())) {
            mReporter.errorf(expr->location(), "Cannot cast from '%s' to '%s'", toString(innerType).data(), toString(expr->toType()).data());
            return ElementaryType::Error;
        }
    } else {
        if (!isConvertible(innerType, expr->toType())) {
            mReporter.errorf(expr->location(), "Implicit conversion from '%s' to '%s' is not allowed", toString(innerType).data(), toString(expr->toType()).data());
            return ElementaryType::Error;
        }
    }

    expr->setReturnType(expr->toType());
    return expr->returnType();
}
} // namespace PExpr::internal
