#include "RVMMapper.h"
#include "RVMStructs.h"
#include "ast/Enums.h"

#include <span>

namespace PExpr::rvm {
using namespace ast;

// Constructor
RVMMapper::RVMMapper()
    : mContext()
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

// Dissolve tuple value into elementary registers
std::vector<RVMValue> RVMMapper::mapTupleValues(const ssa::SSAValue& value)
{
    if (value.type().isTuple()) {
        PEXPR_ASSERT(!value.isConstant(), "Can't deal with constant tuple values!");
        if (auto it = mTupleMap.find(value); it != mTupleMap.end())
            return it->second;
        else
            PEXPR_ASSERT(false, "Invalid SSA with incomplete tuple graph!");
        return {};
    } else {
        // Elementary type
        return { mapValue(value) };
    }
}

static Integer getFlatSize(const type::Type& type)
{
    if (!type.isTuple())
        return 1;

    Integer sum = 0;
    for (const auto& tuple : type.components())
        sum += getFlatSize(tuple);
    return sum;
}

// Map SSA value to RVM value
RVMValue RVMMapper::mapValue(const ssa::SSAValue& ssaValue)
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
    RegId reg         = mContext.allocateRegister(ssaValue.type());
    RVMValue rvmValue = RVMValue::Register(reg, ssaValue.type());

    // Store the mapping for future reference
    if (!ssaValue.isConstant())
        mSSAtoRVMMap[ssaValue.name()] = rvmValue;

    return rvmValue;
}

RVMValue RVMMapper::accessTuple(const ssa::SSAValue& value, Integer idx)
{
    if (auto tIt = mTupleMap.find(value); tIt != mTupleMap.end()) {
        return tIt->second.at(idx);
    } else if (auto aIt = mAccessMap.find(value); aIt != mAccessMap.end()) {
        Integer linearOffset = 0;
        for (Integer i = 0; i < aIt->second.second; ++i)
            linearOffset += getFlatSize(aIt->second.first.type().components().at(i));
        return accessTuple(aIt->second.first, linearOffset + idx);
    } else {
        return mapValue(value);
    }
}

