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

TypeChecker::TypeChecker(const SymbolTable& defs, Reporter& reporter)
    : mDefinitions(defs)
    , mReporter(reporter)
{
}

ElementaryType TypeChecker::handle(const Ptr<Closure>& closure)
{
    mDynamicDefinitions = SymbolTable(&mDefinitions);
    return handleNode(closure);
}

ElementaryType TypeChecker::handleNode(const Ptr<Closure>& closure)
{
    for (auto statement : closure->statements())
        handleNode(statement);
    return handleNode(closure->expression());
}

void TypeChecker::handleNode(const Ptr<Statement>& statement)
{
    switch (statement->type()) {
    case StatementType::VariableDeclaration: {
        auto varStmt = std::reinterpret_pointer_cast<VariableDeclarationStatement>(statement);

        // Type-check the initializer expression first.
        auto type = handleNode(varStmt->expression());
        if (type == ElementaryType::Unspecified)
            return; // Error was caught somewhere else

        // If an explicit declared type is provided, validate / coerce the initializer.
        const auto declared = varStmt->declaredType();
        if (declared != ElementaryType::Unspecified) {
            if (type != declared) {
                if (isConvertible(type, declared)) {
                    // Insert an implicit (non-explicit) cast so downstream passes see an explicit cast node.
                    auto orig     = varStmt->expression();
                    auto castExpr = std::make_shared<CastExpression>(orig->location(), declared, orig, false);
                    varStmt->replaceExpression(castExpr);
                    type = declared;

                    mReporter.warningf(RT_WARNING_IMPLICIT_CAST, orig->location(), "Implicitly converting from '%s' to '%s' for variable '%s'", toString(type).data(), toString(declared).data(), varStmt->name().c_str());
                } else {
                    mReporter.errorf(varStmt->location(), "Cannot implicitly convert initializer from '%s' to declared type '%s' for variable '%s'", toString(type).data(), toString(declared).data(), varStmt->name().c_str());
                    return;
                }
            }
        }

        // Register the variable in the dynamic symbol table so following statements
        // and expressions can resolve it. Use the declared type if present, otherwise the inferred type.
        const bool is_ok = mDynamicDefinitions.addVariable(VariableDef(varStmt->name(), declared != ElementaryType::Unspecified ? declared : type, varStmt->isMutable()));
        if (!is_ok) {
            mReporter.errorf(varStmt->location(), "Trying to declare a new variable '%s'", varStmt->name().c_str());
            return;
        }
    } break;
    case StatementType::VariableAssignment: {
        auto varStmt = std::reinterpret_pointer_cast<VariableAssignmentStatement>(statement);
        auto type    = handleNode(varStmt->expression());
        if (type == ElementaryType::Unspecified)
            return; // Error was caught somewhere else

        // Check if the variable exists and can be updated
        const auto var = mDynamicDefinitions.lookupVariable(varStmt->location(), varStmt->name());
        if (!var.has_value()) {
            mReporter.errorf(varStmt->location(), "Trying to assign a value to unknown variable '%s'", varStmt->name().c_str());
            return;
        }
        if (!var->isMutable()) {
            mReporter.errorf(varStmt->location(), "Trying to reassign a value to constant variable '%s'", varStmt->name().c_str());
            return;
        }
    } break;
    case StatementType::FunctionDeclaration: {
        auto funcStmt = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(statement);

        // Collect parameter types (may include ElementaryType::Unspecified)
        std::vector<ElementaryType> paramTypes;
        paramTypes.reserve(funcStmt->parameters().size());
        for (const auto& p : funcStmt->parameters())
            paramTypes.push_back(p.Type);

        // Pre-register a provisional function definition so the function name is visible
        // inside its own body (allows recursion). For extern functions we register the
        // final signature immediately.
        if (funcStmt->isExtern()) {
            // extern must provide a concrete return type
            mDynamicDefinitions.replaceFunction(FunctionDef(funcStmt->name(), funcStmt->mangledName(), paramTypes, funcStmt->returnType(), funcStmt->isExtern()));
        } else {
            // register with unspecified return type to allow recursive calls
            mDynamicDefinitions.replaceFunction(FunctionDef(funcStmt->name(), funcStmt->mangledName(), paramTypes, ElementaryType::Unspecified, funcStmt->isExtern()));
        }

        // Temporarily expose parameters as variables (only when they have a specified type)
        auto savedDefs = mDynamicDefinitions;
        for (const auto& p : funcStmt->parameters()) {
            if (p.Type == ElementaryType::Unspecified)
                continue; // cannot register a parameter without a type
            mDynamicDefinitions.addVariable(VariableDef(p.Name, p.Type, false));
        }

        // Type-check the function body to determine the return type
        const auto returnType = funcStmt->isExtern() ? funcStmt->returnType() : handleNode(funcStmt->expression());

        // Restore dynamic definitions (provisional function remains in savedDefs)
        mDynamicDefinitions = std::move(savedDefs);

        if (returnType == ElementaryType::Unspecified) {
            mReporter.errorf(funcStmt->location(), "Could not determine return type for function '%s'", funcStmt->name().c_str());
            return;
        }

        if (const auto explicitReturnType = funcStmt->returnType(); returnType != explicitReturnType) {
            mReporter.errorf(funcStmt->location(), "Given explicit return type '%s' in function '%s' does not match the return type '%s' of the defining expression", toString(explicitReturnType).data(), funcStmt->name().c_str(), toString(returnType).data());
            return;
        }

        // Replace provisional registration with the final signature (or add if missing)
        if (!mDynamicDefinitions.replaceFunction(FunctionDef(funcStmt->name(), funcStmt->mangledName(), std::move(paramTypes), returnType, funcStmt->isExtern()))) {
            mReporter.errorf(funcStmt->location(), "Given function '%s' is already defined", funcStmt->name().c_str());
        }
    } break;
    default:
        break;
    }
}

