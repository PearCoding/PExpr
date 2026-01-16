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

std::string SSAValue::toString(bool showType) const
{
    std::string prefix;
    if (this->Kind == Kind::Constant) {
        switch (this->Type) {
        case ElementaryType::Boolean:
            prefix = std::get<bool>(Value) ? "true" : "false";
            break;
        case ElementaryType::Integer:
            prefix = std::to_string(std::get<Integer>(Value));
            break;
        case ElementaryType::Number:
            prefix = std::to_string(std::get<Number>(Value));
            break;
        case ElementaryType::String:
            prefix = "\"" + std::get<std::string>(Value) + "\"";
            break;
        default:
            if (this->Type >= ElementaryType::Vec1) {
                const auto v = std::get<VecN>(Value);
                PEXPR_ASSERT(v.size() == typeArraySize(this->Type), "Vector data and vector type missmatch");

                if (v.empty()) {
                    prefix = "[]";
                } else {
                    prefix = "[" + std::to_string(v[0]);
                    for (size_t i = 1; i < v.size(); ++i)
                        prefix += "," + std::to_string(v[i]);
                    prefix += "]";
                }
            } else {
                PEXPR_ASSERT(false, "Expected specified type for SSAValue constants");
            }
        }
    } else {
        prefix = Name;
    }

    if (prefix.empty())
        return std::string("_");

    if ((showType || this->Kind == Kind::Constant) && Type != PExpr::ElementaryType::Unspecified) {
        std::stringstream ss;
        ss << prefix << ":" << std::string(PExpr::toString(Type));
        return ss.str();
    }
    return prefix;
}

