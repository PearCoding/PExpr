#include "SSAMapper.h"
#include "Enums.h"
#include "Expression.h"
#include "Statement.h"

#include <algorithm>
#include <sstream>

namespace PExpr::ssa {

// Strings

static inline std::string_view toInstructionString(UnaryOperation op)
{
    switch (op) {
    case UnaryOperation::Pos:
        return "pos";
    case UnaryOperation::Neg:
        return "neg";
    case UnaryOperation::Not:
        return "not";
    default:
        PEXPR_ASSERT(false, "Invalid unary operation enum");
        return "";
    }
}

static inline std::string_view toInstructionString(BinaryOperation op)
{
    switch (op) {
    case BinaryOperation::Add:
        return "add";
    case BinaryOperation::Sub:
        return "sub";
    case BinaryOperation::Mul:
        return "mul";
    case BinaryOperation::Div:
        return "div";
    case BinaryOperation::Pow:
        return "pow";
    case BinaryOperation::Mod:
        return "mod";
    case BinaryOperation::And:
        return "and";
    case BinaryOperation::Or:
        return "or";
    case BinaryOperation::Less:
        return "ls";
    case BinaryOperation::Greater:
        return "gt";
    case BinaryOperation::LessEqual:
        return "le";
    case BinaryOperation::GreaterEqual:
        return "ge";
    case BinaryOperation::Equal:
        return "eq";
    case BinaryOperation::NotEqual:
        return "neq";
    default:
        PEXPR_ASSERT(false, "Invalid binary operation enum");
        return "";
    }
}

// SSAValue / Instr dumps

std::string SSAValue::toString(bool suffixType) const
{
    if (Name.empty())
        return std::string("_");
    if ((suffixType || this->Kind == Kind::Constant) && Type != PExpr::ElementaryType::Unspecified) {
        std::stringstream ss;
        ss << Name << ":" << std::string(PExpr::toString(Type));
        return ss.str();
    }
    return Name;
}

std::string SSAInstrAssign::dump() const
{
    std::stringstream ss;
    ss << Target.toString(true) << " = ";
    switch (Operator) {
    case OpKind::Assign:
        ss << "assign";
        break;
    case OpKind::Unary:
        ss << toInstructionString(UnaryOp);
        break;
    case OpKind::Binary:
        ss << toInstructionString(BinaryOp);
        break;
    case OpKind::Access:
        ss << "access[" << Swizzle << "]";
        break;
    case OpKind::Vector:
        ss << "vec[" << Operands.size() << "]";
        break;
    case OpKind::Nop:
        ss << "nop";
        break;
    case OpKind::Cast:
        ss << "cast";
        break;
    default:
        ss << "unknown";
        break;
    }
    ss << "(";
    for (size_t i = 0; i < Operands.size(); ++i) {
        if (i)
            ss << ", ";
        ss << Operands[i].toString(false);
    }
    ss << ")";
    return ss.str();
}

std::string SSAInstrCall::dump() const
{
    std::stringstream ss;
    ss << Target.toString(true) << " = call " << FunctionName << "(";
    for (size_t i = 0; i < Arguments.size(); ++i) {
        if (i)
            ss << ", ";
        ss << Arguments[i].toString(false);
    }
    ss << ")";
    return ss.str();
}

std::string SSAInstrReturn::dump() const
{
    std::stringstream ss;
    ss << "return " << Value.toString(false);
    return ss.str();
}

std::string SSAInstrPhi::dump() const
{
    std::stringstream ss;
    ss << Target.toString(true) << " = phi(";
    for (size_t i = 0; i < Sources.size(); ++i) {
        if (i)
            ss << ", ";
        ss << Sources[i].toString(false);
    }
    ss << ")";
    return ss.str();
}

std::string SSAFunction::dump() const
{
    std::stringstream ss;
    if (Body.empty())
        ss << "extern ";
    ss << "fn " << Name << "(";
    for (size_t i = 0; i < Parameters.size(); ++i) {
        if (i)
            ss << ", ";
        ss << Parameters[i];
    }
    ss << ")";
    if (ReturnType != PExpr::ElementaryType::Unspecified)
        ss << ":" << PExpr::toString(ReturnType);
    ss << std::endl;

    if (!Body.empty()) {
        for (const auto& f : InnerFunctions)
            ss << f.dump() << std::endl;
        for (const auto& instr : Body)
            ss << "  " << instr->dump() << std::endl;
        ss << "endfn" << std::endl;
    }
    return ss.str();
}

std::string SSAProgram::dump() const
{
    std::stringstream ss;
    for (const auto& f : Functions)
        ss << f.dump() << std::endl;
    for (const auto& instr : Body)
        ss << instr->dump() << std::endl;
    return ss.str();
}

// SSAMapper implementation

SSAMapper::SSAMapper() = default;

SSAProgram SSAMapper::map(const Ptr<Closure>& closure)
{
    mProgram = SSAProgram{};
    mCounters.clear();
    mExprValues.clear();
    mapClosure(closure);

    return mProgram;
}

std::string SSAMapper::fresh(const std::string& base)
{
    int& c = mCounters[base];
    ++c;
    std::stringstream ss;
    ss << base << "." << c;
    return ss.str();
}

SSAValue SSAMapper::handleCast(ElementaryType to, const SSAValue& from)
{
    if (to == from.Type || !isConvertible(from.Type, to))
        return from;

    SSAInstrAssign cast;
    SSAValue tgt(SSAValue::Kind::Temp, fresh("t"), to);
    cast.Target   = tgt;
    cast.Operator = SSAInstrAssign::OpKind::Cast;
    cast.Operands = { from };
    mProgram.Body.push_back(std::make_shared<SSAInstrAssign>(cast));
    return tgt;
}

void SSAMapper::mapClosure(const Ptr<Closure>& closure)
{
    if (!closure)
        return;

    // map statements
    for (const auto& stmt : closure->statements())
        mapStatement(stmt);

    // map final expression as return
    if (closure->expression()) {
        SSAValue v = mapExpression(closure->expression());
        auto ret   = std::make_shared<SSAInstrReturn>();
        ret->Value = v;
        mProgram.Body.push_back(ret);
    }
}

void SSAMapper::mapStatement(const Ptr<Statement>& stmt)
{
    if (!stmt)
        return;

    switch (stmt->type()) {
    case StatementType::VariableDeclaration: {
        auto var     = std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt);
        SSAValue rhs = mapExpression(var->expression());

        SSAInstrAssign asg;
        SSAValue tgt(SSAValue::Kind::Named, fresh(var->name()), rhs.Type);
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Assign;
        asg.Operands = { rhs };
        mProgram.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        // remember mapping for this statement's expression pointer, so subsequent uses can reuse name
        mExprValues[stmt.get()] = tgt;
    } break;
    case StatementType::VariableAssignment: {
        auto var     = std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt);
        SSAValue rhs = mapExpression(var->expression());

        SSAInstrAssign asg;
        SSAValue tgt(SSAValue::Kind::Named, fresh(var->name()), rhs.Type);
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Assign;
        asg.Operands = { rhs };
        mProgram.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        // remember mapping for this statement's expression pointer, so subsequent uses can reuse name
        mExprValues[stmt.get()] = tgt;
    } break;
    case StatementType::FunctionDeclaration: {
        auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);
        SSAFunction func;
        func.Name = f->mangledName();
        func.Parameters.reserve(f->parameters().size());
        for (const auto& p : f->parameters())
            func.Parameters.push_back(p.Name);
        func.ReturnType = f->returnType();
        func.External   = f->isExtern();

