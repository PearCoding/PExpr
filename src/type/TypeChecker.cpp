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
    for (const auto& expr : closure->expressions()) {
        if (expr->type() == ExpressionType::FunctionDeclaration) {
            // Pre-register a provisional function definition
            const auto funcStmt = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(expr);
            if (funcStmt->isExtern() && funcStmt->isUnspecified()) {
                mReporter.errorf(funcStmt->location(), "External function '%s' has no return type defined", funcStmt->name().c_str());
            } else {
                auto funcDef = FunctionDef(funcStmt->name(), funcStmt->mangledName(), funcStmt->parameters(), funcStmt->functionReturnType(), funcStmt->isExtern(), funcStmt->hasSideEffects(), funcStmt->location());
                if (!closure->symbols().addFunction(std::move(funcDef))) {
                    std::vector<Type> funcTypes;
                    funcTypes.reserve(funcStmt->parameters().size());
                    for (auto p : funcStmt->parameters())
                        funcTypes.push_back(p->type());

                    auto prevFunc = closure->symbols().lookupFunction(funcStmt->location(), funcStmt->name(), funcTypes, true);
                    if (prevFunc)
                        mReporter.warningf(utils::RT_WARNING_FUNCTION_REDEFINITION, funcStmt->location(), "Function '%s' already defined at %s", funcStmt->name().c_str(), prevFunc->location().toString().c_str());
                    else
                        mReporter.warningf(utils::RT_WARNING_FUNCTION_REDEFINITION, funcStmt->location(), "Function '%s' already defined in the current scope", funcStmt->name().c_str());
                }
            }
        }
    }

    for (const auto& expr : closure->expressions())
        handleNode(closure, expr);

    for (const auto& expr : closure->expressionsWithoutFinal()) {
        if (!expr->returnType().isVoid())
            mReporter.warningf(utils::RT_WARNING_UNUSED_CLOSURE_RETURN, expr->location(),
                               "Statement returns a value of type '%s' which is unused",
                               expr->returnType().toString().data());
    }

    if (closure->hasFinalExpression()) {
        return closure->finalExpression()->returnType();
    } else {
        return Type::Void();
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
    case ExpressionType::Assignment:
        return handleNode(closure, std::reinterpret_pointer_cast<AssignmentExpression>(expr));
    case ExpressionType::VariableDeclaration:
        return handleNode(closure, std::reinterpret_pointer_cast<VariableDeclarationStatement>(expr));
    case ExpressionType::FunctionDeclaration:
        return handleNode(closure, std::reinterpret_pointer_cast<FunctionDeclarationStatement>(expr));
    case ExpressionType::TypeAlias:
        return Type::Void();
    case ExpressionType::Error:
        // Error from the parser, continue type-checking as much as possible
        return Type::Error();
    default:
        PEXPR_ASSERT(false, "Unhandled expression type");
        return Type::Error();
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
    bool hadError = false;
    for (auto& branch : expr->branches()) {
        const auto condRetType = handleNode(closure, branch.Condition);
        if (condRetType.isSpecified()) {
            branch.Condition = injectCastIfNeeded(branch.Condition, Type(TypeKind::Boolean), &hadError);
        } else {
            mReporter.errorf(branch.Condition->location(), "Could not determine type for conditional");
            hadError = true;
        }
    }

    // Determine the type of the expression
    Type returnType = Type::Unspecified();

    if (expr->elseClosure()) {
        returnType = handleNode(expr->elseClosure());
        if (returnType.isError()) // Error handled somewhere else
            hadError = true;
    }

    for (const auto& branch : expr->branches()) {
        const auto bodyType = handleNode(branch.Body);
        if (bodyType.isError()) // Error handled somewhere else
            hadError = true;

        if (!returnType.isSpecified()) {
            returnType = bodyType;
        } else if (bodyType != returnType) {
            if (isConvertible(bodyType, returnType)) {
                // Ok
            } else if (isConvertible(returnType, bodyType)) {
                returnType = bodyType;
            } else {
                mReporter.errorf(branch.Body->hasFinalExpression() ? branch.Body->finalExpression()->location() : branch.Body->location(),
                                 "Expected all branch bodies to evaluate to the type '%s'", returnType.toString().data());
                return Type::Error();
            }
        }
    }

    if (!returnType.isSpecified()) {
        mReporter.errorf(expr->location(), "Could not determine the type of the if expression");
        return Type::Error();
    }

    // Inject casts if necessary
    if (expr->elseClosure() && expr->elseClosure()->hasFinalExpression()) {
        // The else case might still be unspecified
        if (!expr->elseClosure()->finalExpression()->returnType().isSpecified())
            expr->elseClosure()->finalExpression()->setReturnType(returnType);

        expr->elseClosure()->finalExpressionMut() = injectCastIfNeeded(expr->elseClosure()->finalExpression(), returnType, &hadError);
        for (auto& branch : expr->branches())
            branch.Body->finalExpressionMut() = injectCastIfNeeded(branch.Body->finalExpression(), returnType, &hadError);
    } else {
        // No else case means this expression has partial returns, which we do not support and fallback to 'void'.
        if (!returnType.isVoid()) {
            for (auto& branch : expr->branches()) {
                if (branch.Body->hasFinalExpression() && !branch.Body->finalExpression()->returnType().isVoid())
                    mReporter.warningf(utils::RT_WARNING_UNUSED_CLOSURE_RETURN, branch.Body->location(),
                                       "Branch returns a value of type '%s' which is unused",
                                       branch.Body->finalExpression()->returnType().toString().data());
            }
        }
        returnType = Type::Void();
    }

    if (!hadError) {
        expr->setReturnType(returnType);
        return returnType;
    } else {
        expr->setReturnType(Type::Error());
        return Type::Error();
    }
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

    expr->setReturnType(Type::Unspecified());

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
        return Type::Error();
    }

    return expr->returnType();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<BinaryExpression>& expr)
{
    auto leftType  = handleNode(closure, expr->left());
    auto rightType = handleNode(closure, expr->right());
    if (leftType.kind() == TypeKind::Error || rightType.kind() == TypeKind::Error)
        return Type::Error(); // Error was caught somewhere else

    expr->setReturnType(Type::Unspecified());

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
            expr->rightMut() = injectCastIfNeeded(expr->right(), leftType, &hadCastError);
            if (!hadCastError)
                expr->setReturnType(leftType);
        } else if (leftType.isTuple() && rightType.isTuple() && leftType.size() == rightType.size()) {
            auto ct = commonArithmeticType(leftType, rightType);
            if (ct) {
                expr->leftMut()  = injectCastIfNeeded(expr->left(), *ct, &hadCastError);
                expr->rightMut() = injectCastIfNeeded(expr->right(), *ct, &hadCastError);
                if (!hadCastError)
                    expr->setReturnType(*ct);
            }
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
            expr->rightMut() = injectCastIfNeeded(expr->right(), leftType, &hadCastError);
            if (!hadCastError)
                expr->setReturnType(leftType);
        } else if (leftType.isTuple() && rightType.isTuple() && leftType.size() == rightType.size()) { // < tuple * tuple with mixed types
            auto ct = commonArithmeticType(leftType, rightType);
            if (ct) {
                expr->leftMut()  = injectCastIfNeeded(expr->left(), *ct, &hadCastError);
                expr->rightMut() = injectCastIfNeeded(expr->right(), *ct, &hadCastError);
                if (!hadCastError)
                    expr->setReturnType(*ct);
            }
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
        return Type::Error();
    }

    return expr->returnType();
}

