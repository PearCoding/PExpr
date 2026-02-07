#include "RVMMapper.h"
#include "RVMStructs.h"
#include "ast/Enums.h"

#include <span>

namespace PExpr::rvm {
using namespace ast;

// Constructor
RVMMapper::RVMMapper()
{
}

// Dissolve tuple type into elementary types
std::vector<type::Type> RVMMapper::dissolveTupleType(const type::Type& type)
{
    std::vector<type::Type> result;

    if (type.isTuple()) {
        for (const auto& component : type.components()) {
            auto dissolved = dissolveTupleType(component);
            result.insert(result.end(), dissolved.begin(), dissolved.end());
        }
    } else {
        // Elementary type
        result.push_back(type);
    }

    return result;
}

// Map SSA value to RVM value
RVMValue RVMMapper::mapValue(const ssa::SSAValue& ssaValue, RVMContext& context)
{
    if (ssaValue.isConstant()) {
        const auto& rawValue = ssaValue.rawValue();

        if (auto* strPtr = std::get_if<std::string>(&rawValue)) {
            // Lookup string constants
            if (auto it = mStringMap.find(*strPtr); it != mStringMap.end())
                return it->second;
            else
                PEXPR_ASSERT(false, "Collecting all string constants in RVM failed");
        } else if (std::get_if<Tuple>(&rawValue)) {
            PEXPR_ASSERT(false, "Tuple constants should be dissolved before mapping to RVM");
            return RVMValue::Constant(false); // unreachable
        } else if (auto* bVal = std::get_if<bool>(&rawValue)) {
            return RVMValue::Constant(*bVal);
        } else if (auto* iVal = std::get_if<Integer>(&rawValue)) {
            return RVMValue::Constant(*iVal);
        } else if (auto* nVal = std::get_if<Number>(&rawValue)) {
            return RVMValue::Constant(*nVal);
        }
    }

    // Check if this SSA value has already been mapped
    if (!ssaValue.isConstant()) {
        std::string ssaName = ssaValue.name();

        if (auto it = mSSAtoRVMMap.find(ssaName); it != mSSAtoRVMMap.end())
            return it->second;
    }

    // Register/named value - allocate register and store mapping
    RegId reg         = context.allocateRegister(ssaValue.type());
    RVMValue rvmValue = RVMValue::Register(reg, ssaValue.type());

    // Store the mapping for future reference
    if (!ssaValue.isConstant())
        mSSAtoRVMMap[ssaValue.name()] = rvmValue;

    return rvmValue;
}

// Map SSA assign instruction
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapAssign(
    const ssa::SSAInstrAssign& instr,
    RVMContext& context)
{
    std::vector<std::shared_ptr<RVMInstr>> result;

    switch (instr.Operator) {
    case ssa::SSAInstrAssign::OpKind::Assign: {
        // Simple assignment: dst = src
        PEXPR_ASSERT(instr.Operands.size() == 1, "Assign expects 1 operand");

        RVMValue dst = mapValue(instr.Target, context);
        RVMValue src = mapValue(instr.Operands[0], context);

        result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Unary: {
        // Unary operation
        PEXPR_ASSERT(instr.Operands.size() == 1, "Unary expects 1 operand");

        RVMValue dst = mapValue(instr.Target, context);
        RVMValue src = mapValue(instr.Operands[0], context);

        switch (instr.UnaryOp) {
        case UnaryOperation::Neg: {
            // For negation, we could add a NEG opcode or use 2-op SUB from zero
            // Using 0 - src with 3-operand for now
            RVMValue zero = instr.Target.type().kind() == type::TypeKind::Integer
                                ? RVMValue::Constant(Integer(0))
                                : RVMValue::Constant(Number(0.0));
            result.push_back(std::make_shared<RVMInstr3Op>(Opcode::SUB, dst, zero, src));
            break;
        }
        case UnaryOperation::Pos:
            // dst = src (no-op, just move)
            result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
            break;
        case UnaryOperation::Not: {
            // Logical not:  dst = !src
            // Using 2-operand with XOR would need a NOT opcode, for now use 3-op
            RVMValue one = RVMValue::Constant(Integer(1));
            result.push_back(std::make_shared<RVMInstr3Op>(Opcode::XOR, dst, src, one));
            break;
        }
        }
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Binary: {
        // Binary operation
        PEXPR_ASSERT(instr.Operands.size() == 2, "Binary expects 2 operands");

        RVMValue dst  = mapValue(instr.Target, context);
        RVMValue src1 = mapValue(instr.Operands[0], context);
        RVMValue src2 = mapValue(instr.Operands[1], context);

        Opcode op;
        switch (instr.BinaryOp) {
        case BinaryOperation::Add:
            op = Opcode::ADD;
            break;
        case BinaryOperation::Sub:
            op = Opcode::SUB;
            break;
        case BinaryOperation::Mul:
            op = Opcode::MUL;
            break;
        case BinaryOperation::Div:
            op = Opcode::DIV;
            break;
        case BinaryOperation::Mod:
            op = Opcode::MOD;
            break;
        case BinaryOperation::Equal:
            op = Opcode::CMP_EQ;
            break;
        case BinaryOperation::NotEqual:
            op = Opcode::CMP_NE;
            break;
        case BinaryOperation::Less:
            op = Opcode::CMP_LT;
            break;
        case BinaryOperation::LessEqual:
            op = Opcode::CMP_LE;
            break;
        case BinaryOperation::Greater:
            op = Opcode::CMP_GT;
            break;
        case BinaryOperation::GreaterEqual:
            op = Opcode::CMP_GE;
            break;
        case BinaryOperation::And:
            op = Opcode::AND;
            break;
        case BinaryOperation::Or:
            op = Opcode::OR;
            break;
        case BinaryOperation::Pow:
            op = Opcode::POW;
            break;
        }

        result.push_back(std::make_shared<RVMInstr3Op>(op, dst, src1, src2));
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Access: {
        // Tuple access should have been dissolved in SSA stage
        // For now, treat as a move (simplified)
        PEXPR_ASSERT(instr.Operands.size() == 2, "Access expects 2 operands (tuple, index)");

        RVMValue dst = mapValue(instr.Target, context);
        RVMValue src = mapValue(instr.Operands[0], context);

        // For RVM, tuple access should have been dissolved into direct register access
        result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Tuple: {
        // Tuple construction: dissolve into individual register assignments
        // Each tuple element maps to a separate register
        // For now, treat each element as a separate MOV instruction
        for (size_t i = 0; i < instr.Operands.size(); ++i) {
            RVMValue src = mapValue(instr.Operands[i], context);
            // For proper tuple dissolution, we'd need to track which register
            // corresponds to which tuple component. For now, just move to destination.
            RVMValue dst = mapValue(instr.Target, context);
            result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        }
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Cast: {
        // Type cast
        PEXPR_ASSERT(instr.Operands.size() == 1, "Cast expects 1 operand");

        RVMValue dst = mapValue(instr.Target, context);
        RVMValue src = mapValue(instr.Operands[0], context);

        const auto& srcType = instr.Operands[0].type();
        const auto& dstType = instr.Target.type();

        // Determine conversion opcode
        Opcode castOp = Opcode::MOV; // default to move if same type

        if (srcType.kind() == type::TypeKind::Integer && dstType.kind() == type::TypeKind::Number)
            castOp = Opcode::I2F;
        else if (srcType.kind() == type::TypeKind::Number && dstType.kind() == type::TypeKind::Integer)
            castOp = Opcode::F2I;
        else if (srcType.kind() == type::TypeKind::Boolean && dstType.kind() == type::TypeKind::Integer)
            castOp = Opcode::B2I;
        else if (srcType.kind() == type::TypeKind::Integer && dstType.kind() == type::TypeKind::Boolean)
            castOp = Opcode::I2B;
        else if (srcType.kind() == type::TypeKind::Number && dstType.kind() == type::TypeKind::Boolean)
            castOp = Opcode::F2B;
        else if (srcType.kind() == type::TypeKind::Boolean && dstType.kind() == type::TypeKind::Number)
            castOp = Opcode::B2F;

        result.push_back(std::make_shared<RVMInstr2Op>(castOp, dst, src));

        break;
    }
    }

    return result;
}

// Map SSA call instruction
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapCall(
    const ssa::SSAInstrCall& instr,
    RVMContext& context,
    const ssa::SSAProgram& ssaProgram)
{
    std::vector<std::shared_ptr<RVMInstr>> result;

    // Check if this is an internal or external function
    bool isExternal = true;
    for (const auto& func : ssaProgram.Functions) {
        if (func.Name == instr.FunctionName) {
            isExternal = func.External;
            break;
        }
    }

    if (isExternal) {
        // External call: pass arguments as part of the call instruction
        std::vector<RVMValue> args;
        args.reserve(instr.Arguments.size());

        for (const auto& arg : instr.Arguments)
            args.push_back(mapValue(arg, context));

        std::optional<RVMValue> dst;
        if (!instr.Target.type().isVoid())
            dst = mapValue(instr.Target, context);

        result.push_back(std::make_shared<RVMInstrExternalCall>(dst, instr.FunctionName, args));
    } else {
        // Internal call: use calling convention with registers
        // 1. Move arguments to registers %r1, %r2, etc.
        for (size_t i = 0; i < instr.Arguments.size(); ++i) {
            RVMValue src = mapValue(instr.Arguments[i], context);
            RVMValue dst = RVMValue::Register(i + 1, instr.Arguments[i].type()); // %r1, %r2, ...
            if (src.isRegister() && src.regId() == (i + 1))                      //< Already in the correct register, no move needed
                continue;

            result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        }

        // 2. Determine number of registers to save (max of args and return values)
        auto returnTypes       = dissolveTupleType(instr.Target.type());
        uint32_t numReturnRegs = returnTypes.empty() || returnTypes[0].isVoid() ? 0 : static_cast<uint32_t>(returnTypes.size());
        uint32_t numArgRegs    = static_cast<uint32_t>(instr.Arguments.size());
        uint32_t registerCount = std::max(numReturnRegs, numArgRegs);

        // 3. Push frame to save register context
        if (registerCount > 0)
            result.push_back(std::make_shared<RVMInstrPushFrame>(registerCount));

        // 4. Call internal function
        result.push_back(std::make_shared<RVMInstrInternalCall>(instr.FunctionName));

        // 5. Move return values from %r0, %r1, %r2, ... to destinations (if not void)
        if (!instr.Target.type().isVoid()) {
            // TODO: Nested tuples!
            if (instr.Target.type().isTuple()) {
                // Multiple return values: dissolve tuple
                auto retTypes = dissolveTupleType(instr.Target.type());
                for (size_t i = 0; i < retTypes.size(); ++i) {
                    RVMValue src = RVMValue::Register(i, retTypes[i]); // %r0, %r1, %r2, ...
                    // For tuples, we'd need to map each component separately
                    // This is simplified for now - proper implementation would track tuple components
                    RVMValue dst = mapValue(instr.Target, context);
                    result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
                }
            } else {
                // Single return value
                RVMValue dst = mapValue(instr.Target, context);
                if (!dst.isRegister() || dst.regId() != 0) {                   //< Only move if destination is not already %r0
                    RVMValue src = RVMValue::Register(0, instr.Target.type()); // %r0 holds return value
                    result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
                }
            }
        }

        // 6. Pop frame to restore register context
        if (registerCount > 0)
            result.push_back(std::make_shared<RVMInstrPopFrame>(registerCount));
    }

    return result;
}

// Map SSA return instruction
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapReturn(
    const ssa::SSAInstrReturn& instr,
    RVMContext& context)
{
    std::vector<std::shared_ptr<RVMInstr>> result;

    if (!instr.Value.type().isVoid()) {
        // TODO: Not working with nested types
        // Dissolve tuple return types
        auto retTypes = dissolveTupleType(instr.Value.type());

        if (retTypes.size() > 1 || instr.Value.type().isTuple()) {
            // Multiple return values: move each to %r0, %r1, %r2, ...
            for (size_t i = 0; i < retTypes.size(); ++i) {
                RVMValue src = mapValue(instr.Value, context);
                RVMValue dst = RVMValue::Register(i, retTypes[i]); // %r0, %r1, %r2, ...
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
            }
        } else {
            // Single return value: move to %r0
            RVMValue src = mapValue(instr.Value, context);
            if (!src.isRegister() || src.regId() != 0) {                  //< Only move if source is not already %r0
                RVMValue dst = RVMValue::Register(0, instr.Value.type()); // %r0
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
            }
        }
    }

    // Add the actual return instruction
    result.push_back(std::make_shared<RVMInstrReturn>(std::nullopt));

    return result;
}

// Map SSA branch instruction
std::shared_ptr<RVMInstr> RVMMapper::mapBranch(
    const ssa::SSAInstrBranch& instr,
    RVMContext& context)
{
    RVMValue cond = mapValue(instr.Condition, context);

    // Branch if not zero (condition is true)
    return std::make_shared<RVMInstrBranch>(Opcode::BRNZ, cond, instr.TargetLabel);
}

// Map SSA goto instruction
std::shared_ptr<RVMInstr> RVMMapper::mapGoto(const ssa::SSAInstrGoto& instr)
{
    return std::make_shared<RVMInstrJump>(instr.TargetLabel);
}

// Map SSA instructions to RVM instructions
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapInstructions(
    const std::vector<std::shared_ptr<ssa::SSAInstr>>& ssaInstrs,
    RVMContext& context,
    const ssa::SSAProgram& ssaProgram)
{
    std::vector<std::shared_ptr<RVMInstr>> result;

    for (const auto& ssaInstr : ssaInstrs) {
        // Handle label instructions
        if (auto* label = dynamic_cast<ssa::SSAInstrLabel*>(ssaInstr.get())) {
            // Emit label instruction for jump targets
            result.push_back(std::make_shared<RVMInstrLabel>(label->Name));
            continue;
        }

        // Handle assign instructions
        if (auto* assign = dynamic_cast<ssa::SSAInstrAssign*>(ssaInstr.get())) {
            auto instrs = mapAssign(*assign, context);
            result.insert(result.end(), instrs.begin(), instrs.end());
            continue;
        }

        // Handle call instructions
        if (auto* call = dynamic_cast<ssa::SSAInstrCall*>(ssaInstr.get())) {
            auto instrs = mapCall(*call, context, ssaProgram);
            result.insert(result.end(), instrs.begin(), instrs.end());
            continue;
        }

        // Handle return instructions
        if (auto* ret = dynamic_cast<ssa::SSAInstrReturn*>(ssaInstr.get())) {
            auto instrs = mapReturn(*ret, context);
            result.insert(result.end(), instrs.begin(), instrs.end());
            continue;
        }

        // Handle branch instructions
        if (auto* branch = dynamic_cast<ssa::SSAInstrBranch*>(ssaInstr.get())) {
            result.push_back(mapBranch(*branch, context));
            continue;
        }

        // Handle goto instructions
        if (auto* gotoInstr = dynamic_cast<ssa::SSAInstrGoto*>(ssaInstr.get())) {
            result.push_back(mapGoto(*gotoInstr));
            continue;
        }

        // Handle phi instructions
        if (auto* phi = dynamic_cast<ssa::SSAInstrPhi*>(ssaInstr.get())) {
            // Phi nodes select values based on conditions
            // Implementation: result = cond1 ? val1 : (cond2 ? val2 : ... elseVal)
            // We need to generate conditional branches for this
            RVMValue dst = mapValue(phi->Target, context);

            if (phi->Conditions.empty()) {
                // No conditions, just use the else branch (or first branch if no else)
                size_t branchIdx = phi->Branches.size() > 0 ? 0 : 0;
                if (branchIdx < phi->Branches.size()) {
                    RVMValue branchVal = mapValue(phi->Branches[branchIdx], context);
                    result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, branchVal));
                }
                continue;
            }

            // Generate conditional selection logic
            // For N conditions, we need N-1 conditional branches and a final else/default
            std::string phiEndLabel = "phi_end_" + std::to_string(result.size()); // Unique label

            for (size_t i = 0; i < phi->Conditions.size(); ++i) {
                RVMValue cond      = mapValue(phi->Conditions[i], context);
                RVMValue branchVal = mapValue(phi->Branches[i], context);

                // If condition is true, move branch value and jump to end
                // Branch if condition is false to next condition
                std::string nextLabel = "phi_next_" + std::to_string(result.size()) + "_" + std::to_string(i);

                // Branch if condition is zero (false) to next condition
                result.push_back(std::make_shared<RVMInstrBranch>(Opcode::BRZ, cond, nextLabel));

                // Condition is true: move branch value to destination
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, branchVal));
                result.push_back(std::make_shared<RVMInstrJump>(phiEndLabel));

                // Next condition label
                result.push_back(std::make_shared<RVMInstrLabel>(nextLabel));
            }

            // Handle else branch (if exists) or default (last branch)
            size_t elseIdx = phi->Conditions.size();
            if (elseIdx < phi->Branches.size()) {
                // There's an explicit else branch
                RVMValue elseVal = mapValue(phi->Branches[elseIdx], context);
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, elseVal));
            } else if (phi->Branches.size() > phi->Conditions.size()) {
                // Should not happen based on SSA spec, but handle gracefully
                RVMValue defaultVal = mapValue(phi->Branches.back(), context);
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, defaultVal));
            }