std::string SSAValue::baseName() const
{
    PEXPR_ASSERT(Kind == SSAValue::Kind::Named, "Only named values have a base name");
    if (Name.empty())
        return std::string();
    auto pos = Name.find('.');
    if (pos == std::string::npos)
        return Name;
    return Name.substr(0, pos);
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
    case OpKind::Swizzle:
        ss << "swizzle[" << Swizzle << "]";
        break;
    case OpKind::Access:
        ss << "access";
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
    ss << Target.toString(true) << " = call[" << FunctionName << "](";
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

std::string SSAInstrLabel::dump() const
{
    return std::string(Name + ":");
}

std::string SSAInstrBranch::dump() const
{
    std::stringstream ss;
    ss << "br " << Condition.toString(false) << " -> " << TargetLabel;
    return ss.str();
}

std::string SSAInstrGoto::dump() const
{
    return std::string("goto " + TargetLabel);
}

std::string SSAInstrPhi::dump() const
{
    std::stringstream ss;
    ss << Target.toString(true) << " = phi[";
    for (size_t i = 0; i < Conditions.size(); ++i) {
        if (i)
            ss << ", ";
        ss << Conditions[i].toString(false);
    }
    ss << "](";
    for (size_t i = 0; i < Branches.size(); ++i) {
        if (i)
            ss << ", ";
        ss << Branches[i].toString(false);
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
    mCounters.clear();
    mExprValues.clear();
    mScopeStack.clear();
    return mapClosure(closure);
}

std::string SSAMapper::fresh(const std::string& base, bool updateScope)
{
    int& c = mCounters[base];
    ++c;
    if (updateScope && !mScopeStack.empty())
        currentScope()[base] = c;
    std::stringstream ss;
    ss << base << "." << c;
    return ss.str();
}

void SSAMapper::pushScope()
{
    if (mScopeStack.empty())
        mScopeStack.emplace_back();
    else
        mScopeStack.push_back(currentScope()); // copy current scope
}

void SSAMapper::popScope()
{
    PEXPR_ASSERT(!mScopeStack.empty(), "Scope stack underflow");
    mScopeStack.pop_back();
}

int SSAMapper::getCurrentVersion(const std::string& base) const
{
    if (mScopeStack.empty())
        return 0;
    const auto& scope = currentScope();
    if (const auto it = scope.find(base); it != scope.end())
        return it->second;
    return 0;
}

std::unordered_map<std::string, int>& SSAMapper::currentScope()
{
    PEXPR_ASSERT(!mScopeStack.empty(), "No scope active");
    return mScopeStack.back();
}

const std::unordered_map<std::string, int>& SSAMapper::currentScope() const
{
    PEXPR_ASSERT(!mScopeStack.empty(), "No scope active");
    return mScopeStack.back();
}

// Inline a mapped closure body into the current program by replacing any
// SSAInstrReturn instructions with assignments to a fresh temporary variable.
// Returns the SSAValue representing the last returned value (or a nil constant).
SSAValue SSAMapper::inlineClosureBody(SSAProgram& program, const std::vector<std::shared_ptr<SSAInstr>>& body)
{
    SSAValue lastVal = SSAValue(SSAValue::Kind::Constant, "nil", ElementaryType::Error);

    for (const auto& instr : body) {
        if (auto ret = dynamic_cast<SSAInstrReturn*>(instr.get())) {
            // create assignment to capture returned value
            SSAInstrAssign asg;
            SSAValue tgt(SSAValue::Kind::Temp, fresh("%"), ret->Value.Type);
            asg.Target   = tgt;
            asg.Operator = SSAInstrAssign::OpKind::Assign;
            asg.Operands = { ret->Value };
            program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
            lastVal = tgt;
        } else {
            // non-return instructions are appended as-is
            program.Body.push_back(instr);
        }
    }

    return lastVal;
}

SSAProgram SSAMapper::mapClosure(const Ptr<Closure>& closure)
{
    if (!closure)
        return SSAProgram{};

    pushScope();
    auto program = SSAProgram{};

    // map statements
    for (const auto& stmt : closure->statements())
        mapStatement(program, stmt);

    // map final expression as return
    if (closure->expression()) {
        SSAValue v = mapExpression(program, closure->expression());
        auto ret   = std::make_shared<SSAInstrReturn>();
        ret->Value = v;
        program.Body.push_back(ret);
    }

    popScope();
    return program;
}

void SSAMapper::mapStatement(SSAProgram& program, const Ptr<Statement>& stmt)
{
    if (!stmt)
        return;

    switch (stmt->type()) {
    case StatementType::VariableDeclaration: {
        auto var     = std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt);
        SSAValue rhs = mapExpression(program, var->expression());

        SSAInstrAssign asg;
        SSAValue tgt(SSAValue::Kind::Named, fresh(var->name(), true), rhs.Type);
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Assign;
        asg.Operands = { rhs };
        program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        // remember mapping for this statement's expression pointer, so subsequent uses can reuse name
        mExprValues[stmt.get()] = tgt;
    } break;
    case StatementType::VariableAssignment: {
        auto var     = std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt);
        SSAValue rhs = mapExpression(program, var->expression());

        SSAInstrAssign asg;
        SSAValue tgt(SSAValue::Kind::Named, fresh(var->name(), true), rhs.Type);
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Assign;
        asg.Operands = { rhs };
        program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        // remember mapping for this statement's expression pointer, so subsequent uses can reuse name
        mExprValues[stmt.get()] = tgt;
    } break;
    case StatementType::FunctionDeclaration: {
        auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);

        // Prepare SSAFunction entry up-front and insert into program so recursive
        // calls (or other functions mapping) can see the function entry.
        SSAFunction func;
        func.Name = f->mangledName();
        func.Parameters.reserve(f->parameters().size());
        for (const auto& p : f->parameters())
            func.Parameters.push_back(p.Name);
        func.ReturnType = f->returnType();
        func.External   = f->isExtern();

        // Acquire inner closure
        auto innerProg = mapClosure(f->closure());
        func.Body      = std::move(innerProg.Body);

        // Given the uplifting all functions are global
        program.Functions.insert(program.Functions.end(), innerProg.Functions.begin(), innerProg.Functions.end());
        program.Functions.push_back(std::move(func));
    } break;
    default:
        // unsupported - emit comment as an assign to a dummy temp
        {
            SSAInstrAssign asg;
            asg.Target   = SSAValue(SSAValue::Kind::Temp, fresh("tmp"), ElementaryType::Unspecified);
            asg.Operator = SSAInstrAssign::OpKind::Nop;
            program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        }
        break;
    }
}