inline std::string printArgs(const ParameterList& args)
{
    std::stringstream stream;

    for (size_t i = 0; i < args.size(); ++i) {
        if (i)
            stream << ", ";
        stream << args[i]->name() << ":" << args[i]->type().toString();
    }

    return stream.str();
}

inline std::string printArgs(const std::vector<Type>& args)
{
    std::stringstream stream;

    for (size_t i = 0; i < args.size(); ++i) {
        if (i)
            stream << ", ";
        stream << args[i].toString();
    }

    return stream.str();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<CallExpression>& expr)
{
    std::vector<Type> fromArgs;
    fromArgs.reserve(expr->parameters().size());

    // First, type-check arguments to obtain their types.
    bool hadError = false;
    for (size_t i = 0; i < expr->parameters().size(); ++i) {
        auto type = handleNode(closure, expr->parameters().at(i));
        if (type.kind() == TypeKind::Error)
            hadError = true; // Error was caught somewhere else
        fromArgs.push_back(type);
    }

    expr->setReturnType(Type::Unspecified());

    // Lookup the function (this allows matching with implicit convertible args)
    if (const auto def = closure->symbols().lookupFunction(expr->location(), expr->name(), fromArgs); def.has_value()) {
        // For any parameter where the actual type differs from the parameter type
        // and an implicit conversion exists, inject an implicit CastExpression
        // (explicit=false) so downstream passes see an explicit cast node.

        const auto& pList = def.value().parameters();
        for (size_t i = 0; i < expr->parameters().size() && i < pList.size(); ++i) {
            const auto desired = pList[i]->type();
            auto castExpr      = injectCastIfNeeded(expr->parameters().at(i), desired, &hadError);
            fromArgs[i]        = castExpr->returnType();
            expr->replaceParameter(i, castExpr);
        }

        if (!hadError)
            expr->setReturnType(def.value().returnType());
        else
            expr->setReturnType(Type::Error());
        expr->setMangledName(def->mangledName());
    } else {
        auto availableFunctions = closure->symbols().getFunctions(expr->name());
        if (availableFunctions.empty()) {
            mReporter.errorf(expr->location(), "Call to an undeclared function '%s(%s)' found", expr->name().c_str(), printArgs(fromArgs).c_str());
        } else {
            std::stringstream stream;
            stream << "Call to function '" << expr->name() << "(" << printArgs(fromArgs) << ")' does not match. The following functions are available:";
            for (const auto& avlFunc : availableFunctions)
                stream << std::endl
                       << "  | " << expr->name() << "(" << printArgs(avlFunc.parameters()) << ") -> " << avlFunc.returnType().toString() << " [" << avlFunc.location() << "]";

            mReporter.error(expr->location(), stream.str());
        }
        expr->setReturnType(Type::Error());
        return Type::Error();
    }

    return expr->returnType();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<SwizzleExpression>& expr)
{
    auto innerType = handleNode(closure, expr->inner());
    if (innerType.kind() == TypeKind::Error)
        return innerType; // Error was caught somewhere else

    expr->setReturnType(Type::Unspecified());

    if (!innerType.isTuple()) {
        mReporter.errorf(expr->location(), "Swizzle operator is only defined for vector types");
        return Type::Error();
    }

    // The access operator also allows expanding e.g., vec2.xyxy -> vec4 operations
    const auto& swizzle   = expr->swizzle();
    const size_t vec_size = innerType.size();

    if (vec_size == 0) {
        mReporter.errorf(expr->location(), "The access operator can not be applied to empty tuples");
        return Type::Error();
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
        return Type::Error();
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

    expr->setReturnType(Type::Unspecified());

    if (innerType.kind() == TypeKind::Tuple) {
        const size_t vec_size = innerType.size();
        if (vec_size <= expr->index()) {
            mReporter.errorf(expr->location(), "Trying to access element %zu of tuple of size %zu", expr->index(), vec_size);
            return Type::Error();
        }
    } else {
        mReporter.errorf(expr->location(), "Access operator is only defined for tuple/vector types");
        return Type::Error();
    }

    expr->setReturnType(innerType.components().at(expr->index()));
    return expr->returnType();
}

Type TypeChecker::handleNode(const Ptr<Closure>& closure, const Ptr<TupleExpression>& expr)
{
    if (expr->entries().size() == 0) {
        mReporter.errorf(expr->location(), "Can not create an empty tuple");
        return Type::Error();
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
            return Type::Error();
        }
    } else {
        if (!isConvertible(innerType, expr->toType())) {
            mReporter.errorf(expr->location(), "Implicit conversion from '%s' to '%s' is not allowed", innerType.toString().data(), expr->toType().toString().data());
            return Type::Error();
        }
    }

    expr->setReturnType(expr->toType());
    return expr->returnType();
}

