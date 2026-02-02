#include "TypeChecker.h"
#include "Definitions.h"
#include "Mangler.h"
#include "ast/Statement.h"

#include <algorithm>
#include <functional>
#include <sstream>

namespace PExpr::type {
using namespace ast;

inline void typeError(utils::Reporter& rep, const Ptr<UnaryExpression>& expr, const Type& type)
{
    rep.errorf(expr->location(), "Can not use operator '%s' with type '%s'", toString(expr->op()).data(), type.toString().c_str());
}

inline void typeError(utils::Reporter& rep, const Ptr<BinaryExpression>& expr, const Type& left, const Type& right)
{
    rep.errorf(expr->location(), "Can not use operator '%s' with types '%s' and '%s'", toString(expr->op()).data(), left.toString().c_str(), right.toString().c_str());
}

Ptr<ast::Expression> TypeChecker::injectCastIfNeeded(const Ptr<ast::Expression>& origExpr, const Type& toType, bool* hadError)
{
    const auto origType = origExpr->returnType();
    PEXPR_ASSERT(origType.kind() != TypeKind::Unspecified, "Expected a valid type");

    if (origType == toType)
        return origExpr;

    if (isConvertible(origType, toType)) {
        auto castExpr = std::make_shared<CastExpression>(origExpr->location(), toType, origExpr);

        utils::ReportType rt = utils::RT_WARNING_IMPLICIT_CAST;

        // Special case: `int` literal for a `num` branch
        if (toType.kind() == TypeKind::Number && origType.kind() == TypeKind::Integer)
            rt = utils::RT_WARNING_IMPLICIT_CAST_INT;

        mReporter.warningf(rt, origExpr->location(), "Implicitly converting from '%s' to '%s'", origType.toString().data(), toType.toString().data());

        return castExpr;
    } else {
        if (hadError)
            *hadError = true;
        mReporter.errorf(origExpr->location(), "Can not convert '%s' to '%s'", origType.toString().data(), toType.toString().data());
        return origExpr;
    }
}

Type TypeChecker::handle(const Ptr<Closure>& closure)
{
    return handleNode(closure);
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure)
{
    // Preregister functions in this closure
    for (const auto& statement : closure->statements()) {
        if (statement->type() == StatementType::FunctionDeclaration) {
            const auto funcStmt = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(statement);

            // Pre-register a provisional function definition
            if (!closure->symbols().addFunction(FunctionDef(funcStmt->name(), funcStmt->mangledName(), funcStmt->parameters(), funcStmt->returnType(), funcStmt->isExtern(), funcStmt->hasSideEffects())))
                mReporter.errorf(funcStmt->location(), "Function '%s' already defined in the current scope", funcStmt->name().c_str());
        } else if (statement->type() == StatementType::TypeAlias) {
            // Type aliases are already registered by the parser immediately
            // No need to re-register here
        }
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
        const auto declStmt = std::reinterpret_pointer_cast<VariableDeclarationStatement>(statement);
        const auto& pattern = declStmt->pattern();

        // Type-check the RHS expression
        const auto rhsType = handleNode(closure, declStmt->expression());
        if (rhsType.kind() == TypeKind::Error)
            return;

        // TODO: Rework this
        // Check if pattern is a single simple binding (i.e., regular variable declaration)
        if (pattern->size() == 1 && pattern->elements()[0].isSimpleBinding()) {
            auto binding = pattern->elements()[0].simpleBinding();

            // Check type compatibility if explicit type is provided
            if (binding->type().kind() != TypeKind::Unspecified) {
                if (!isConvertible(rhsType, binding->type())) {
                    mReporter.errorf(declStmt->location(), "Cannot convert from '%s' to '%s' for variable '%s' declaration",
                                     rhsType.toString().c_str(), binding->type().toString().c_str(), binding->name().c_str());
                    return;
                }
            } else {
                // If the variable was unspecified before, replace it
                binding->setType(rhsType);
            }
        } else {
            // Destructuring pattern: RHS must return a tuple
            if (!rhsType.isTuple()) {
                mReporter.errorf(declStmt->location(), "Destructuring requires a tuple expression on the right-hand side");
                return;
            }

            // Register variables using the original rhsType
            std::function<bool(Pattern&, const Type&)> registerVariables =
                [&](Pattern& pattern, const Type& type) -> bool {
                if (!type.isTuple()) {
                    mReporter.errorf(pattern.location(), "Nested pattern requires a tuple type");
                    return false;
                }

                const auto& components = type.components();
                if (pattern.size() != components.size()) {
                    mReporter.errorf(pattern.location(), "Pattern size %zu does not match tuple size %zu", pattern.size(), components.size());
                    return false;
                }

                for (size_t i = 0; i < pattern.size(); ++i) {
                    auto& elem           = pattern.elements()[i];
                    const auto& elemType = components.at(i);

                    if (elem.isSimpleBinding()) {
                        auto binding = elem.simpleBinding();

                        // Check type compatibility if explicit type is provided
                        if (binding->type().kind() != TypeKind::Unspecified) {
                            if (!isConvertible(elemType, binding->type())) {
                                mReporter.errorf(declStmt->location(), "Cannot convert from '%s' to '%s' for variable '%s' declaration",
                                                 elemType.toString().c_str(), binding->type().toString().c_str(), binding->name().c_str());
                                return false;
                            }
                        } else {
                            binding->setType(elemType);
                        }
                    } else {
                        // Nested pattern - recurse
                        if (!registerVariables(*elem.nestedPattern(), elemType))
                            return false;
                    }
                }

                return true;
            };

            registerVariables(*pattern, rhsType);
        }
    } break;
    case StatementType::VariableAssignment: {
        const auto assignStmt = std::reinterpret_pointer_cast<VariableAssignmentStatement>(statement);
        auto pattern          = assignStmt->pattern();

        // Type-check the RHS expression
        const auto rhsType = handleNode(closure, assignStmt->expression());
        if (rhsType.kind() == TypeKind::Error)
            return;

        // TODO: Rework this
        // Check if pattern is a single simple binding (i.e., regular variable assignment)
        if (pattern->size() == 1 && pattern->elements()[0].isSimpleBinding()) {
            auto binding = pattern->elements()[0].simpleBinding();

            if (!binding->isMutable()) {
                mReporter.errorf(assignStmt->location(), "Cannot assign to immutable variable '%s'", binding->name().c_str());
                return;
            }

            // Check type compatibility
            const auto& varType = binding->type();
            if (!isConvertible(rhsType, varType)) {
                mReporter.errorf(assignStmt->location(), "Cannot convert from '%s' to '%s' for variable '%s'",
                                 rhsType.toString().c_str(), varType.toString().c_str(), binding->name().c_str());
                return;
            }
        } else {
            // Destructuring pattern: RHS must be a tuple
            if (!rhsType.isTuple()) {
                mReporter.errorf(assignStmt->location(), "Destructuring requires a tuple expression on the right-hand side");
                return;
            }

            // Helper function to recursively process pattern elements for assignment
            std::function<bool(Pattern&, const Type&)> processPattern =
                [&](Pattern& pattern, const Type& type) -> bool {
                if (!type.isTuple()) {
                    mReporter.errorf(pattern.location(), "Nested pattern requires a tuple type");
                    return false;
                }

                const auto& components = type.components();
                if (pattern.size() != components.size()) {
                    mReporter.errorf(pattern.location(), "Pattern size %zu does not match tuple size %zu", pattern.size(), components.size());
                    return false;
                }

                for (size_t i = 0; i < pattern.size(); ++i) {
                    auto& elem           = pattern.elements()[i];
                    const auto& elemType = components.at(i);

                    if (elem.isSimpleBinding()) {
                        auto binding = elem.simpleBinding();

                        if (!binding->isMutable()) {
                            mReporter.errorf(elem.location(), "Cannot assign to immutable variable '%s'", binding->name().c_str());
                            return false;
                        }

                        // Check type compatibility
                        const auto& varType = binding->type();
                        if (!isConvertible(elemType, varType)) {
                            mReporter.errorf(elem.location(), "Cannot convert tuple element from '%s' to '%s' for variable '%s'",
                                             elemType.toString().c_str(), varType.toString().c_str(), binding->name().c_str());
                            return false;
                        }
                    } else {
                        // Nested pattern - recurse
                        if (!processPattern(*elem.nestedPattern(), elemType))
                            return false;
                    }
                }

                return true;
            };

            processPattern(*pattern, rhsType);
        }
    } break;
    case StatementType::FunctionDeclaration: {
        const auto funcStmt = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(statement);

        if (funcStmt->isExtern() || !funcStmt->closure()) {
            // Do nothing
        } else {
            // Type-check the function body to determine the return type and process it
            const auto returnType = handleNode(funcStmt->closure());

            // The declared return type (potentially unspecified)
            const auto declaredReturnType = funcStmt->returnType();

            if (declaredReturnType.kind() == TypeKind::Unspecified || declaredReturnType.kind() == TypeKind::Error)
                funcStmt->setReturnType(returnType);

            if (returnType.kind() == TypeKind::Unspecified) {
                mReporter.errorf(funcStmt->location(), "Could not determine return type for function '%s'", funcStmt->name().c_str());
                return;
            }

            closure->symbols().replaceFunction(FunctionDef(funcStmt->name(), funcStmt->mangledName(), funcStmt->parameters(), funcStmt->returnType(), funcStmt->isExtern(), funcStmt->hasSideEffects()));
            funcStmt->closure()->expressionMut() = injectCastIfNeeded(funcStmt->closure()->expression(), funcStmt->returnType(), nullptr);
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
    // Setup conditionals
    bool hadCastError = false;
    for (auto& branch : expr->branches()) {
        handleNode(closure, branch.Condition);
        branch.Condition = injectCastIfNeeded(branch.Condition, Type(TypeKind::Boolean), &hadCastError);

        if (hadCastError)
            return Type(TypeKind::Error);
    }

    // Determine the type of the expression
    Type returnType = handleNode(expr->elseClosure());
    if (returnType.kind() == TypeKind::Error) // Error handled somewhere else
        return returnType;

    for (const auto& branch : expr->branches()) {
        const auto bodyType = handleNode(branch.Body);
        if (bodyType.kind() == TypeKind::Error) // Error handled somewhere else
            return bodyType;

        if (returnType.kind() == TypeKind::Unspecified) {
            returnType = bodyType;
        } else if (bodyType != returnType) {
            if (isConvertible(bodyType, returnType)) {
                // Ok
            } else if (isConvertible(returnType, bodyType)) {
                returnType = bodyType;
            } else {
                mReporter.errorf(branch.Condition->location(), "Expected all branch bodies to evaluate to the type '%s'", returnType.toString().data());
                return Type(TypeKind::Error);
            }
        }
    }

    // Inject casts if necessary
    expr->elseClosure()->expressionMut() = injectCastIfNeeded(expr->elseClosure()->expression(), returnType, &hadCastError);
    for (auto& branch : expr->branches())
        branch.Body->expressionMut() = injectCastIfNeeded(branch.Body->expression(), returnType, &hadCastError);

    if (!hadCastError)
        expr->setReturnType(returnType);
    else
        expr->setReturnType(Type(TypeKind::Error));
    return returnType;
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<VariableExpression>& expr)
{
    PEXPR_UNUSED(closure);
    expr->setReturnType(expr->variable()->type());
    return expr->returnType();
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
    case UnaryOperation::Not: {
        bool hadCastError = false;
        expr->innerMut()  = injectCastIfNeeded(expr->inner(), Type(TypeKind::Boolean), &hadCastError);
        if (!hadCastError)
            expr->setReturnType(Type(TypeKind::Boolean));
    } break;
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

    bool hadCastError = false;
    switch (expr->op()) {
    case BinaryOperation::Add:
    case BinaryOperation::Sub:
        if (leftType.isArithmetic() && leftType == rightType) {
            expr->setReturnType(leftType);
        } else if (isConvertible(leftType, rightType)) {
            expr->leftMut() = injectCastIfNeeded(expr->left(), rightType, &hadCastError);
            if (!hadCastError)
                expr->setReturnType(rightType);
        } else if (isConvertible(rightType, leftType)) {
            expr->rightMut() = injectCastIfNeeded(expr->left(), leftType, &hadCastError);
            if (!hadCastError)
                expr->setReturnType(leftType);
        }
        break;
    case BinaryOperation::Mul:
    case BinaryOperation::Div:
        if (leftType.isArithmetic() && leftType == rightType) { // < i * i, f * f, v * v
            expr->setReturnType(leftType);
        } else if (rightType.isArithmetic() && isConvertible(leftType, rightType)) { // < i * f
            expr->leftMut() = injectCastIfNeeded(expr->left(), rightType, &hadCastError);
            if (!hadCastError)
                expr->setReturnType(rightType);
        } else if (leftType.isArithmetic() && isConvertible(rightType, leftType)) { // < f * i
            expr->rightMut() = injectCastIfNeeded(expr->left(), leftType, &hadCastError);
            if (!hadCastError)
                expr->setReturnType(leftType);
        } else if (leftType.isTuple() && isConvertible(leftType, Type::AsVector(leftType.size())) && isConvertible(rightType, TypeKind::Number)) { // < v * f, v * i
            expr->leftMut()  = injectCastIfNeeded(expr->left(), Type::AsVector(leftType.size()), &hadCastError);
            expr->rightMut() = injectCastIfNeeded(expr->right(), Type(TypeKind::Number), &hadCastError);
            if (!hadCastError)
                expr->setReturnType(expr->left()->returnType());
        } else if (expr->op() != BinaryOperation::Div && rightType.isTuple() && isConvertible(rightType, Type::AsVector(rightType.size())) && isConvertible(leftType, TypeKind::Number)) { // < f * v,  i * v
            expr->leftMut()  = injectCastIfNeeded(expr->left(), Type(TypeKind::Number), &hadCastError);
            expr->rightMut() = injectCastIfNeeded(expr->right(), Type::AsVector(rightType.size()), &hadCastError);
            if (!hadCastError)
                expr->setReturnType(expr->right()->returnType());
        }
        break;
    case BinaryOperation::Pow:
        if (leftType == rightType && leftType.kind() == TypeKind::Integer) {
            expr->setReturnType(leftType); // i ^ i
        } else if (isConvertible(leftType, TypeKind::Number) && isConvertible(rightType, TypeKind::Number)) {
            expr->leftMut()  = injectCastIfNeeded(expr->left(), Type(TypeKind::Number), &hadCastError);
            expr->rightMut() = injectCastIfNeeded(expr->right(), Type(TypeKind::Number), &hadCastError);
            if (!hadCastError)
                expr->setReturnType(Type(TypeKind::Number)); // f ^ f
        } else if (leftType.isTuple() && isConvertible(leftType, Type::AsVector(leftType.size())) && isConvertible(rightType, TypeKind::Number)) {
            expr->leftMut()  = injectCastIfNeeded(expr->left(), Type::AsVector(leftType.size()), &hadCastError);
            expr->rightMut() = injectCastIfNeeded(expr->right(), Type(TypeKind::Number), &hadCastError);
            if (!hadCastError)
                expr->setReturnType(expr->left()->returnType()); // vec ^ f
        }
        break;
    case BinaryOperation::Mod: {
        expr->leftMut()  = injectCastIfNeeded(expr->left(), Type(TypeKind::Integer), &hadCastError);
        expr->rightMut() = injectCastIfNeeded(expr->right(), Type(TypeKind::Integer), &hadCastError);
        if (!hadCastError)
            expr->setReturnType(Type(TypeKind::Integer));
    } break;
    case BinaryOperation::And:
    case BinaryOperation::Or: {
        expr->leftMut()  = injectCastIfNeeded(expr->left(), Type(TypeKind::Boolean), &hadCastError);
        expr->rightMut() = injectCastIfNeeded(expr->right(), Type(TypeKind::Boolean), &hadCastError);
        if (!hadCastError)
            expr->setReturnType(Type(TypeKind::Boolean));
    } break;
    case BinaryOperation::Less:
    case BinaryOperation::Greater:
    case BinaryOperation::LessEqual:
    case BinaryOperation::GreaterEqual: {
        auto sameType = leftType;
        if (isConvertible(leftType, rightType))
            sameType = rightType;

        // We only support `int` and `num`
        if (sameType.kind() != TypeKind::Integer)
            sameType = Type(TypeKind::Number);

        expr->leftMut()  = injectCastIfNeeded(expr->left(), sameType, &hadCastError);
        expr->rightMut() = injectCastIfNeeded(expr->right(), sameType, &hadCastError);
        if (!hadCastError)
            expr->setReturnType(Type(TypeKind::Boolean));
    } break;
    case BinaryOperation::Equal:
    case BinaryOperation::NotEqual: {
        auto sameType = leftType;
        if (isConvertible(leftType, rightType))
            sameType = rightType;

        expr->leftMut()  = injectCastIfNeeded(expr->left(), sameType, &hadCastError);
        expr->rightMut() = injectCastIfNeeded(expr->right(), sameType, &hadCastError);
        if (!hadCastError)
            expr->setReturnType(Type(TypeKind::Boolean));
    } break;
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
        bool hadCastError = false;
        const auto& pList = def.value().parameters();
        for (size_t i = 0; i < expr->parameters().size() && i < pList.size(); ++i) {
            const auto desired = pList[i]->type();
            auto castExpr      = injectCastIfNeeded(expr->parameters().at(i), desired, &hadCastError);
            fromArgs[i]        = castExpr->returnType();
            expr->replaceParameter(i, castExpr);
        }

        if (!hadCastError)
            expr->setReturnType(def.value().returnType());
        else
            expr->setReturnType(Type(TypeKind::Error));
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

    if (!innerType.isTuple()) {
        mReporter.errorf(expr->location(), "Swizzle operator is only defined for vector types");
        return Type(TypeKind::Error);
    }

    // The access operator also allows expanding e.g., vec2.xyxy -> vec4 operations
    const auto& swizzle   = expr->swizzle();
    const size_t vec_size = innerType.size();

    if (vec_size == 0) {
        mReporter.errorf(expr->location(), "The access operator can not be applied to empty tuples");
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

    if (!isValid || swizzle.empty()) {
        mReporter.errorf(expr->location(), "Invalid swizzle components '%s' given", std::string(swizzle).c_str());
        return Type(TypeKind::Error);
    } else {
        PEXPR_ASSERT(swizzle.size() > 0, "Expected at least a single component");

        std::vector<Type> retTypes;
        retTypes.reserve(swizzle.size());
        for (char c : swizzle) {
            if (c == 'x' || c == 'r')
                retTypes.push_back(innerType.components().at(0));
            else if (c == 'y' || c == 'g')
                retTypes.push_back(innerType.components().at(1));
            else if (c == 'z' || c == 'b')
                retTypes.push_back(innerType.components().at(2));
            else if (c == 'w' || c == 'a')
                retTypes.push_back(innerType.components().at(3));
        }
        if (retTypes.size() == 1)
            expr->setReturnType(retTypes[0]);
        else
            expr->setReturnType(Type(std::move(retTypes)));
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
        mReporter.errorf(expr->location(), "Access operator is only defined for tuple/vector types");
        return Type(TypeKind::Error);
    }

    expr->setReturnType(innerType.components().at(expr->index()));
    return expr->returnType();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<TupleExpression>& expr)
{
    if (expr->entries().size() == 0) {
        mReporter.errorf(expr->location(), "Can not create an empty tuple");
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
} // namespace PExpr::type