        // map function body using a nested mapper so temporaries are local
        if (f->expression() && f->expression()->type() == ExpressionType::Closure) {
            auto closureExpr = std::reinterpret_pointer_cast<ClosureExpression>(f->expression());
            SSAMapper inner;
            auto innerProg = inner.map(closureExpr->closure());
            // move innerProg.mainBody into func.body
            for (auto& instr : innerProg.Body)
                func.Body.push_back(instr);
        } else {
            // if body is not a closure, map expression into a single return instr inside function
            if (f->expression()) {
                SSAMapper inner;
                Ptr<Closure> tmp = std::make_shared<Closure>(f->expression()->location(), nullptr);
                tmp->setExpression(f->expression());
                auto innerProg = inner.map(tmp);
                for (auto& instr : innerProg.Body)
                    func.Body.push_back(instr);
            }
        }

        mProgram.Functions.push_back(std::move(func));
    } break;
    default:
        // unsupported - emit comment as an assign to a dummy temp
        {
            SSAInstrAssign asg;
            asg.Target   = SSAValue(SSAValue::Kind::Temp, fresh("tmp"), ElementaryType::Unspecified);
            asg.Operator = SSAInstrAssign::OpKind::Nop;
            mProgram.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        }
        break;
    }
}

SSAValue SSAMapper::mapExpression(const Ptr<Expression>& expr)
{
    if (!expr)
        return SSAValue{ SSAValue::Kind::Constant, "nil", ElementaryType::Unspecified };

    // reuse cached value if present
    auto it = mExprValues.find(expr.get());
    if (it != mExprValues.end())
        return it->second;

    SSAValue result;

    switch (expr->type()) {
    case ExpressionType::Literal: {
        auto lit = std::reinterpret_pointer_cast<LiteralExpression>(expr);
        std::string sval;
        SSAValue v;
        v.Kind = SSAValue::Kind::Constant;
        v.Type = lit->returnType();
        switch (lit->returnType()) {
        case ElementaryType::Boolean:
            sval    = lit->getBool() ? "true" : "false";
            v.Name  = sval;
            v.Value = ExtendedValueVariant(lit->getBool());
            break;
        case ElementaryType::Integer:
            sval    = std::to_string(lit->getInteger());
            v.Name  = sval;
            v.Value = ExtendedValueVariant(static_cast<Integer>(lit->getInteger()));
            break;
        case ElementaryType::Number:
            sval    = std::to_string(lit->getNumber());
            v.Name  = sval;
            v.Value = ExtendedValueVariant(static_cast<Number>(lit->getNumber()));
            break;
        case ElementaryType::String:
            sval    = std::string("\"") + lit->getString() + "\"";
            v.Name  = sval;
            v.Value = ExtendedValueVariant(lit->getString());
            break;
        default:
            sval    = "unknown";
            v.Name  = sval;
            v.Value = ExtendedValueVariant(std::string());
        }
        result = v;
    } break;
    case ExpressionType::Variable: {
        auto v = std::reinterpret_pointer_cast<VariableExpression>(expr);
        // prefer last SSA version if present in counters, else plain name
        auto cit = mCounters.find(v->name());
        if (cit != mCounters.end()) {
            std::stringstream ss;
            ss << v->name() << "." << cit->second;
            result = SSAValue(SSAValue::Kind::Named, ss.str(), v->returnType());
        } else {
            result = SSAValue(SSAValue::Kind::Named, v->name(), v->returnType());
        }
    } break;
    case ExpressionType::Vector: {
        auto v = std::reinterpret_pointer_cast<VectorExpression>(expr);
        std::vector<SSAValue> inners;
        inners.reserve(v->entries().size());
        for (const auto& e : v->entries())
            inners.push_back(handleCast(ElementaryType::Number, mapExpression(e)));

        SSAValue tgt(SSAValue::Kind::Temp, fresh("t"), v->returnType());
        SSAInstrAssign asg;
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Vector;
        asg.Operands = std::move(inners);
        mProgram.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        result = tgt;
    } break;
    case ExpressionType::Unary: {
        auto u         = std::reinterpret_pointer_cast<UnaryExpression>(expr);
        SSAValue inner = mapExpression(u->inner());
        SSAValue tgt(SSAValue::Kind::Temp, fresh("t"), u->returnType());
        SSAInstrAssign asg;
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Unary;
        asg.UnaryOp  = u->op();
        asg.Operands = { inner };
        mProgram.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        result = tgt;
    } break;
    case ExpressionType::Binary: {
        auto b     = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        SSAValue L = mapExpression(b->left());
        SSAValue R = mapExpression(b->right());
        SSAValue tgt(SSAValue::Kind::Temp, fresh("t"), b->returnType());
        SSAInstrAssign asg;
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Binary;
        asg.BinaryOp = b->op();
        asg.Operands = { L, R };
        mProgram.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        result = tgt;
    } break;
    case ExpressionType::Call: {
        auto c = std::reinterpret_pointer_cast<CallExpression>(expr);
        std::vector<SSAValue> args;
        args.reserve(c->parameters().size());
        for (const auto& p : c->parameters())
            args.push_back(mapExpression(p)); // TODO: Implicit casts

        PEXPR_ASSERT(!c->mangledName().empty(), "The typechecker must run before the SSAMapper and assign valid mangled names to function calls!");
        SSAValue tgt(SSAValue::Kind::Temp, fresh(c->name()), c->returnType());
        auto call                = std::make_shared<SSAInstrCall>();
        call->Target             = tgt;
        call->FunctionName       = c->mangledName();
        call->PublicFunctionName = c->name();
        call->Arguments          = args;
        mProgram.Body.push_back(call);
        result = tgt;
    } break;
    case ExpressionType::Access: {
        auto a      = std::reinterpret_pointer_cast<AccessExpression>(expr);
        SSAValue in = mapExpression(a->inner());
        SSAValue tgt(SSAValue::Kind::Temp, fresh("t"), a->returnType());
        SSAInstrAssign asg;
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Access;
        asg.Swizzle  = a->swizzle();
        asg.Operands = { in };
        mProgram.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        result = tgt;
    } break;
    case ExpressionType::Closure: {
        auto c = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        // Map nested closure as a function-like entity and return a temp referencing it.
        SSAMapper inner;
        auto prog = inner.map(c->closure());
        // create a synthetic function name and register it as a function in program
        const std::string funcName = fresh("closure");
        SSAFunction func;
        func.Name       = funcName;
        func.ReturnType = c->returnType();
        func.External   = false;
        // move inner instructions into function body
        for (auto& instr : prog.Body)
            func.Body.push_back(instr);
        func.InnerFunctions = std::move(prog.Functions);
        mProgram.Functions.push_back(std::move(func));

        // directly call the closure
        SSAValue tgt(SSAValue::Kind::Temp, fresh(funcName), c->returnType());
        auto call                = std::make_shared<SSAInstrCall>();
        call->Target             = tgt;
        call->PublicFunctionName = funcName;
        call->FunctionName       = funcName;
        call->Arguments          = {}; // empty
        mProgram.Body.push_back(call);
        result = tgt;
    } break;
    case ExpressionType::Branch: {
        auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);
        // Map each branch and collect their return values (if any)
        std::vector<SSAValue> branchVals;
        for (const auto& b : br->branches()) {
            SSAMapper inner;
            auto prog = inner.map(b.Body);
            // append branch instructions as inline (for now)
            for (const auto& instr : prog.Body)
                mProgram.Body.push_back(instr);
            // try to extract last return value if present
            SSAValue lastVal = SSAValue(SSAValue::Kind::Constant, "nil", ElementaryType::Unspecified);
            if (!prog.Body.empty()) {
                // inspect last instr; if SSAInstrReturn, use its value
                auto last = prog.Body.back();
                if (auto ret = dynamic_cast<SSAInstrReturn*>(last.get()))
                    lastVal = ret->Value;
            }
            branchVals.push_back(lastVal);
        }
        // else closure
        SSAMapper inner;
        auto elseProg = inner.map(br->elseClosure());
        for (const auto& instr : elseProg.Body)
            mProgram.Body.push_back(instr);
        SSAValue elseVal = SSAValue(SSAValue::Kind::Constant, "nil", ElementaryType::Unspecified);
        if (!elseProg.Body.empty()) {
            auto last = elseProg.Body.back();
            if (auto ret = dynamic_cast<SSAInstrReturn*>(last.get()))
                elseVal = ret->Value;
        }
        // create phi
        SSAValue tgt(SSAValue::Kind::Temp, fresh("phi"), branchVals.front().Type);
        auto phi     = std::make_shared<SSAInstrPhi>();
        phi->Target  = tgt;
        phi->Sources = branchVals;
        phi->Sources.push_back(elseVal);
        mProgram.Body.push_back(phi);
        result = tgt;
    } break;
    default:
        result = SSAValue{ SSAValue::Kind::Constant, "unknown", ElementaryType::Unspecified };
        break;
    }

    // attach type from AST (if typechecker ran)
    result.Type = expr->returnType();

    // cache result
    mExprValues[expr.get()] = result;
    return result;
}

} // namespace PExpr::ssa