ElementaryType TypeChecker::handleNode(const Ptr<Expression>& expr)
{
    switch (expr->type()) {
    case ExpressionType::Variable:
        return handleNode(std::reinterpret_pointer_cast<VariableExpression>(expr));
    case ExpressionType::Literal:
        return handleNode(std::reinterpret_pointer_cast<LiteralExpression>(expr));
    case ExpressionType::Unary:
        return handleNode(std::reinterpret_pointer_cast<UnaryExpression>(expr));
    case ExpressionType::Binary:
        return handleNode(std::reinterpret_pointer_cast<BinaryExpression>(expr));
    case ExpressionType::Call:
        return handleNode(std::reinterpret_pointer_cast<CallExpression>(expr));
    case ExpressionType::Access:
        return handleNode(std::reinterpret_pointer_cast<AccessExpression>(expr));
    case ExpressionType::Vector:
        return handleNode(std::reinterpret_pointer_cast<VectorExpression>(expr));
    case ExpressionType::Cast:
        return handleNode(std::reinterpret_pointer_cast<CastExpression>(expr));
    case ExpressionType::Closure:
        return handleNode(std::reinterpret_pointer_cast<ClosureExpression>(expr));
    case ExpressionType::Branch:
        return handleNode(std::reinterpret_pointer_cast<BranchExpression>(expr));
    default:
        return ElementaryType::Unspecified;
    }
}

ElementaryType TypeChecker::handleNode(const Ptr<ClosureExpression>& expr)
{
    auto def            = mDynamicDefinitions;
    ElementaryType type = handleNode(expr->closure());
    expr->setReturnType(type);
    mDynamicDefinitions = std::move(def);
    return type;
}