Type TypeChecker::handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::VariableDeclarationStatement>& expr)
{
    // Type-check the RHS expression
    const auto rhsType = handleNode(closure, expr->expression());

    // TODO: Rework this
    // Check if pattern is a single simple binding (i.e., regular variable declaration)
    const auto& pattern = expr->pattern();
    if (pattern->size() == 1 && pattern->elements()[0].isSimpleBinding()) {
        auto binding = pattern->elements()[0].simpleBinding();

        // Check type compatibility if explicit type is provided
        if (binding->type().isSpecified()) {
            if (binding->type().isVoid()) {
                mReporter.errorf(expr->location(), "Cannot assign a 'void' to variable '%s'", binding->name().c_str());
                return Type::Error();
            }

            if (!isConvertible(rhsType, binding->type())) {
                mReporter.errorf(expr->location(), "Cannot convert from '%s' to '%s' for variable '%s' declaration",
                                 rhsType.toString().c_str(), binding->type().toString().c_str(), binding->name().c_str());
                return Type::Error();
            }
        } else {
            // If the variable was unspecified before, replace it
            binding->setType(rhsType);
        }

        return Type::Void();
    } else {
        // Destructuring pattern: RHS must return a tuple
        if (!rhsType.isTuple()) {
            mReporter.errorf(expr->location(), "Destructuring requires a tuple expression on the right-hand side");
            return Type::Error();
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
                    if (binding->type().isSpecified()) {
                        if (binding->type().isVoid()) {
                            mReporter.errorf(expr->location(), "Cannot assign a 'void' to variable '%s'", binding->name().c_str());
                            return false;
                        }

                        if (!isConvertible(elemType, binding->type())) {
                            mReporter.errorf(expr->location(), "Cannot convert from '%s' to '%s' for variable '%s' declaration",
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

        if (registerVariables(*pattern, rhsType))
            return Type::Void(); //< In contrary to C/C++ we do not allow 'let k = (a = 44) * 4;' type of expressions.
        else
            return Type::Error();
    }
}

Type TypeChecker::handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::FunctionDeclarationStatement>& expr)
{
    if (expr->isExtern() || !expr->closure()) {
        // Do nothing
    } else {
        // Type-check the function body to determine the return type and process it
        const auto functionReturnType = handleNode(expr->closure());

        // The declared return type (potentially unspecified)
        if (!expr->functionReturnType().isSpecified())
            expr->setFunctionReturnType(functionReturnType);

        if (!functionReturnType.isSpecified()) {
            mReporter.errorf(expr->location(), "Could not determine return type for function '%s'", expr->name().c_str());
            return Type::Error();
        }

        closure->symbols().replaceFunction(FunctionDef(expr->name(), expr->mangledName(), expr->parameters(), expr->functionReturnType(), expr->isExtern(), expr->hasSideEffects(), expr->location()));
        if (closure->hasFinalExpression())
            expr->closure()->finalExpressionMut() = injectCastIfNeeded(expr->closure()->finalExpression(), expr->functionReturnType(), nullptr);
    }
    return Type::Void();
}

Type TypeChecker::handleNode(const Ptr<ast::Closure>& closure, const Ptr<ast::AssignmentExpression>& expr)
{
    // Type-check the RHS expression
    const auto rhsType = handleNode(closure, expr->rvalue());
    if (rhsType.isError())
        return Type::Error();

    // Check LHS type
    const auto lhs = expr->lvalue();

    if (lhs->type() == ExpressionType::Variable) {
        // Simple variable assignment: x = expr
        auto varExpr  = std::reinterpret_pointer_cast<VariableExpression>(lhs);
        auto variable = varExpr->variable();

        if (!variable->isMutable()) {
            mReporter.errorf(expr->location(), "Cannot assign to immutable variable '%s'", variable->name().c_str());
            return Type::Error();
        }

        // Check type compatibility
        const auto& varType = variable->type();
        if (varType.isVoid()) {
            mReporter.errorf(expr->location(), "Cannot assign a 'void' to variable '%s'", variable->name().c_str());
            return Type::Error();
        }

        if (!isConvertible(rhsType, varType)) {
            mReporter.errorf(expr->location(), "Cannot convert from '%s' to '%s' for variable '%s'",
                             rhsType.toString().c_str(), varType.toString().c_str(), variable->name().c_str());
            return Type::Error();
        }

        // Assignment expressions return the RHS type (like in C/C++)
        expr->setReturnType(rhsType);
        return rhsType;
    } else if (lhs->type() == ExpressionType::Tuple) {
        // Destructuring tuple assignment: [a, b] = expr
        auto tupleExpr = std::reinterpret_pointer_cast<TupleExpression>(lhs);

        // Destructuring assignment: RHS must be a tuple
        if (!rhsType.isTuple()) {
            mReporter.errorf(expr->location(), "Destructuring requires a tuple expression on the right-hand side");
            return Type::Error();
        }

        // Helper function to recursively check nested tuple assignments
        std::function<bool(const Ptr<Expression>&, const Type&)> checkTupleAssignment =
            [&](const Ptr<Expression>& lhsElem, const Type& rhsElemType) -> bool {
            if (lhsElem->type() == ExpressionType::Variable) {
                // Simple variable in tuple: [x, y] = [1, 2]
                auto varExpr  = std::reinterpret_pointer_cast<VariableExpression>(lhsElem);
                auto variable = varExpr->variable();

                if (!variable->isMutable()) {
                    mReporter.errorf(lhsElem->location(), "Cannot assign to immutable variable '%s'", variable->name().c_str());
                    return false;
                }

                // Check type compatibility
                const auto& varType = variable->type();
                if (varType.isVoid()) {
                    mReporter.errorf(lhsElem->location(), "Cannot assign a 'void' to variable '%s'", variable->name().c_str());
                    return false;
                }

                if (!isConvertible(rhsElemType, varType)) {
                    mReporter.errorf(lhsElem->location(), "Cannot convert tuple element from '%s' to '%s' for variable '%s'",
                                     rhsElemType.toString().c_str(), varType.toString().c_str(), variable->name().c_str());
                    return false;
                }
                return true;
            } else if (lhsElem->type() == ExpressionType::Tuple) {
                // Nested tuple in tuple: [[x, y], z] = [[1, 2], 3]
                auto nestedTupleExpr = std::reinterpret_pointer_cast<TupleExpression>(lhsElem);

                if (!rhsElemType.isTuple()) {
                    mReporter.errorf(lhsElem->location(), "Nested tuple destructuring requires a tuple type");
                    return false;
                }

                const auto& nestedLhsEntries    = nestedTupleExpr->entries();
                const auto& nestedRhsComponents = rhsElemType.components();

                if (nestedLhsEntries.size() != nestedRhsComponents.size()) {
                    mReporter.errorf(lhsElem->location(), "Nested tuple size %zu does not match tuple size %zu",
                                     nestedLhsEntries.size(), nestedRhsComponents.size());
                    return false;
                }

                // Recursively check each nested element
                for (size_t j = 0; j < nestedLhsEntries.size(); ++j) {
                    if (!checkTupleAssignment(nestedLhsEntries[j], nestedRhsComponents[j]))
                        return false;
                }
                return true;
            } else {
                mReporter.errorf(lhsElem->location(), "Left-hand side of tuple assignment must be a variable or tuple");
                return false;
            }
        };

        // Check that tuple sizes match
        const auto& lhsEntries    = tupleExpr->entries();
        const auto& rhsComponents = rhsType.components();

        if (lhsEntries.size() != rhsComponents.size()) {
            mReporter.errorf(expr->location(), "Tuple size %zu does not match tuple size %zu", lhsEntries.size(), rhsComponents.size());
            return Type::Error();
        }

        // Check each element in the tuple LHS
        for (size_t i = 0; i < lhsEntries.size(); ++i) {
            if (!checkTupleAssignment(lhsEntries[i], rhsComponents[i]))
                return Type::Error();
        }

        // Assignment expressions return the RHS type (like in C/C++)
        expr->setReturnType(rhsType);
        return rhsType;
    } else {
        mReporter.errorf(lhs->location(), "Left-hand side of assignment must be a variable or tuple");
        return Type::Error();
    }
}

} // namespace PExpr::type
