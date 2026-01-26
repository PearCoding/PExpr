#include "TypeChecker.h"
#include "Definitions.h"
#include "Mangler.h"
#include "Reporter.h"

#include <algorithm>
#include <sstream>

namespace PExpr::internal {
inline void typeError(Reporter& rep, const Ptr<UnaryExpression>& expr, const Type& type)
{
    rep.errorf(expr->location(), "Can not use operator '%s' with type '%s'", toString(expr->op()).data(), type.toString().c_str());
}

inline void typeError(Reporter& rep, const Ptr<BinaryExpression>& expr, const Type& left, const Type& right)
{
    rep.errorf(expr->location(), "Can not use operator '%s' with types '%s' and '%s'", toString(expr->op()).data(), left.toString().c_str(), right.toString().c_str());
}

Type TypeChecker::handle(const Ptr<Closure>& closure)
{
    return handleNode(closure);
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure)
{
    // Preregister functions in this closure
    for (const auto& statement : closure->statements()) {
        if (statement->type() != StatementType::FunctionDeclaration)
            continue;

        const auto funcStmt = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(statement);

        // Pre-register a provisional function definition
        if (!closure->symbols().addFunction(FunctionDef(funcStmt->name(), funcStmt->mangledName(), funcStmt->parameters(), funcStmt->returnType(), funcStmt->isExtern(), funcStmt->hasSideEffects())))
            mReporter.errorf(funcStmt->location(), "Function '%s' already defined in the current scope", funcStmt->name().c_str());
    }

    for (const auto& statement : closure->statements())
        handleNode(closure, statement);

    const Type type = handleNode(closure, closure->expression());
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
        if (type.kind() == TypeKind::Error)
            return; // Error was caught somewhere else

        if (type.kind() == TypeKind::Unspecified) {
            mReporter.errorf(varStmt->location(), "Can not determine type of variable '%s'", varStmt->name().c_str());
            return;
        }

        // If an explicit declared type is provided, validate / coerce the initializer.
        const auto declared = varStmt->declaredType();
        if (declared.kind() != TypeKind::Unspecified && type != declared) {
            if (isConvertible(type, declared)) {
                // Insert an implicit (non-explicit) cast so downstream passes see an explicit cast node.
                const auto orig     = varStmt->expression();
                const auto castExpr = std::make_shared<CastExpression>(orig->location(), declared, orig, false);

                ReportType rt = RT_WARNING_IMPLICIT_CAST;
                // Special case: `int` literal for a `num` variable
                if (declared.kind() == TypeKind::Number && type.kind() == TypeKind::Integer)
                    rt = RT_WARNING_IMPLICIT_CAST_INT;

                mReporter.warningf(rt, orig->location(), "Implicitly converting from '%s' to '%s' for variable '%s'", type.toString().c_str(), declared.toString().c_str(), varStmt->name().c_str());
                varStmt->replaceExpression(castExpr);
            } else {
                mReporter.errorf(varStmt->location(), "Cannot implicitly convert initializer from '%s' to declared type '%s' for variable '%s'", type.toString().c_str(), declared.toString().c_str(), varStmt->name().c_str());
                return;
            }
        }

        // Register the variable
        if (!closure->symbols().addVariable(VariableDef(varStmt->name(), declared.kind() != TypeKind::Unspecified ? declared : type, varStmt->isMutable())))
            mReporter.errorf(varStmt->location(), "New variable '%s' already exists in the current scope", varStmt->name().c_str());
    } break;
    case StatementType::VariableAssignment: {
        const auto varStmt = std::reinterpret_pointer_cast<VariableAssignmentStatement>(statement);
        const auto type    = handleNode(closure, varStmt->expression());
        if (type.kind() == TypeKind::Error)
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

            ReportType rt = RT_WARNING_IMPLICIT_CAST;
            // Special case: `int` literal for a `num` variable
            if (declared.kind() == TypeKind::Number && type.kind() == TypeKind::Integer)
                rt = RT_WARNING_IMPLICIT_CAST_INT;

            mReporter.warningf(rt, orig->location(), "Implicitly converting from '%s' to '%s' for variable '%s'", type.toString().c_str(), declared.toString().c_str(), varStmt->name().c_str());
            varStmt->replaceExpression(castExpr);
        } else if (type != declared) {
            mReporter.errorf(varStmt->location(), "Cannot implicitly convert from '%s' to declared type '%s' for variable '%s'", type.toString().c_str(), declared.toString().c_str(), varStmt->name().c_str());
        }
    } break;
    case StatementType::FunctionDeclaration: {
        const auto funcStmt = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(statement);

        // Add parameters to the symbol table
        if (funcStmt->closure()) {
            for (const auto& p : funcStmt->parameters()) {
                if (!funcStmt->closure()->symbols().addVariable(VariableDef(p.Name, p.ParamType, false))) //< TODO: Really non-mutable?
                    mReporter.errorf(funcStmt->location(), "Parameter '%s' already exists in the current scope", p.Name.c_str());
            }

            PEXPR_ASSERT(funcStmt->closure()->symbols().parent() == &closure->symbols(), "Invalid parent relationship");
        }

        // Type-check the function body to determine the return type
        const auto returnType = funcStmt->isExtern() ? funcStmt->returnType() : handleNode(funcStmt->closure());
        funcStmt->setReturnType(returnType);

        if (returnType.kind() == TypeKind::Unspecified) {
            mReporter.errorf(funcStmt->location(), "Could not determine return type for function '%s'", funcStmt->name().c_str());
            return;
        }

        if (!funcStmt->isExtern())
            closure->symbols().replaceFunction(FunctionDef(funcStmt->name(), funcStmt->mangledName(), funcStmt->parameters(), returnType, funcStmt->isExtern(), funcStmt->hasSideEffects()));

        const auto declared = funcStmt->returnType();
        if (isConvertible(returnType, declared) && returnType != declared) {
            // Insert an implicit (non-explicit) cast so downstream passes see an explicit cast node.
            const auto orig     = funcStmt->closure()->expression();
            const auto castExpr = std::make_shared<CastExpression>(orig->location(), declared, orig, false);

            // Special case: `int` literal for a `num` variable
            if (funcStmt->closure()->expression()->type() == ExpressionType::Literal && declared.kind() == TypeKind::Number && returnType.kind() == TypeKind::Integer) {
                // Ignore warning
            } else {
                mReporter.warningf(RT_WARNING_IMPLICIT_CAST, orig->location(), "Implicitly converting return value from '%s' to '%s' for function '%s'", returnType.toString().c_str(), declared.toString().c_str(), funcStmt->name().c_str());
            }

            funcStmt->closure()->replaceExpression(castExpr);
        } else if (returnType != declared) {
            mReporter.errorf(funcStmt->location(), "Cannot implicitly convert return value from '%s' to declared type '%s' for function '%s'", returnType.toString().c_str(), declared.toString().c_str(), funcStmt->name().c_str());
        }
    } break;
    default:
        break;
    }
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<Expression>& expr)
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
    case ExpressionType::Swizzle:
        return handleNode(closure, std::reinterpret_pointer_cast<SwizzleExpression>(expr));
    case ExpressionType::Access:
        return handleNode(closure, std::reinterpret_pointer_cast<AccessExpression>(expr));
    case ExpressionType::Tuple:
        return handleNode(closure, std::reinterpret_pointer_cast<TupleExpression>(expr));
    case ExpressionType::Cast:
        return handleNode(closure, std::reinterpret_pointer_cast<CastExpression>(expr));
    case ExpressionType::Closure:
        return handleNode(closure, std::reinterpret_pointer_cast<ClosureExpression>(expr));
    case ExpressionType::Branch:
        return handleNode(closure, std::reinterpret_pointer_cast<BranchExpression>(expr));
    default:
        PEXPR_ASSERT(false, "Unhandled expression type");
        return Type(TypeKind::Error);
    }
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<ClosureExpression>& expr)
{
    PEXPR_UNUSED(closure);
    PEXPR_ASSERT(expr->closure()->symbols().parent() == &closure->symbols(), "Invalid parent relationship");

    Type type = handleNode(expr->closure());
    expr->setReturnType(type);
    return type;
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<BranchExpression>& expr)
{
    Type returnType = handleNode(expr->elseClosure());
    if (returnType.kind() == TypeKind::Error) // Error handled somewhere else
        return returnType;

    for (const auto& branch : expr->branches()) {
        const Type conditionType = handleNode(closure, branch.Condition);
        if (isConvertible(conditionType, Type(TypeKind::Boolean))) {
            branch.Condition->setReturnType(Type(TypeKind::Boolean));
        } else {
            mReporter.error(branch.Condition->location(), "Expected condition to evaluate to bool");
            return Type(TypeKind::Error);
        }

        const auto bodyType = handleNode(branch.Body);
        if (bodyType.kind() == TypeKind::Error) // Error handled somewhere else
            return bodyType;

        if (returnType.kind() == TypeKind::Unspecified) {
            returnType = bodyType;
        } else if (bodyType != returnType && isConvertible(bodyType, returnType)) {
            // Inject a CastExpression so the branch body expression has the desired return type.
            // This ensures later stages (SSA mapper) see an explicit cast node rather than relying
            // on the mapper to insert SSA-level casts.
            auto origExpr = branch.Body->expression();
            auto castExpr = std::make_shared<CastExpression>(origExpr->location(), returnType, origExpr);
            branch.Body->replaceExpression(castExpr);

            ReportType rt = RT_WARNING_IMPLICIT_CAST;
            // Special case: `int` literal for a `num` branch
            if (returnType.kind() == TypeKind::Number && bodyType.kind() == TypeKind::Integer)
                rt = RT_WARNING_IMPLICIT_CAST_INT;

            mReporter.warningf(rt, origExpr->location(), "Implicitly converting from '%s' to '%s' for conditional branch", bodyType.toString().data(), returnType.toString().data());
        } else if (bodyType == returnType) {
            // matching type — nothing to do
        } else {
            mReporter.errorf(branch.Condition->location(), "Expected all branch bodies to evaluate to the type '%s'", returnType.toString().data());
            return Type(TypeKind::Error);
        }
    }

    expr->setReturnType(returnType);
    return returnType;
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<VariableExpression>& expr)
{
    if (const auto def = closure->symbols().lookupVariable(expr->location(), expr->name()); def.has_value()) {
        expr->setReturnType(def.value().type());
        return def.value().type();
    } else {
        mReporter.errorf(expr->location(), "Unknown identifier '%s' found", expr->name().c_str());
        return Type(TypeKind::Error);
    }
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<LiteralExpression>& expr)
{
    PEXPR_UNUSED(closure);
    return expr->returnType();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<UnaryExpression>& expr)
{
    auto innerType = handleNode(closure, expr->inner());
    if (innerType.kind() == TypeKind::Error) // Error handled somewhere else
        return innerType;

    expr->setReturnType(Type(TypeKind::Unspecified));

    switch (expr->op()) {
    case UnaryOperation::Pos:
    case UnaryOperation::Neg:
        if (innerType.isArithmetic())
            expr->setReturnType(innerType);
        break;
    case UnaryOperation::Not:
        if (isConvertible(innerType, Type(TypeKind::Boolean)))
            expr->setReturnType(Type(TypeKind::Boolean));
        break;
    default:
        break;
    }

    if (expr->isUnspecified()) {
        typeError(mReporter, expr, innerType);
        return Type(TypeKind::Error);
    }

    return expr->returnType();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<BinaryExpression>& expr)
{
    auto leftType  = handleNode(closure, expr->left());
    auto rightType = handleNode(closure, expr->right());
    if (leftType.kind() == TypeKind::Error || rightType.kind() == TypeKind::Error)
        return Type(TypeKind::Error); // Error was caught somewhere else

    expr->setReturnType(Type(TypeKind::Unspecified));

    switch (expr->op()) {
    case BinaryOperation::Add:
    case BinaryOperation::Sub:
        if (leftType.isArithmetic() && rightType.isArithmetic()) {
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
        if (leftType.isArithmetic() && rightType.isArithmetic()) {
            if (leftType == rightType)
                expr->setReturnType(leftType);
            else if (isConvertible(leftType, rightType))
                expr->setReturnType(rightType);
            else if (isConvertible(rightType, leftType))
                expr->setReturnType(leftType);
            else if (leftType.isVector() && isConvertible(rightType, TypeKind::Number))
                expr->setReturnType(leftType); // vec * f, vec / f
            else if (expr->op() != BinaryOperation::Div && rightType.isVector() && isConvertible(leftType, TypeKind::Number))
                expr->setReturnType(rightType); // f * vec
        }
        break;
    case BinaryOperation::Pow:
        if (leftType.isArithmetic() && rightType.isArithmetic()) {
            if (leftType == rightType && leftType.kind() == TypeKind::Integer)
                expr->setReturnType(leftType); // i ^ i
            else if (isConvertible(leftType, TypeKind::Number) && isConvertible(rightType, TypeKind::Number))
                expr->setReturnType(Type(TypeKind::Number)); // f ^ f
            else if (leftType.isVector() && isConvertible(rightType, TypeKind::Number))
                expr->setReturnType(leftType); // vec ^ f
        }
        break;
    case BinaryOperation::Mod:
        if (isConvertible(leftType, TypeKind::Integer) && isConvertible(rightType, TypeKind::Integer))
            expr->setReturnType(Type(TypeKind::Integer)); // i % i
        break;
    case BinaryOperation::And:
    case BinaryOperation::Or:
        if (isConvertible(leftType, Type(TypeKind::Boolean)) && isConvertible(rightType, Type(TypeKind::Boolean)))
            expr->setReturnType(Type(TypeKind::Boolean));
        break;
    case BinaryOperation::Less:
    case BinaryOperation::Greater:
    case BinaryOperation::LessEqual:
    case BinaryOperation::GreaterEqual:
        if (isConvertible(leftType, TypeKind::Boolean) && isConvertible(rightType, TypeKind::Boolean))
            expr->setReturnType(Type(TypeKind::Boolean));
        else if (isConvertible(leftType, TypeKind::Number) && isConvertible(rightType, TypeKind::Number))
            expr->setReturnType(Type(TypeKind::Boolean));
        break;
    case BinaryOperation::Equal:
    case BinaryOperation::NotEqual:
        if (isConvertible(leftType, rightType) || isConvertible(rightType, leftType))
            expr->setReturnType(Type(TypeKind::Boolean));
        break;
    default:
        break;
    }

    if (expr->isUnspecified()) {
        typeError(mReporter, expr, leftType, rightType);
        return Type(TypeKind::Error);
    }

    return expr->returnType();
}

inline std::string printArgs(const std::vector<Type>& args)
{
    std::stringstream stream;

    for (size_t i = 0; i < args.size(); ++i) {
        stream << args[i].toString();
        if (i != args.size() - 1)
            stream << ", ";
    }

    return stream.str();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<CallExpression>& expr)
{
    std::vector<Type> fromArgs;
    fromArgs.reserve(expr->parameters().size());

    // First, type-check arguments to obtain their types.
    for (size_t i = 0; i < expr->parameters().size(); ++i) {
        auto type = handleNode(closure, expr->parameters().at(i));
        if (type.kind() == TypeKind::Error)
            return type; // Error was caught somewhere else
        fromArgs.push_back(type);
    }

    expr->setReturnType(Type(TypeKind::Unspecified));

    // Lookup the function (this allows matching with implicit convertible args)
    if (const auto def = closure->symbols().lookupFunction(expr->location(), expr->name(), fromArgs); def.has_value()) {
        // For any parameter where the actual type differs from the parameter type
        // and an implicit conversion exists, inject an implicit CastExpression
        // (explicit=false) so downstream passes see an explicit cast node.
        const auto& pList = def.value().parameters();
        for (size_t i = 0; i < expr->parameters().size() && i < pList.size(); ++i) {
            const auto desired = pList[i].ParamType;
            const auto actual  = fromArgs[i];
            if (actual != desired) {
                if (isConvertible(actual, desired)) {
                    auto original = expr->parameters().at(i);
                    auto castExpr = std::make_shared<CastExpression>(original->location(), desired, original, false);
                    expr->replaceParameter(i, castExpr);
                    fromArgs[i] = desired;

                    ReportType rt = RT_WARNING_IMPLICIT_CAST;
                    // Special case: `int` literal for a `num` parameter
                    if (desired.kind() == TypeKind::Number && actual.kind() == TypeKind::Integer)
                        rt = RT_WARNING_IMPLICIT_CAST_INT;

                    mReporter.warningf(rt, original->location(), "Implicitly converting from '%s' to '%s' for function parameter %zu", actual.toString().data(), desired.toString().data(), i);
                } else {
                    mReporter.errorf(expr->parameters().at(i)->location(), "Cannot implicitly convert from '%s' to '%s' for function parameter %zu", actual.toString().data(), desired.toString().data(), i);
                    return Type(TypeKind::Error);
                }
            }
        }

        expr->setReturnType(def.value().returnType());
        expr->setMangledName(def->mangledName());
    } else {
        mReporter.errorf(expr->location(), "Function '%s(%s)' is unknown or ambiguous", expr->name().c_str(), printArgs(fromArgs).c_str());
        return Type(TypeKind::Error);
    }

    return expr->returnType();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<SwizzleExpression>& expr)
{
    auto innerType = handleNode(closure, expr->inner());
    if (innerType.kind() == TypeKind::Error)
        return innerType; // Error was caught somewhere else

    expr->setReturnType(Type(TypeKind::Unspecified));

    // The access operator also allows expanding e.g., vec2.xyxy -> vec4 operations
    if (innerType.isVector()) {
        const auto& swizzle = expr->swizzle();

        const size_t vec_size = innerType.size();

        if (vec_size == 0 || vec_size > 4) {
            mReporter.errorf(expr->location(), "The access operator is only defined for vector types of 1-4, but got a vector of size %zu instead", vec_size);
            return Type(TypeKind::Error);
        }

        bool isValid = true;
        for (char c : swizzle) {
            isValid = (c == 'x' || c == 'r'
                       || (vec_size > 1 && c == 'y') || (vec_size > 1 && c == 'g')
                       || (vec_size > 2 && c == 'z') || (vec_size > 2 && c == 'b')
                       || (vec_size > 3 && c == 'w') || (vec_size > 3 && c == 'a'));

            if (!isValid)
                break;
        }

        PEXPR_ASSERT(swizzle.size() > 0, "Expected at least a single component");
        if (!isValid) {
            mReporter.errorf(expr->location(), "Invalid swizzle components '%s' given", std::string(swizzle).c_str());
            return Type(TypeKind::Error);
        } else {
            switch (swizzle.size()) {
            case 1:
                expr->setReturnType(Type(TypeKind::Number));
                break;
            case 2:
                expr->setReturnType(Type({ Type(TypeKind::Number), Type(TypeKind::Number) }));
                break;
            case 3:
                expr->setReturnType(Type({ Type(TypeKind::Number), Type(TypeKind::Number), Type(TypeKind::Number) }));
                break;
            case 4:
                expr->setReturnType(Type({ Type(TypeKind::Number), Type(TypeKind::Number), Type(TypeKind::Number), Type(TypeKind::Number) }));
                break;
            default:
                mReporter.errorf(expr->location(), "Expected a maximum of 4 components but got %zu", swizzle.size());
                return Type(TypeKind::Error);
            }
        }
    } else {
        mReporter.errorf(expr->location(), "Swizzle operator is only defined for vector types");
        return Type(TypeKind::Error);
    }

    return expr->returnType();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<AccessExpression>& expr)
{
    auto innerType = handleNode(closure, expr->inner());
    if (innerType.kind() == TypeKind::Error)
        return innerType; // Error was caught somewhere else

    expr->setReturnType(Type(TypeKind::Unspecified));

    if (innerType.kind() == TypeKind::Tuple) {
        const size_t vec_size = innerType.size();

        if (vec_size < expr->index()) {
            mReporter.errorf(expr->location(), "Out of bounds access with %zu on tuple of size %zu", expr->index(), vec_size);
            return Type(TypeKind::Error);
        }
    } else {
        mReporter.errorf(expr->location(), "Access operator is only defined for vector types");
        return Type(TypeKind::Error);
    }

    expr->setReturnType(innerType.components().at(expr->index()));
    return expr->returnType();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<TupleExpression>& expr)
{
    if (expr->entries().size() == 0) {
        mReporter.errorf(expr->location(), "Can not create an empty vector");
        return Type(TypeKind::Error);
    }

    // Ensure each entry is type-checked.
    std::vector<Type> innerTypes;
    for (size_t i = 0; i < expr->entries().size(); ++i) {
        auto orig        = expr->entries().at(i);
        const auto pType = handleNode(closure, orig);
        if (pType.kind() == TypeKind::Error)
            return pType; // Error handled somewhere else

        innerTypes.push_back(pType);
    }

    expr->setReturnType(Type(innerTypes));
    return expr->returnType();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<CastExpression>& expr)
{
    // Type-check inner expression first
    const auto innerType = handleNode(closure, expr->inner());
    if (innerType.kind() == TypeKind::Error)
        return innerType; // Error was reported deeper

    // Validate allowed conversion depending on whether the cast is explicit or implicit
    if (expr->isExplicit()) {
        if (!isExplicitConvertible(innerType, expr->toType())) {
            mReporter.errorf(expr->location(), "Cannot cast from '%s' to '%s'", innerType.toString().data(), expr->toType().toString().data());
            return Type(TypeKind::Error);
        }
    } else {
        if (!isConvertible(innerType, expr->toType())) {
            mReporter.errorf(expr->location(), "Implicit conversion from '%s' to '%s' is not allowed", innerType.toString().data(), expr->toType().toString().data());
            return Type(TypeKind::Error);
        }
    }

    expr->setReturnType(expr->toType());
    return expr->returnType();
}
} // namespace PExpr::internal