ElementaryType TypeChecker::handleNode(const Ptr<BranchExpression>& expr)
{
    ElementaryType returnType = handleNode(expr->elseClosure());

    for (const auto& branch : expr->branches()) {
        const ElementaryType conditionType = handleNode(branch.Condition);
        if (isConvertible(conditionType, ElementaryType::Boolean)) {
            branch.Condition->setReturnType(ElementaryType::Boolean);
        } else {
            mReporter.error(branch.Condition->location(), "Expected condition to evaluate to bool");
            return ElementaryType::Unspecified;
        }

        const ElementaryType bodyType = handleNode(branch.Body);

        if (returnType == ElementaryType::Unspecified) {
            returnType = bodyType;
        } else if (bodyType != returnType && isConvertible(bodyType, returnType)) {
            // Inject a CastExpression so the branch body expression has the desired return type.
            // This ensures later stages (SSA mapper) see an explicit cast node rather than relying
            // on the mapper to insert SSA-level casts.
            auto origExpr = branch.Body->expression();
            auto castExpr = std::make_shared<CastExpression>(origExpr->location(), returnType, origExpr);
            branch.Body->replaceExpression(castExpr);

            mReporter.warningf(RT_WARNING_IMPLICIT_CAST, origExpr->location(), "Implicitly converting from '%s' to '%s' for if branch", toString(bodyType).data(), toString(returnType).data());
        } else if (bodyType == returnType) {
            // matching type — nothing to do
        } else {
            mReporter.errorf(branch.Condition->location(), "Expected all branch bodies to evaluate to the type '%s'", toString(returnType).data());
            return ElementaryType::Unspecified;
        }
    }

    expr->setReturnType(returnType);
    return returnType;
}

ElementaryType TypeChecker::handleNode(const Ptr<VariableExpression>& expr)
{
    if (const auto def = mDynamicDefinitions.lookupVariable(expr->location(), expr->name()); def.has_value()) {
        expr->setReturnType(def.value().type());
        return def.value().type();
    } else {
        mReporter.errorf(expr->location(), "Unknown identifier '%s' found", expr->name().c_str());
        return ElementaryType::Unspecified;
    }
}