// Map SSA assign instruction
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapAssign(const ssa::SSAInstrAssign& instr)
{
    std::vector<std::shared_ptr<RVMInstr>> result;

    switch (instr.Operator) {
    case ssa::SSAInstrAssign::OpKind::Assign: {
        // Simple assignment: dst = src
        PEXPR_ASSERT(instr.Operands.size() == 1, "Assign expects 1 operand");

        if (instr.Target.type().isTuple()) {
            // Forward the association
            mTupleMap[instr.Target] = mTupleMap.at(instr.Operands[0]);
        } else {
            RVMValue dst = mapValue(instr.Target);
            RVMValue src = mapValue(instr.Operands[0]);

            result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        }
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Unary: {
        // Unary operation
        PEXPR_ASSERT(instr.Operands.size() == 1, "Unary expects 1 operand");

        RVMValue dst = mapValue(instr.Target);
        RVMValue src = mapValue(instr.Operands[0]);

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

        RVMValue dst  = mapValue(instr.Target);
        RVMValue src1 = mapValue(instr.Operands[0]);
        RVMValue src2 = mapValue(instr.Operands[1]);

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
        // Follow the chain of access until an elementary type is found
        PEXPR_ASSERT(instr.Operands.size() == 2, "Access expects 2 operands (tuple, index)");
        Integer index = instr.Operands[1].valueAs<Integer>();

        if (instr.Target.type().isTuple()) {
            // Remember for the following access'es
            mAccessMap[instr.Target] = { instr.Operands[0], index };
        } else {
            RVMValue dst = mapValue(instr.Target);
            RVMValue src = accessTuple(instr.Operands[0], index);
            PEXPR_ASSERT(dst.type() == src.type(), "Type does not match after accessTuple!");

            // For RVM, tuple access should have been dissolved into direct register access
            result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        }
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Tuple: {
        std::vector<RVMValue> elements;

        for (size_t i = 0; i < instr.Operands.size(); ++i) {
            if (auto it = mTupleMap.find(instr.Operands[i]); it != mTupleMap.end()) {
                for (const auto& e : it->second)
                    elements.push_back(e);
            } else {
                const auto dstType = instr.Operands[i].type();
                PEXPR_ASSERT(!dstType.isTuple(), "Undetected tuple found during tuple dissolving");

                RVMValue src = mapValue(instr.Operands[i]);
                RVMValue dst = RVMValue::Register(mContext.allocateRegister(dstType), dstType);

                elements.push_back(dst);
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
            }
        }

        mTupleMap[instr.Target] = std::move(elements);
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Cast: {
        // Type cast
        PEXPR_ASSERT(instr.Operands.size() == 1, "Cast expects 1 operand");

        RVMValue dst = mapValue(instr.Target);
        RVMValue src = mapValue(instr.Operands[0]);

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
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapCall(const ssa::SSAInstrCall& instr, const ssa::SSAProgram& ssaProgram)
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
            args.push_back(mapValue(arg));

        // TODO: Tuple returns!?
        std::optional<RVMValue> dst;
        if (!instr.Target.type().isVoid())
            dst = mapValue(instr.Target);

        result.push_back(std::make_shared<RVMInstrExternalCall>(dst, instr.FunctionName, args));
    } else {
        // Internal call: use calling convention with registers
        // 1. Get flat call arguments
        std::vector<RVMValue> arguments;
        for (const auto& arg : instr.Arguments) {
            const auto types = mapTupleValues(arg);
            arguments.insert(arguments.end(), types.begin(), types.end());
        }

        // 2. Determine number of registers to save (max of args and return values)
        auto returnTypes       = dissolveTupleType(instr.Target.type());
        uint32_t numReturnRegs = returnTypes.empty() || instr.Target.type().isVoid() ? 0 : static_cast<uint32_t>(returnTypes.size());
        uint32_t numArgRegs    = static_cast<uint32_t>(arguments.size());
        uint32_t registerCount = std::max(numReturnRegs, numArgRegs);

        // 3. Push frame to save register context
        if (registerCount > 0)
            result.push_back(std::make_shared<RVMInstrPushFrame>(registerCount));

        // 4. Move arguments to registers %r0, %r1, %r2, etc.
        for (size_t i = 0; i < arguments.size(); ++i) {
            RVMValue src = arguments[i];
            if (src.isRegister() && src.regId() == i) //< Already in the correct register, no move needed
                continue;

            RVMValue dst = RVMValue::Register(i, arguments[i].type()); // %r0, %r1, %r2, ...
            result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        }

        // 5. Call internal function
        result.push_back(std::make_shared<RVMInstrInternalCall>(instr.FunctionName));

        // 6. Move return values from %r0, %r1, %r2, ... to destinations (if not void)
        if (!instr.Target.type().isVoid()) {
            if (instr.Target.type().isTuple()) {
                // Multiple return values. Essentially a virtual tuple instruction
                std::vector<RVMValue> values;
                for (size_t i = 0; i < returnTypes.size(); ++i) {
                    RVMValue dst = RVMValue::Register(mContext.allocateRegister(returnTypes[i]), returnTypes[i]);
                    if (!dst.isRegister() || dst.regId() != i) {              //< Only move if destination is not already %r0
                        RVMValue src = RVMValue::Register(i, returnTypes[i]); // %r0 holds return value
                        result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
                    }
                    values.push_back(dst); // %r0, %r1, %r2, ...
                }
                mTupleMap[instr.Target] = std::move(values);
            } else {
                // Single return value
                RVMValue dst = mapValue(instr.Target);
                if (!dst.isRegister() || dst.regId() != 0) {                   //< Only move if destination is not already %r0
                    RVMValue src = RVMValue::Register(0, instr.Target.type()); // %r0 holds return value
                    result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
                }
            }
        }

        // 7. Pop frame to restore register context
        if (registerCount > 0)
            result.push_back(std::make_shared<RVMInstrPopFrame>(registerCount));
    }

    return result;
}

// Map SSA return instruction
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapReturn(const ssa::SSAInstrReturn& instr)
{
    std::vector<std::shared_ptr<RVMInstr>> result;

    if (!instr.Value.type().isVoid()) {
        if (auto it = mTupleMap.find(instr.Value); it != mTupleMap.end()) {
            // Dissolved tuple, flat it out
            for (size_t i = 0; i < it->second.size(); ++i) {
                RVMValue src = it->second[i];
                RVMValue dst = RVMValue::Register(i, src.type()); // %r0, %r1, %r2, ...
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
            }
        } else {
            // Single return value: move to %r0
            RVMValue src = mapValue(instr.Value);
            if (!src.isRegister() || src.regId() != 0) {                  //< Only move if source is not already %r0
                RVMValue dst = RVMValue::Register(0, instr.Value.type()); // %r0
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
            }
        }
    }

    // Add the actual return instruction
    result.push_back(std::make_shared<RVMInstrReturn>());

    return result;
}

// Map SSA branch instruction
std::shared_ptr<RVMInstr> RVMMapper::mapBranch(const ssa::SSAInstrBranch& instr)
{
    RVMValue cond = mapValue(instr.Condition);

    // Branch if not zero (condition is true)
    return std::make_shared<RVMInstrBranch>(Opcode::BRNZ, cond, instr.TargetLabel);
}

// Map SSA goto instruction
std::shared_ptr<RVMInstr> RVMMapper::mapGoto(const ssa::SSAInstrGoto& instr)
{
    return std::make_shared<RVMInstrJump>(instr.TargetLabel);
}

// Map SSA instructions to RVM instructions
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapInstructions(const std::vector<std::shared_ptr<ssa::SSAInstr>>& ssaInstrs, const ssa::SSAProgram& ssaProgram)
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
            auto instrs = mapAssign(*assign);
            result.insert(result.end(), instrs.begin(), instrs.end());
            continue;
        }

        // Handle call instructions
        if (auto* call = dynamic_cast<ssa::SSAInstrCall*>(ssaInstr.get())) {
            auto instrs = mapCall(*call, ssaProgram);
            result.insert(result.end(), instrs.begin(), instrs.end());
            continue;
        }

        // Handle return instructions
        if (auto* ret = dynamic_cast<ssa::SSAInstrReturn*>(ssaInstr.get())) {
            auto instrs = mapReturn(*ret);
            result.insert(result.end(), instrs.begin(), instrs.end());
            continue;
        }

        // Handle branch instructions
        if (auto* branch = dynamic_cast<ssa::SSAInstrBranch*>(ssaInstr.get())) {
            result.push_back(mapBranch(*branch));
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
            RVMValue dst = mapValue(phi->Target);

            if (phi->Conditions.empty()) {
                // No conditions, just use the else branch (or first branch if no else)
                size_t branchIdx = phi->Branches.size() > 0 ? 0 : 0;
                if (branchIdx < phi->Branches.size()) {
                    RVMValue branchVal = mapValue(phi->Branches[branchIdx]);
                    result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, branchVal));
                }
                continue;
            }

            // Generate conditional selection logic
            // For N conditions, we need N-1 conditional branches and a final else/default
            std::string phiEndLabel = "phi_end_" + std::to_string(result.size()); // Unique label

            for (size_t i = 0; i < phi->Conditions.size(); ++i) {
                RVMValue cond      = mapValue(phi->Conditions[i]);
                RVMValue branchVal = mapValue(phi->Branches[i]);

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
                RVMValue elseVal = mapValue(phi->Branches[elseIdx]);
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, elseVal));
            } else if (phi->Branches.size() > phi->Conditions.size()) {
                // Should not happen based on SSA spec, but handle gracefully
                RVMValue defaultVal = mapValue(phi->Branches.back());
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
                    rvmProgram.push_back(std::make_shared<RVMInstrStringLiteral>(target, content));
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

            rvmProgram.push_back(std::make_shared<RVMInstrComment>(comment));
            hadExternal = true;
        }
    }

    if (hadExternal) //< Add an empty line after external function declarations
        rvmProgram.push_back(std::make_shared<RVMInstrComment>(""));

    // (3) Map main program body
    auto mainInstructions = mapInstructions(ssaProgram.Body, ssaProgram);
    rvmProgram.insert(rvmProgram.end(), mainInstructions.begin(), mainInstructions.end());

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
            rvmProgram.push_back(std::make_shared<RVMInstrComment>(signature));

            // Add label for the function
            rvmProgram.push_back(std::make_shared<RVMInstrLabel>(ssaFunc.Name));

            // Map function body instructions
            auto funcInstructions = mapInstructions(ssaFunc.Body, ssaProgram);

            // Add function body to main body
            for (auto& instr : funcInstructions)
                rvmProgram.push_back(instr);
        }
    }

    return rvmProgram;
}

} // namespace PExpr::rvm