            // End of phi resolution
            result.push_back(std::make_shared<RVMInstrLabel>(phiEndLabel));
        }
    }

    return result;
}

// Map SSA program to RVM program
RVMProgram RVMMapper::mapProgram(const ssa::SSAProgram& ssaProgram)
{
    RVMProgram rvmProgram;

    // (1) Construct string table by loading all constants in the program
    auto handleStringConstant = [&](std::span<const std::shared_ptr<ssa::SSAInstr>> instructions) {
        for (const auto& instr : instructions) {
            instr->forEachValue([&](const ssa::SSAValue& val) {
                if (!val.isConstant() || val.type().kind() != type::TypeKind::String)
                    return;

                const std::string content = val.valueAs<std::string>();
                if (!mStringMap.contains(content)) {
                    auto target = RVMValue::StringRef(mStringMap.size());
                    rvmProgram.Body.push_back(std::make_shared<RVMInstrStringLiteral>(target, content));
                    mStringMap[content] = target;
                }
            });
        }
    };

    handleStringConstant(ssaProgram.Body);
    for (const auto& ssaFunc : ssaProgram.Functions) {
        if (!ssaFunc.External)
            handleStringConstant(ssaFunc.Body);
    }

    // (2) External functions - they become comments in the body
    bool hadExternal = false;
    for (const auto& ssaFunc : ssaProgram.Functions) {
        if (ssaFunc.External) {
            // Create a comment for external function declaration
            std::string comment = "[[extern";
            if (!ssaFunc.HasSideEffect)
                comment += ", pure";
            comment += "]] fn " + ssaFunc.Name + "(";

            for (size_t i = 0; i < ssaFunc.Parameters.size(); ++i) {
                if (i > 0)
                    comment += ", ";
                comment += ssaFunc.Parameters[i];
            }
            comment += ") : " + ssaFunc.ReturnType.toString();

            rvmProgram.Body.push_back(std::make_shared<RVMInstrComment>(comment));
            hadExternal = true;
        }
    }

    if (hadExternal) //< Add an empty line after external function declarations
        rvmProgram.Body.push_back(std::make_shared<RVMInstrComment>(""));

    // (3) Map main program body
    RVMContext mainContext;
    auto mainInstructions = mapInstructions(ssaProgram.Body, mainContext, ssaProgram);
    rvmProgram.Body.insert(rvmProgram.Body.end(), mainInstructions.begin(), mainInstructions.end());

    // (4) Map internal functions and embed them directly in the program body
    for (const auto& ssaFunc : ssaProgram.Functions) {
        if (!ssaFunc.External) {
            // Add comment with function signature at start of function block
            std::string signature = "fn " + ssaFunc.Name + "(";
            for (size_t i = 0; i < ssaFunc.Parameters.size(); ++i) {
                if (i > 0)
                    signature += ", ";
                signature += ssaFunc.Parameters[i];
            }
            signature += ") : " + ssaFunc.ReturnType.toString();
            rvmProgram.Body.push_back(std::make_shared<RVMInstrComment>(signature));

            // Add label for the function
            rvmProgram.Body.push_back(std::make_shared<RVMInstrLabel>(ssaFunc.Name));

            // Map function body instructions
            RVMContext funcContext;
            auto funcInstructions = mapInstructions(ssaFunc.Body, funcContext, ssaProgram);

            // Add function body to main body
            for (auto& instr : funcInstructions)
                rvmProgram.Body.push_back(instr);
        }
    }

    return rvmProgram;
}

} // namespace PExpr::rvm