ElementaryType TypeChecker::handleNode(const Ptr<LiteralExpression>& expr)
{
    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<UnaryExpression>& expr)
{
    auto innerType = handleNode(expr->inner());
    if (innerType == ElementaryType::Unspecified)
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

    if (expr->isUnspecified())
        typeError(mReporter, expr, innerType);

    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<BinaryExpression>& expr)
{
    auto leftType  = handleNode(expr->left());
    auto rightType = handleNode(expr->right());
    if (leftType == ElementaryType::Unspecified || rightType == ElementaryType::Unspecified)
        return rightType; // Error was caught somewhere else

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

    if (expr->isUnspecified())
        typeError(mReporter, expr, leftType, rightType);

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

ElementaryType TypeChecker::handleNode(const Ptr<CallExpression>& expr)
{
    std::vector<ElementaryType> fromArgs;
    fromArgs.reserve(expr->parameters().size());

    // First, type-check arguments to obtain their types.
    for (size_t i = 0; i < expr->parameters().size(); ++i) {
        auto type = handleNode(expr->parameters().at(i));
        if (type == ElementaryType::Unspecified)
            return ElementaryType::Unspecified; // Error was caught somewhere else
        fromArgs.push_back(type);
    }

    expr->setReturnType(ElementaryType::Unspecified);

    // Lookup the function (this allows matching with implicit convertible args)
    if (const auto def = mDynamicDefinitions.lookupFunction(expr->location(), expr->name(), fromArgs); def.has_value()) {
        // For any parameter where the actual type differs from the parameter type
        // and an implicit conversion exists, inject an implicit CastExpression
        // (explicit=false) so downstream passes see an explicit cast node.
        const auto& paramTypes = def->parameters();
        for (size_t i = 0; i < expr->parameters().size() && i < paramTypes.size(); ++i) {
            const ElementaryType desired = paramTypes[i];
            const ElementaryType actual  = fromArgs[i];
            if (actual != desired) {
                if (isConvertible(actual, desired)) {
                    auto original = expr->parameters().at(i);
                    auto castExpr = std::make_shared<CastExpression>(original->location(), desired, original, false);
                    expr->replaceParameter(i, castExpr);
                    fromArgs[i] = desired;

                    mReporter.warningf(RT_WARNING_IMPLICIT_CAST, original->location(), "Implicitly converting from '%s' to '%s' for function parameter %zu", toString(actual).data(), toString(desired).data(), i);
                } else {
                    mReporter.errorf(expr->parameters().at(i)->location(), "Cannot implicitly convert from '%s' to '%s' for function parameter %zu", toString(actual).data(), toString(desired).data(), i);
                    return ElementaryType::Unspecified;
                }
            }
        }

        expr->setReturnType(def.value().returnType());
        expr->setMangledName(def->mangledName());
    } else {
        mReporter.errorf(expr->location(), "Function '%s(%s)' is unknown or ambiguous", expr->name().c_str(), printArgs(fromArgs).c_str());
    }

    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<AccessExpression>& expr)
{
    auto innerType = handleNode(expr->inner());
    if (innerType == ElementaryType::Unspecified)
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
                break;
            }
        }
    } else {
        mReporter.errorf(expr->location(), "Access operator is only defined for vector types");
    }

    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<VectorExpression>& expr)
{
    // Ensure each entry is type-checked and, if necessary, inject an implicit
    // CastExpression to Number so downstream passes (SSA) see explicit casts.
    for (size_t i = 0; i < expr->entries().size(); ++i) {
        auto orig        = expr->entries().at(i);
        const auto pType = handleNode(orig);
        if (pType == ElementaryType::Unspecified)
            return ElementaryType::Unspecified; // Error handled somewhere else

        if (pType == ElementaryType::Number)
            continue;

        // Allow implicit conversion to Number (e.g. Integer -> Number) by injecting a cast.
        if (isConvertible(pType, ElementaryType::Number)) {
            mReporter.warningf(RT_WARNING_IMPLICIT_CAST, orig->location(), "Implicitly converting from '%s' to '%s' for vector parameter %zu", toString(pType).data(), toString(ElementaryType::Number).data(), i);

            auto castExpr = std::make_shared<CastExpression>(orig->location(), ElementaryType::Number, orig, false);
            expr->replaceEntry(i, castExpr);
            // We don't need to update pType variable; CastExpression will report Number when type-checked later.
            continue;
        }

        mReporter.errorf(orig->location(), "Expected vector values to be convertible to '%s'", toString(ElementaryType::Number).data());
        return ElementaryType::Unspecified;
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
        return ElementaryType::Unspecified; // Should be caught somewhere else
    }
    expr->setReturnType(type);
    return expr->returnType();
}

ElementaryType TypeChecker::handleNode(const Ptr<CastExpression>& expr)
{
    // Type-check inner expression first
    const ElementaryType innerType = handleNode(expr->inner());
    if (innerType == ElementaryType::Unspecified)
        return innerType; // Error was reported deeper

    // Validate allowed conversion depending on whether the cast is explicit or implicit
    if (expr->isExplicit()) {
        if (!isExplicitConvertible(innerType, expr->toType())) {
            mReporter.errorf(expr->location(), "Cannot cast from '%s' to '%s'", toString(innerType).data(), toString(expr->toType()).data());
            return ElementaryType::Unspecified;
        }
    } else {
        if (!isConvertible(innerType, expr->toType())) {
            mReporter.errorf(expr->location(), "Implicit conversion from '%s' to '%s' is not allowed", toString(innerType).data(), toString(expr->toType()).data());
            return ElementaryType::Unspecified;
        }
    }

    expr->setReturnType(expr->toType());
    return expr->returnType();
}
} // namespace PExpr::internal
