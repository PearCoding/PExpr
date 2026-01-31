#include "SSAMapper.h"
#include "SSALiveAnalyzer.h"
#include "ast/Expression.h"
#include "ast/Pattern.h"
#include "ast/Statement.h"

#include <algorithm>
#include <ranges>
#include <sstream>

namespace PExpr::ssa {
using namespace ast;
using namespace type;

SSAMapper::SSAMapper(utils::Reporter& reporter)
    : mReporter(reporter)
{
}

SSAProgram SSAMapper::map(const Ptr<Closure>& closure)
{
    mContext.reset();
    mExprValues.clear();
    return mapClosure(closure);
}

SSAValue SSAMapper::castIfNeeded(SSAProgram& program, const parser::Location& loc, const SSAValue& fromValue, const Type& toType)
{
    if (fromValue.type() != toType) {
        SSAValue casted = SSAValue::Named(mContext.fresh("%"), toType);
        SSAInstrAssign cast;
        cast.Target   = casted;
        cast.Operator = SSAInstrAssign::OpKind::Cast;
        cast.Operands = { fromValue };
        program.Body.push_back(std::make_shared<SSAInstrAssign>(cast));

        // Emit warning for implicit cast
        utils::ReportType rt = utils::RT_WARNING_IMPLICIT_CAST;
        // Special case: `int` literal for a `num` variable
        if (toType.kind() == TypeKind::Number && fromValue.type().kind() == TypeKind::Integer)
            rt = utils::RT_WARNING_IMPLICIT_CAST_INT;

        mReporter.warningf(rt, loc,
                           "Implicitly converting from '%s' to '%s'",
                           fromValue.type().toString().c_str(), toType.toString().c_str());

        return casted;
    } else {
        return fromValue;
    }
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

            // Update context with variable names from the inlined instruction
            // This ensures that subsequent variable references use the correct version
            instr->forEachValue([this](const SSAValue& val) {
                if (!val.isConstant())
                    mContext.update(val);
            });
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
        auto declStmt       = std::reinterpret_pointer_cast<VariableDeclarationStatement>(stmt);
        SSAValue rhs        = mapExpression(program, declStmt->expression());
        const auto& pattern = declStmt->pattern();

        // TODO: Rework this
        // Check if pattern is a single simple binding (i.e., regular variable declaration)
        if (pattern->size() == 1 && pattern->elements()[0].isSimpleBinding()) {
            const auto varDef = pattern->elements()[0].simpleBinding();

            PEXPR_ASSERT(varDef->type().kind() != TypeKind::Unspecified, "All unspecified types must be specified in the SSA stage.");

            // Cast if needed
            SSAValue finalVal = castIfNeeded(program, pattern->elements()[0].location(), rhs, varDef->type());

            // Assign to variable (fresh name for declaration)
            SSAInstrAssign asg;
            SSAValue tgt = SSAValue::Named(mContext.fresh(varDef->uniqueName(), true), finalVal.type());
            asg.Target   = tgt;
            asg.Operator = SSAInstrAssign::OpKind::Assign;
            asg.Operands = { finalVal };
            program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        } else {
            // Destructuring pattern: RHS must be a tuple
            // Get tuple type and component types
            const Type& tupleType = rhs.type();
            PEXPR_ASSERT(tupleType.isTuple(), "Destructuring requires tuple type");

            // Helper function to recursively process pattern elements
            std::function<void(const Pattern&, SSAValue, const Type&)> processPattern =
                [&](const Pattern& pattern, SSAValue tupleValue, const Type& tupleType) -> void {
                PEXPR_ASSERT(tupleType.isTuple(), "Expected tuple type for destructuring");
                const auto& components = tupleType.components();

                for (size_t i = 0; i < pattern.size(); ++i) {
                    const auto& elem     = pattern.elements()[i];
                    const Type& elemType = components.at(i);

                    // Extract tuple element using Access operation
                    SSAValue index   = SSAValue::Constant((Integer)(i));
                    SSAValue elemVal = SSAValue::Named(mContext.fresh("%"), elemType);
                    {
                        SSAInstrAssign access;
                        access.Target   = elemVal;
                        access.Operator = SSAInstrAssign::OpKind::Access;
                        access.Operands = { tupleValue, index };
                        program.Body.push_back(std::make_shared<SSAInstrAssign>(access));
                    }

                    if (elem.isSimpleBinding()) {
                        const auto varDef = elem.simpleBinding();

                        PEXPR_ASSERT(varDef->type().kind() != TypeKind::Unspecified, "All unspecified types must be specified in the SSA stage.");

                        // Cast if needed (can't be really handled in the TypeChecker)
                        SSAValue finalVal = castIfNeeded(program, elem.location(), elemVal, varDef->type());

                        // Assign to variable (fresh name for declaration)
                        SSAInstrAssign asg;
                        SSAValue tgt = SSAValue::Named(mContext.fresh(varDef->uniqueName(), true), finalVal.type());
                        asg.Target   = tgt;
                        asg.Operator = SSAInstrAssign::OpKind::Assign;
                        asg.Operands = { finalVal };
                        program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
                    } else {
                        // Nested pattern - recurse
                        processPattern(*elem.nestedPattern(), elemVal, elemType);
                    }
                }
            };

            processPattern(*pattern, rhs, tupleType);
        }
    } break;
    case StatementType::VariableAssignment: {
        auto assignStmt     = std::reinterpret_pointer_cast<VariableAssignmentStatement>(stmt);
        SSAValue rhs        = mapExpression(program, assignStmt->expression());
        const auto& pattern = assignStmt->pattern();

        // TODO: Rework this
        // Check if pattern is a single simple binding (i.e., regular variable assignment)
        if (pattern->size() == 1 && pattern->elements()[0].isSimpleBinding()) {
            const auto varDef = pattern->elements()[0].simpleBinding();

            // Cast if needed
            SSAValue finalVal = castIfNeeded(program, pattern->elements()[0].location(), rhs, varDef->type());

            // Assign to variable (fresh version for assignment)
            SSAInstrAssign asg;
            SSAValue tgt = SSAValue::Named(mContext.fresh(varDef->uniqueName(), true), finalVal.type());
            asg.Target   = tgt;
            asg.Operator = SSAInstrAssign::OpKind::Assign;
            asg.Operands = { finalVal };
            program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
        } else {
            // Destructuring pattern: RHS must be a tuple
            // Helper function to recursively process pattern elements for assignment
            std::function<void(const Pattern&, SSAValue)> processPattern =
                [&](const Pattern& pattern, SSAValue tupleValue) -> void {
                PEXPR_ASSERT(tupleValue.type().isTuple(), "Expected tuple type for destructuring");
                const auto& components = tupleValue.type().components();
                for (size_t i = 0; i < pattern.size(); ++i) {
                    const auto& elem     = pattern.elements()[i];
                    const Type& elemType = components.at(i);

                    // Extract tuple element using Access operation
                    SSAValue index   = SSAValue::Constant((Integer)i);
                    SSAValue elemVal = SSAValue::Named(mContext.fresh("%"), elemType);
                    {
                        SSAInstrAssign access;
                        access.Target   = elemVal;
                        access.Operator = SSAInstrAssign::OpKind::Access;
                        access.Operands = { tupleValue, index };
                        program.Body.push_back(std::make_shared<SSAInstrAssign>(access));
                    }

                    if (elem.isSimpleBinding()) {
                        const auto varDef = elem.simpleBinding();

                        // Cast if needed (can't be really handled in the TypeChecker)
                        SSAValue finalVal = castIfNeeded(program, elem.location(), elemVal, varDef->type());

                        // Assign to variable (fresh version for assignment)
                        SSAInstrAssign asg;
                        SSAValue tgt = SSAValue::Named(mContext.fresh(varDef->uniqueName(), true), finalVal.type());
                        asg.Target   = tgt;
                        asg.Operator = SSAInstrAssign::OpKind::Assign;
                        asg.Operands = { finalVal };
                        program.Body.push_back(std::make_shared<SSAInstrAssign>(asg));
                    } else {
                        // Nested pattern - recurse
                        processPattern(*elem.nestedPattern(), elemVal);
                    }
                }
            };

            processPattern(*pattern, rhs);
        }
    } break;
    case StatementType::FunctionDeclaration: {
        auto f = std::reinterpret_pointer_cast<FunctionDeclarationStatement>(stmt);

        // Prepare SSAFunction entry up-front and insert into program so recursive
        // calls (or other functions mapping) can see the function entry.
        SSAFunction func;
        func.Name = f->mangledName();
        func.Parameters.reserve(f->parameters().size());
        for (const auto& p : f->parameters())
            func.Parameters.push_back(p->uniqueName());
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
    case StatementType::TypeAlias:
        // Type aliases are compile-time only, nothing to map
        break;
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
        auto v          = std::reinterpret_pointer_cast<VariableExpression>(expr);
        const auto name = v->variable()->uniqueName();
        int version     = mContext.getCurrentVersion(name);
        if (version > 0) {
            std::stringstream ss;
            ss << name << "." << version;
            result = SSAValue::Named(ss.str(), v->returnType());
        } else {
            result = SSAValue::Named(name, v->returnType());
        }
    } break;
    case ExpressionType::Tuple: {
        auto v = std::reinterpret_pointer_cast<TupleExpression>(expr);
        std::vector<SSAValue> inners;
        inners.reserve(v->entries().size());
        for (const auto& e : v->entries())
            inners.push_back(mapExpression(program, e));

        SSAValue tgt = SSAValue::Named(mContext.fresh("%"), v->returnType());
        SSAInstrAssign asg;
        asg.Target   = tgt;
        asg.Operator = SSAInstrAssign::OpKind::Tuple;
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
        SSAValue tgt             = SSAValue::Named(mContext.fresh("%"), c->returnType());
        auto call                = std::make_shared<SSAInstrCall>();
        call->Target             = tgt;
        call->FunctionName       = c->mangledName();
        call->PublicFunctionName = c->name();
        call->Arguments          = args;
        program.Body.push_back(call);
        result = tgt;
    } break;
    case ExpressionType::Swizzle: {
        auto a                     = std::reinterpret_pointer_cast<SwizzleExpression>(expr);
        SSAValue in                = mapExpression(program, a->inner());
        const std::string& swizzle = a->swizzle();

        // Helper to map swizzle character to index
        auto charToIndex = [](char c) -> size_t {
            switch (c) {
            case 'x':
            case 'r':
                return 0;
            case 'y':
            case 'g':
                return 1;
            case 'z':
            case 'b':
                return 2;
            case 'w':
            case 'a':
                return 3;
            default:
                PEXPR_ASSERT(false, "Invalid swizzle character");
                return 0;
            }
        };

        if (swizzle.size() == 1) {
            // Single component access -> returns a number
            size_t idx        = charToIndex(swizzle[0]);
            SSAValue indexVal = SSAValue::Constant((Integer)idx);
            SSAValue tgt      = SSAValue::Named(mContext.fresh("%"), a->returnType());
            SSAInstrAssign access;
            access.Target   = tgt;
            access.Operator = SSAInstrAssign::OpKind::Access;
            access.Operands = { in, indexVal };
            program.Body.push_back(std::make_shared<SSAInstrAssign>(access));
            result = tgt;
        } else {
            // Multiple components -> create accesses then tuple
            std::vector<SSAValue> accessedValues;
            accessedValues.reserve(swizzle.size());
            for (char c : swizzle) {
                size_t idx        = charToIndex(c);
                SSAValue indexVal = SSAValue::Constant((Integer)idx);
                SSAValue elem     = SSAValue::Named(mContext.fresh("%"), Type(TypeKind::Number));
                SSAInstrAssign access;
                access.Target   = elem;
                access.Operator = SSAInstrAssign::OpKind::Access;
                access.Operands = { in, indexVal };
                program.Body.push_back(std::make_shared<SSAInstrAssign>(access));
                accessedValues.push_back(elem);
            }
            SSAValue tgt = SSAValue::Named(mContext.fresh("%"), a->returnType());
            SSAInstrAssign tuple;
            tuple.Target   = tgt;
            tuple.Operator = SSAInstrAssign::OpKind::Tuple;
            tuple.Operands = std::move(accessedValues);
            program.Body.push_back(std::make_shared<SSAInstrAssign>(tuple));
            result = tgt;
        }
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
        branchLabels.reserve(br->branches().size() + 1);
        for (size_t i = 0; i < br->branches().size(); ++i)
            branchLabels.push_back(mContext.fresh("lbl"));
        branchLabels.push_back(mContext.fresh("lbl")); // Else
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

        // Collect all variable updates from all branches and map all branches
        std::unordered_map<std::string, std::vector<SSAValue>> varUpdates;
        std::vector<SSAProgram> progs;
        progs.reserve(branchLabels.size() + 1);

        auto handleBranch = [&](const Ptr<Closure>& closure) {
            auto prog = mapClosure(closure);
            SSALiveAnalyzer branchAnalyzer;
            auto updates = branchAnalyzer.analyze(prog.Body);

            // For each variable updated in this branch, add its value
            for (const auto& val : updates) {
                const auto valName = val.baseName();

                // Do not include variables defined inside the branch closure itself
                if (mContext.getCurrentVersion(valName) < 0)
                    continue;

                // If this is the first time we see this variable in this branch position,
                // we need to fill in values for previous branches
                if (varUpdates.find(valName) == varUpdates.end()) {
                    int v = mContext.getCurrentVersion(valName);

                    // Initialize with empty values for previous branches
                    varUpdates[valName].resize(progs.size(), SSAValue::Named(valName + "." + std::to_string(v), val.type()));
                }
                varUpdates[valName].push_back(val);
            }

            // For variables we've seen before but not updated in this branch,
            // add empty placeholder (will be filled later)
            for (auto& [valName, vals] : varUpdates) {
                if (vals.size() == progs.size()) {
                    int v = mContext.getCurrentVersion(valName);

                    // This variable was updated in a previous branch but not this one
                    vals.push_back(SSAValue::Named(valName + "." + std::to_string(v), vals.at(0).type()));
                }
            }

            progs.push_back(prog);
        };

        for (const auto& b : br->branches())
            handleBranch(b.Body);
        handleBranch(br->elseClosure());

        std::vector<SSAValue> branchVals;
        branchVals.reserve(branchLabels.size() + 1);

        // Start with else and go back
        for (size_t i = progs.size(); i > 0; --i) {
            // Add the label
            SSAInstrLabel lbl;
            lbl.Name = branchLabels.at(i - 1);
            program.Body.push_back(std::make_shared<SSAInstrLabel>(lbl));

            // Inline the body
            SSAValue val = inlineClosureBody(program, progs.at(i - 1).Body);
            branchVals.push_back(val);

            // after body jump to join
            SSAInstrGoto gToJoin;
            gToJoin.TargetLabel = joinLabel;
            program.Body.push_back(std::make_shared<SSAInstrGoto>(gToJoin));
        }

        // Emit join label
        SSAInstrLabel jlbl;
        jlbl.Name = joinLabel;
        program.Body.push_back(std::make_shared<SSAInstrLabel>(jlbl));

        // Use the BranchExpression's declared return type as the phi node type.
        const auto phiType = expr->returnType();

        // create phi target with chosen type
        SSAValue tgt = SSAValue::Named(mContext.fresh("%"), phiType);
        auto phi     = std::make_shared<SSAInstrPhi>();
        phi->Target  = tgt;

        // Insert into phi block
        phi->Conditions = std::move(conditionVals);

        // Insert backwards as the inlined the `else` case first
        phi->Branches.reserve(branchVals.size());
        for (size_t i = branchVals.size(); i > 0; --i)
            phi->Branches.push_back(branchVals[i - 1]);

        program.Body.push_back(phi);

        // Create phi nodes for variables updated in all branches
        for (const auto& [varName, vals] : varUpdates) {
            // Create a phi node
            SSAValue phiTgt    = SSAValue::Named(mContext.fresh(varName, true), vals[0].type());
            auto varPhi        = std::make_shared<SSAInstrPhi>();
            varPhi->Target     = phiTgt;
            varPhi->Conditions = phi->Conditions; // Same conditions as main phi
            varPhi->Branches   = vals;
            program.Body.push_back(varPhi);
        }

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
