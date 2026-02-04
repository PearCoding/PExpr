#include "RVMMapper.h"
#include "RVMStructs.h"
#include "ast/Enums.h"

#include <unordered_map>

namespace PExpr::rvm {
using namespace ast;

// Helper structure to track SSA value to RVM register(s) mapping
// Since tuples need to be dissolved, one SSA value may map to multiple registers
struct ValueMapping {
    std::vector<RVMValue> values; // For tuples, this contains multiple elementary values
};

// Context for mapping SSA to RVM
class MapperContext {
public:
    std::shared_ptr<RVMStringTable> stringTable;
    RVMContext& rvmContext;
    std::unordered_map<std::string, ValueMapping> ssaToRvmMap;

    explicit MapperContext(RVMContext& ctx, std::shared_ptr<RVMStringTable> strTable)
        : stringTable(std::move(strTable))
        , rvmContext(ctx)
    {
    }

    void mapValue(const std::string& ssaName, const ValueMapping& mapping)
    {
        ssaToRvmMap[ssaName] = mapping;
    }

    ValueMapping getValue(const std::string& ssaName)
    {
        auto it = ssaToRvmMap.find(ssaName);
        if (it != ssaToRvmMap.end())
            return it->second;
        // Return empty mapping if not found
        return ValueMapping{};
    }
};

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

// Map SSA value to RVM value(s), dissolving tuples
RVMValue RVMMapper::mapValue(const ssa::SSAValue& ssaValue,
                             std::shared_ptr<RVMStringTable> stringTable,
                             RVMContext& context)
{
    MapperContext mapCtx(context, stringTable);

    if (ssaValue.isConstant()) {
        const auto& rawValue = ssaValue.rawValue();
        const auto& type     = ssaValue.type();

        // Handle string constants via string table
        if (auto* strPtr = std::get_if<std::string>(&rawValue)) {
            uint32_t strId = stringTable->addString(*strPtr);
            return RVMValue::StringRef(strId, type);
        }
        // Handle tuple constants (need to dissolve)
        else if (std::get_if<Tuple>(&rawValue)) {
            PEXPR_ASSERT(false, "Tuple constants should be dissolved before mapping to RVM");
            return RVMValue::Constant(false); // unreachable
        }
        // Handle elementary constants
        else if (auto* bVal = std::get_if<bool>(&rawValue)) {
            return RVMValue::Constant(*bVal);
        } else if (auto* iVal = std::get_if<Integer>(&rawValue)) {
            return RVMValue::Constant(*iVal);
        } else if (auto* nVal = std::get_if<Number>(&rawValue)) {
            return RVMValue::Constant(*nVal);
        }
    }

    // Register/named value - for now create a simple register mapping
    // In practice, the full mapper context should track SSA name -> RVM register(s)
    RegId reg = context.allocateRegister(ssaValue.type());
    return RVMValue::Register(reg, ssaValue.type());
}

// Map SSA assign instruction
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapAssign(
    const ssa::SSAInstrAssign& instr,
    std::shared_ptr<RVMStringTable> stringTable,
    RVMContext& context)
{
    std::vector<std::shared_ptr<RVMInstr>> result;
    MapperContext mapCtx(context, stringTable);

    switch (instr.Operator) {
    case ssa::SSAInstrAssign::OpKind::Assign: {
        // Simple assignment: dst = src
        PEXPR_ASSERT(instr.Operands.size() == 1, "Assign expects 1 operand");

        RVMValue dst = mapValue(instr.Target, stringTable, context);
        RVMValue src = mapValue(instr.Operands[0], stringTable, context);

        result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Unary: {
        // Unary operation
        PEXPR_ASSERT(instr.Operands.size() == 1, "Unary expects 1 operand");

        RVMValue dst = mapValue(instr.Target, stringTable, context);
        RVMValue src = mapValue(instr.Operands[0], stringTable, context);

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

        RVMValue dst  = mapValue(instr.Target, stringTable, context);
        RVMValue src1 = mapValue(instr.Operands[0], stringTable, context);
        RVMValue src2 = mapValue(instr.Operands[1], stringTable, context);

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

        RVMValue dst = mapValue(instr.Target, stringTable, context);
        RVMValue src = mapValue(instr.Operands[0], stringTable, context);

        // For RVM, tuple access should have been dissolved into direct register access
        result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Tuple: {
        // Tuple construction should be dissolved
        // Each tuple element maps to a separate register
        PEXPR_ASSERT(false, "Tuple construction should be dissolved before RVM mapping");
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Cast: {
        // Type cast
        PEXPR_ASSERT(instr.Operands.size() == 1, "Cast expects 1 operand");

        RVMValue dst = mapValue(instr.Target, stringTable, context);
        RVMValue src = mapValue(instr.Operands[0], stringTable, context);

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
std::shared_ptr<RVMInstr> RVMMapper::mapCall(
    const ssa::SSAInstrCall& instr,
    std::shared_ptr<RVMStringTable> stringTable,
    RVMContext& context)
{
    std::vector<RVMValue> args;
    args.reserve(instr.Arguments.size());

    for (const auto& arg : instr.Arguments) {
        args.push_back(mapValue(arg, stringTable, context));
    }

    std::optional<RVMValue> dst;
    if (!instr.Target.type().isVoid()) {
        dst = mapValue(instr.Target, stringTable, context);
    }

    return std::make_shared<RVMInstrCall>(dst, instr.FunctionName, args);
}

// Map SSA return instruction
std::shared_ptr<RVMInstr> RVMMapper::mapReturn(
    const ssa::SSAInstrReturn& instr,
    std::shared_ptr<RVMStringTable> stringTable,
    RVMContext& context)
{
    std::optional<RVMValue> retVal;

    if (!instr.Value.type().isVoid()) {
        retVal = mapValue(instr.Value, stringTable, context);
    }

    return std::make_shared<RVMInstrReturn>(retVal);
}

// Map SSA branch instruction
std::shared_ptr<RVMInstr> RVMMapper::mapBranch(
    const ssa::SSAInstrBranch& instr,
    std::shared_ptr<RVMStringTable> stringTable,
    RVMContext& context)
{
    RVMValue cond = mapValue(instr.Condition, stringTable, context);

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
    std::shared_ptr<RVMStringTable> stringTable,
    RVMContext& context)
{
    std::vector<std::shared_ptr<RVMInstr>> result;
    MapperContext mapCtx(context, stringTable);

    for (const auto& ssaInstr : ssaInstrs) {
        // Handle label instructions
        if (auto* label = dynamic_cast<ssa::SSAInstrLabel*>(ssaInstr.get())) {
            // Emit label instruction for jump targets
            result.push_back(std::make_shared<RVMInstrLabel>(label->Name));
            continue;
        }

        // Handle assign instructions
        if (auto* assign = dynamic_cast<ssa::SSAInstrAssign*>(ssaInstr.get())) {
            auto instrs = mapAssign(*assign, stringTable, context);
            result.insert(result.end(), instrs.begin(), instrs.end());
            continue;
        }

        // Handle call instructions
        if (auto* call = dynamic_cast<ssa::SSAInstrCall*>(ssaInstr.get())) {
            result.push_back(mapCall(*call, stringTable, context));
            continue;
        }

        // Handle return instructions
        if (auto* ret = dynamic_cast<ssa::SSAInstrReturn*>(ssaInstr.get())) {
            result.push_back(mapReturn(*ret, stringTable, context));
            continue;
        }

        // Handle branch instructions
        if (auto* branch = dynamic_cast<ssa::SSAInstrBranch*>(ssaInstr.get())) {
            result.push_back(mapBranch(*branch, stringTable, context));
            continue;
        }

        // Handle goto instructions
        if (auto* gotoInstr = dynamic_cast<ssa::SSAInstrGoto*>(ssaInstr.get())) {
            result.push_back(mapGoto(*gotoInstr));
            continue;
        }

        // Handle phi instructions
        if (auto* phi = dynamic_cast<ssa::SSAInstrPhi*>(ssaInstr.get())) {
            // Phi nodes need special handling in RVM
            // For now, we convert them to a series of conditional moves
            // This is simplified - proper handling would require more sophisticated analysis
            // TODO
            RVMValue dst = mapValue(phi->Target, stringTable, context);

            // Convert phi to conditional moves based on conditions
            for (size_t i = 0; i < phi->Conditions.size(); ++i) {
                RVMValue cond   = mapValue(phi->Conditions[i], stringTable, context);
                RVMValue branch = mapValue(phi->Branches[i], stringTable, context);

                // If condition is true, move branch value to dst
                // This is a simplified representation - actual implementation may vary
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, branch));
            }

            // Handle else branch
            if (phi->Branches.size() > phi->Conditions.size()) {
                RVMValue elseBranch = mapValue(phi->Branches.back(), stringTable, context);
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, elseBranch));
            }
            continue;
        }
    }

    return result;
}

// Map SSA function to RVM function
RVMFunction RVMMapper::mapFunction(const ssa::SSAFunction& ssaFunc,
                                   std::shared_ptr<RVMStringTable> stringTable)
{
    RVMFunction rvmFunc;
    rvmFunc.name          = ssaFunc.Name;
    rvmFunc.returnType    = ssaFunc.ReturnType;
    rvmFunc.external      = ssaFunc.External;
    rvmFunc.hasSideEffect = ssaFunc.HasSideEffect;

    // Dissolve tuple parameters into elementary types
    for (size_t i = 0; i < ssaFunc.Parameters.size(); ++i) {
        // For now, assume parameters are elementary types
        // In full implementation, we'd need to access parameter types
        // and dissolve tuples into multiple parameters
        rvmFunc.parameters.push_back(type::Type(type::TypeKind::Unspecified));
    }

    // Map function body
    if (!ssaFunc.External) {
        RVMContext funcContext;
        rvmFunc.body = mapInstructions(ssaFunc.Body, stringTable, funcContext);
    }

    return rvmFunc;
}

// Map SSA program to RVM program
RVMProgram RVMMapper::mapProgram(const ssa::SSAProgram& ssaProgram)
{
    RVMProgram rvmProgram;
    rvmProgram.stringTable = std::make_shared<RVMStringTable>();

    // Map main program body
    RVMContext mainContext;
    rvmProgram.body = mapInstructions(ssaProgram.Body, rvmProgram.stringTable, mainContext);

    // Map all functions
    rvmProgram.functions.reserve(ssaProgram.Functions.size());
    for (const auto& ssaFunc : ssaProgram.Functions) {
        rvmProgram.functions.push_back(mapFunction(ssaFunc, rvmProgram.stringTable));
    }

    return rvmProgram;
}

// Dissolve tuple instruction (placeholder for now)
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::dissolveTupleInstruction(
    const std::shared_ptr<ssa::SSAInstr>& ssaInstr,
    std::shared_ptr<RVMStringTable> stringTable,
    RVMContext& context)
{
    // This would handle dissolving tuple operations into elementary operations
    // For now, delegate to standard instruction mapping
    std::vector<std::shared_ptr<ssa::SSAInstr>> instrs = { ssaInstr };
    return mapInstructions(instrs, stringTable, context);
}

} // namespace PExpr::rvm