SSAValue SSAMapper::mapExpression(SSAProgram& program, const Ptr<Expression>& expr)
{
    if (!expr)
        return SSAValue{ SSAValue::Kind::Constant, "nil", ElementaryType::Error };

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
        auto v      = std::reinterpret_pointer_cast<VariableExpression>(expr);
        int version = getCurrentVersion(v->name());
        if (version > 0) {
            std::stringstream ss;
            ss << v->name() << "." << version;
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
            inners.push_back(mapExpression(program, e));

        SSAValue tgt(SSAValue::Kind::Temp, fresh("%"), v->returnType());
        SSAInstrAssign asg;
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Vector;
        asg.Operands = std::move(inners);
        program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        result = tgt;
    } break;
    case ExpressionType::Unary: {
        auto u         = std::reinterpret_pointer_cast<UnaryExpression>(expr);
        SSAValue inner = mapExpression(program, u->inner());
        SSAValue tgt(SSAValue::Kind::Temp, fresh("%"), u->returnType());
        SSAInstrAssign asg;
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Unary;
        asg.UnaryOp  = u->op();
        asg.Operands = { inner };
        program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        result = tgt;
    } break;
    case ExpressionType::Binary: {
        auto b     = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        SSAValue L = mapExpression(program, b->left());
        SSAValue R = mapExpression(program, b->right());
        SSAValue tgt(SSAValue::Kind::Temp, fresh("%"), b->returnType());
        SSAInstrAssign asg;
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Binary;
        asg.BinaryOp = b->op();
        asg.Operands = { L, R };
        program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        result = tgt;
    } break;
    case ExpressionType::Call: {
        auto c = std::reinterpret_pointer_cast<CallExpression>(expr);
        std::vector<SSAValue> args;
        args.reserve(c->parameters().size());
        for (const auto& p : c->parameters())
            args.push_back(mapExpression(program, p));

        PEXPR_ASSERT(!c->mangledName().empty(), "The typechecker must run before the SSAMapper and assign valid mangled names to function calls!");
        SSAValue tgt(SSAValue::Kind::Temp, fresh(c->name()), c->returnType());
        auto call                = std::make_shared<SSAInstrCall>();
        call->Target             = tgt;
        call->FunctionName       = c->mangledName();
        call->PublicFunctionName = c->name();
        call->Arguments          = args;
        program.Body.push_back(call);
        result = tgt;
    } break;
    case ExpressionType::Swizzle: {
        auto a      = std::reinterpret_pointer_cast<SwizzleExpression>(expr);
        SSAValue in = mapExpression(program, a->inner());
        SSAValue tgt(SSAValue::Kind::Temp, fresh("%"), a->returnType());
        SSAInstrAssign asg;
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Swizzle;
        asg.Swizzle  = a->swizzle();
        asg.Operands = { in };
        program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        result = tgt;
    } break;
    case ExpressionType::Access: {
        auto a       = std::reinterpret_pointer_cast<AccessExpression>(expr);
        SSAValue in  = mapExpression(program, a->inner());
        SSAValue cst = SSAValue::Constant((Integer)a->index());
        SSAValue tgt(SSAValue::Kind::Temp, fresh("%"), a->returnType());
        SSAInstrAssign asg;
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Access;
        asg.Operands = { in, cst };
        program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        result = tgt;
    } break;
    case ExpressionType::Cast: {
        auto c = std::reinterpret_pointer_cast<CastExpression>(expr);
        // Map inner expression and emit an SSA cast instruction
        SSAValue inner = mapExpression(program, c->inner());
        // If both types match do nothing else typechecker should ensure correctness
        if (inner.Type == c->toType()) {
            result = inner;
        } else {
            SSAInstrAssign cast;
            SSAValue tgt(SSAValue::Kind::Temp, fresh("%"), c->toType());
            cast.Target   = tgt;
            cast.Operator = SSAInstrAssign::OpKind::Cast;
            cast.Operands = { inner };
            program.Body.push_back(std::make_shared<SSAInstrAssign>(cast));
            result = tgt;
        }
    } break;
    case ExpressionType::Closure: {
        auto c    = std::reinterpret_pointer_cast<ClosureExpression>(expr);
        auto prog = mapClosure(c->closure());
        result    = inlineClosureBody(program, prog.Body);
        // Given the uplifting all functions are global
        program.Functions.insert(program.Functions.end(), prog.Functions.begin(), prog.Functions.end());
    } break;
    case ExpressionType::Branch: {
        auto br = std::reinterpret_pointer_cast<BranchExpression>(expr);

        // Prepare labels for each branch, else and join
        std::vector<std::string> branchLabels;
        branchLabels.reserve(br->branches().size());
        for (size_t i = 0; i < br->branches().size(); ++i)
            branchLabels.push_back(fresh("lbl"));
        std::string elseLabel = fresh("lbl");
        std::string joinLabel = fresh("lbl");

        // Emit conditional branches for each branch condition that jump to their label
        std::vector<SSAValue> conditionVals;
        conditionVals.reserve(branchLabels.size());
        for (size_t i = 0; i < br->branches().size(); ++i) {
            const auto& single = br->branches()[i];
            // map condition expression
            SSAValue cond = mapExpression(program, single.Condition);
            conditionVals.push_back(cond);

            // emit branch instruction
            SSAInstrBranch bInstr;
            bInstr.Condition   = cond;
            bInstr.TargetLabel = branchLabels[i];
            program.Body.push_back(std::make_shared<SSAInstrBranch>(bInstr));
        }

        // If none matched, fall through to else label; emit else label and inline else body
        SSAInstrLabel elbl;
        elbl.Name = elseLabel;
        program.Body.push_back(std::make_shared<SSAInstrLabel>(elbl));
        // SSAMapper elseMapper;
        auto elseProg    = mapClosure(br->elseClosure());
        SSAValue elseVal = inlineClosureBody(program, elseProg.Body);
        // after else body jump to join
        SSAInstrGoto gToJoin;
        gToJoin.TargetLabel = joinLabel;
        program.Body.push_back(std::make_shared<SSAInstrGoto>(gToJoin));

        // Now emit each branch body under its label and jump to join after
        std::vector<SSAValue> branchVals;
        branchVals.reserve(branchLabels.size());
        for (size_t i = 0; i < br->branches().size(); ++i) {
            SSAInstrLabel lbl;
            lbl.Name = branchLabels[i];
            program.Body.push_back(std::make_shared<SSAInstrLabel>(lbl));

            // SSAMapper inner;
            auto prog        = mapClosure(br->branches()[i].Body);
            SSAValue lastVal = inlineClosureBody(program, prog.Body);
            branchVals.push_back(lastVal);

            SSAInstrGoto toJoin;
            toJoin.TargetLabel = joinLabel;
            program.Body.push_back(std::make_shared<SSAInstrGoto>(toJoin));
        }

        // Emit join label
        SSAInstrLabel jlbl;
        jlbl.Name = joinLabel;
        program.Body.push_back(std::make_shared<SSAInstrLabel>(jlbl));

        // Use the BranchExpression's declared return type as the phi node type.
        ElementaryType phiType = expr->returnType();

        // create phi target with chosen type
        SSAValue tgt(SSAValue::Kind::Temp, fresh("phi"), phiType);
        auto phi    = std::make_shared<SSAInstrPhi>();
        phi->Target = tgt;

        // Insert into phi block
        phi->Conditions = std::move(conditionVals);
        phi->Branches   = std::move(branchVals);
        phi->Branches.push_back(elseVal);

        program.Body.push_back(phi);
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
