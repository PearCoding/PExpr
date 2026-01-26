#include "SSAMapper.h"
#include "Enums.h"
#include "Expression.h"
#include "Statement.h"

#include <algorithm>
#include <ranges>
#include <sstream>

namespace PExpr::ssa {
SSAMapper::SSAMapper() = default;

SSAProgram SSAMapper::map(const Ptr<Closure>& closure)
{
    mContext.reset();
    mExprValues.clear();
    return mapClosure(closure);
}

// Inline a mapped closure body into the current program by replacing any
// SSAInstrReturn instructions with assignments to a fresh temporary variable.
// Returns the SSAValue representing the last returned value (or a nil constant).
SSAValue SSAMapper::inlineClosureBody(SSAProgram& program, const std::vector<std::shared_ptr<SSAInstr>>& body)
{
    SSAValue lastVal;

    for (const auto& instr : body) {
        if (auto ret = dynamic_cast<SSAInstrReturn*>(instr.get())) {
            // create assignment to capture returned value
            SSAInstrAssign asg;
            SSAValue tgt = SSAValue::Named(mContext.fresh("%"), ret->Value.type());
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
    PEXPR_ASSERT(closure, "Expected valid closure to map");

    mContext.pushScope();
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

    mContext.popScope();
    return program;
}

void SSAMapper::mapStatement(SSAProgram& program, const Ptr<Statement>& stmt)
{
    PEXPR_ASSERT(stmt, "Expected valid statement to map");

    switch (stmt->type()) {
    case StatementType::VariableDeclaration: {
        auto var     = std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt);
        SSAValue rhs = mapExpression(program, var->expression());

        SSAInstrAssign asg;
        SSAValue tgt = SSAValue::Named(mContext.fresh(var->name(), true), rhs.type());
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
        SSAValue tgt = SSAValue::Named(mContext.fresh(var->name(), true), rhs.type());
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
        func.ReturnType    = f->returnType();
        func.External      = f->isExtern();
        func.HasSideEffect = f->hasSideEffects();

        // Acquire inner closure if available
        if (f->closure()) {
            auto innerProg = mapClosure(f->closure());
            func.Body      = std::move(innerProg.Body);

            // Given the uplifting all functions are global
            program.Functions.insert(program.Functions.end(), innerProg.Functions.begin(), innerProg.Functions.end());
        }

        program.Functions.push_back(std::move(func));
    } break;
    default:
        PEXPR_ASSERT(false, "unsupported statement type");
        break;
    }
}

SSAValue SSAMapper::mapExpression(SSAProgram& program, const Ptr<Expression>& expr)
{
    PEXPR_ASSERT(expr, "Expected valid expression to map");

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
        switch (lit->returnType().kind()) {
        case TypeKind::Boolean:
            v = SSAValue::Constant(lit->getBool());
            break;
        case TypeKind::Integer:
            v = SSAValue::Constant(lit->getInteger());
            break;
        case TypeKind::Number:
            v = SSAValue::Constant(lit->getNumber());
            break;
        case TypeKind::String:
            v = SSAValue::Constant(lit->getString());
            break;
        default:
            PEXPR_ASSERT(false, "Unknown literal return type");
            break;
        }
        result = v;
    } break;
    case ExpressionType::Variable: {
        auto v      = std::reinterpret_pointer_cast<VariableExpression>(expr);
        int version = mContext.getCurrentVersion(v->name());
        if (version > 0) {
            std::stringstream ss;
            ss << v->name() << "." << version;
            result = SSAValue::Named(ss.str(), v->returnType());
        } else {
            result = SSAValue::Named(v->name(), v->returnType());
        }
    } break;
    case ExpressionType::Vector: {
        auto v = std::reinterpret_pointer_cast<VectorExpression>(expr);
        std::vector<SSAValue> inners;
        inners.reserve(v->entries().size());
        for (const auto& e : v->entries())
            inners.push_back(mapExpression(program, e));

        SSAValue tgt = SSAValue::Named(mContext.fresh("%"), v->returnType());
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
        SSAValue tgt   = SSAValue::Named(mContext.fresh("%"), u->returnType());
        SSAInstrAssign asg;
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Unary;
        asg.UnaryOp  = u->op();
        asg.Operands = { inner };
        program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        result = tgt;
    } break;
    case ExpressionType::Binary: {
        auto b       = std::reinterpret_pointer_cast<BinaryExpression>(expr);
        SSAValue L   = mapExpression(program, b->left());
        SSAValue R   = mapExpression(program, b->right());
        SSAValue tgt = SSAValue::Named(mContext.fresh("%"), b->returnType());
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
        SSAValue tgt             = SSAValue::Named(mContext.fresh(c->name()), c->returnType());
        auto call                = std::make_shared<SSAInstrCall>();
        call->Target             = tgt;
        call->FunctionName       = c->mangledName();
        call->PublicFunctionName = c->name();
        call->Arguments          = args;
        program.Body.push_back(call);
        result = tgt;
    } break;
    case ExpressionType::Swizzle: {
        auto a       = std::reinterpret_pointer_cast<SwizzleExpression>(expr);
        SSAValue in  = mapExpression(program, a->inner());
        SSAValue tgt = SSAValue::Named(mContext.fresh("%"), a->returnType());
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
        SSAValue tgt = SSAValue::Named(mContext.fresh("%"), a->returnType());
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
        if (inner.type() == c->toType()) {
            result = inner;
        } else {
            SSAInstrAssign cast;
            SSAValue tgt  = SSAValue::Named(mContext.fresh("%"), c->toType());
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
            branchLabels.push_back(mContext.fresh("lbl"));
        std::string elseLabel = mContext.fresh("lbl");
        std::string joinLabel = mContext.fresh("lbl");

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
        const auto phiType = expr->returnType();

        // create phi target with chosen type
        SSAValue tgt = SSAValue::Named(mContext.fresh("phi"), phiType);
        auto phi     = std::make_shared<SSAInstrPhi>();
        phi->Target  = tgt;

        // Insert into phi block
        phi->Conditions = std::move(conditionVals);
        phi->Branches   = std::move(branchVals);
        phi->Branches.push_back(elseVal);

        program.Body.push_back(phi);
        result = tgt;
    } break;
    default:
        PEXPR_ASSERT(false, "Unknown expression type");
        break;
    }

    // cache result
    mExprValues[expr.get()] = result;
    return result;
}

} // namespace PExpr::ssa
