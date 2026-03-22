#include "RVMMapper.h"
#include "RVMStructs.h"
#include "ast/Enums.h"

#include <functional>
#include <span>

namespace PExpr::rvm {
using namespace ast;

// Helper: Convert BinaryOperation to Opcode
static Opcode binaryOpToOpcode(BinaryOperation op)
{
    switch (op) {
    case BinaryOperation::Add:
        return Opcode::ADD;
    case BinaryOperation::Sub:
        return Opcode::SUB;
    case BinaryOperation::Mul:
        return Opcode::MUL;
    case BinaryOperation::Div:
        return Opcode::DIV;
    case BinaryOperation::Mod:
        return Opcode::MOD;
    case BinaryOperation::Equal:
        return Opcode::CMP_EQ;
    case BinaryOperation::NotEqual:
        return Opcode::CMP_NE;
    case BinaryOperation::Less:
        return Opcode::CMP_LT;
    case BinaryOperation::LessEqual:
        return Opcode::CMP_LE;
    case BinaryOperation::Greater:
        return Opcode::CMP_GT;
    case BinaryOperation::GreaterEqual:
        return Opcode::CMP_GE;
    case BinaryOperation::And:
        return Opcode::AND;
    case BinaryOperation::Or:
        return Opcode::OR;
    case BinaryOperation::Pow:
        return Opcode::POW;
    }
    PEXPR_ASSERT(false, "Invalid binary operation");
    return Opcode::NOP;
}

// Virtual registers start above the physical call-convention range (r0..r63)
static constexpr RegId kVirtualBase = 64;

// Constructor
RVMMapper::RVMMapper()
    : mNextVirtualRegister(kVirtualBase)
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

static void dissolveConstantTuple(const Tuple& tuple, std::vector<RVMValue>& constants)
{
    for (const auto& elem : tuple->elements) {
        if (const bool* valB = std::get_if<bool>(&elem)) {
            constants.push_back(RVMValue::Constant(*valB));
        } else if (const Integer* valI = std::get_if<Integer>(&elem)) {
            constants.push_back(RVMValue::Constant(*valI));
        } else if (const Number* valN = std::get_if<Number>(&elem)) {
            constants.push_back(RVMValue::Constant(*valN));
        } else if (const Tuple* nestedTuple = std::get_if<Tuple>(&elem)) {
            dissolveConstantTuple(*nestedTuple, constants);
        } else {
            PEXPR_ASSERT(false, "Invalid constant in tuple");
        }
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

// Helper to compute values for a tuple access (e.g., inner = outer[0])
// This handles cases where the target tuple's values are a subset of the source tuple's values
std::vector<RVMValue> RVMMapper::computeAccessValues(const ssa::SSAValue& tuple, Integer index)
{
    // Get the parent tuple values
    auto parentValues = mapTupleValues(tuple);

    // Compute the flat offset (sum of sizes before the access index)
    Integer flatOffset     = 0;
    const auto& components = tuple.type().components();
    for (Integer i = 0; i < index; ++i)
        flatOffset += getFlatSize(components.at(i));

    // Get the type of the sub-tuple at the access index
    const auto& subTupleType = components.at(index);
    auto subTupleTypes       = dissolveTupleType(subTupleType);

    // Extract the sub-tuple values starting from the flat offset
    std::vector<RVMValue> result;
    for (size_t i = 0; i < subTupleTypes.size(); ++i)
        result.push_back(parentValues.at(flatOffset + i));

    return result;
}

// Dissolve tuple value into elementary registers
std::vector<RVMValue> RVMMapper::mapTupleValues(const ssa::SSAValue& value)
{
    if (value.type().isTuple()) {
        if (auto it = mTupleMap.find(value); it != mTupleMap.end()) {
            return it->second;
        } else if (value.isConstant()) {
            // Dissolve constant tuple and cache the result
            std::vector<RVMValue> constants;
            const auto& tuple = value.valueAs<Tuple>();
            dissolveConstantTuple(tuple, constants);
            mTupleMap[value] = constants;
            return constants;
        } else {
            // TODO:
            const auto& types = dissolveTupleType(value.type());
            std::vector<RVMValue> result;
            result.reserve(types.size());
            for (const auto& type : types)
                result.push_back(RVMValue::Register(mNextVirtualRegister++, type));
            mTupleMap[value] = result;
            return result;
        }
    } else {
        // Elementary type
        return { mapValue(value) };
    }
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
    RegId reg         = mNextVirtualRegister++;
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
    } else if (value.type().isTuple()) {
        // Tuple not in mTupleMap yet - use mapTupleValues to handle it
        auto tupleValues = mapTupleValues(value);
        return tupleValues.at(idx);
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
        if (instr.Target.type().isTuple()) {
            // Tuple assignment: could be tuple copy or tuple construction
            if (instr.Operands.size() == 1) {
                // Tuple copy: use mapTupleValues to handle both mapped tuples and constant tuples
                mTupleMap[instr.Target] = mapTupleValues(instr.Operands[0]);
            } else {
                // Tuple construction from multiple operands
                std::vector<RVMValue> elements;

                for (size_t i = 0; i < instr.Operands.size(); ++i) {
                    // Use mapTupleValues to handle both mapped tuples and constant tuples
                    auto operandValues = mapTupleValues(instr.Operands[i]);
                    for (const auto& e : operandValues)
                        elements.push_back(e);
                }

                mTupleMap[instr.Target] = std::move(elements);
            }
        } else {
            // Simple scalar assignment: dst = src
            PEXPR_ASSERT(instr.Operands.size() == 1, "Scalar assign expects 1 operand");
            RVMValue dst = mapValue(instr.Target);
            RVMValue src = mapValue(instr.Operands[0]);

            result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        }
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Unary: {
        // Unary operation
        PEXPR_ASSERT(instr.Operands.size() == 1, "Unary expects 1 operand");

        if (instr.Target.type().isTuple()) {
            // Element-wise unary operation on tuples
            auto srcTuple = mapTupleValues(instr.Operands[0]);
            auto dstTypes = dissolveTupleType(instr.Target.type());

            PEXPR_ASSERT(srcTuple.size() == dstTypes.size(), "Tuple size mismatch in unary operation");

            std::vector<RVMValue> elements;
            for (size_t i = 0; i < srcTuple.size(); ++i) {
                RVMValue dst = RVMValue::Register(mNextVirtualRegister++, dstTypes[i]);
                RVMValue src = srcTuple[i];

                switch (instr.UnaryOp) {
                case UnaryOperation::Neg: {
                    RVMValue zero = dstTypes[i].kind() == type::TypeKind::Integer
                                        ? RVMValue::Constant(Integer(0))
                                        : RVMValue::Constant(Number(0.0));
                    result.push_back(std::make_shared<RVMInstr3Op>(Opcode::SUB, dst, zero, src));
                    break;
                }
                case UnaryOperation::Pos:
                    result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
                    break;
                case UnaryOperation::Not: {
                    RVMValue one = RVMValue::Constant(Integer(1));
                    result.push_back(std::make_shared<RVMInstr3Op>(Opcode::XOR, dst, src, one));
                    break;
                }
                }
                elements.push_back(dst);
            }
            mTupleMap[instr.Target] = std::move(elements);
        } else {
            // Scalar unary operation
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
        }
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Binary: {
        // Binary operation
        PEXPR_ASSERT(instr.Operands.size() == 2, "Binary expects 2 operands");

        Opcode op = binaryOpToOpcode(instr.BinaryOp);

        if (instr.Target.type().isTuple()) {
            auto dstTypes = dissolveTupleType(instr.Target.type());
            std::vector<RVMValue> elements;
            elements.reserve(dstTypes.size());

            if (instr.Operands[0].type().isTuple() && !instr.Operands[1].type().isTuple()) {
                // Scalar × Tuple operation (e.g., vec * 2.0)
                auto srcTuple  = mapTupleValues(instr.Operands[0]);
                auto scalarSrc = mapValue(instr.Operands[1]);
                PEXPR_ASSERT(srcTuple.size() == dstTypes.size(), "Tuple size mismatch in scalar x tuple operation");

                for (size_t i = 0; i < srcTuple.size(); ++i) {
                    RVMValue dst = RVMValue::Register(mNextVirtualRegister++, dstTypes[i]);
                    result.push_back(std::make_shared<RVMInstr3Op>(op, dst, srcTuple[i], scalarSrc));
                    elements.push_back(dst);
                }
            } else if (!instr.Operands[0].type().isTuple() && instr.Operands[1].type().isTuple()) {
                // Tuple × Scalar operation (e.g., 2.0 * vec)
                auto scalarSrc = mapValue(instr.Operands[0]);
                auto srcTuple  = mapTupleValues(instr.Operands[1]);
                PEXPR_ASSERT(srcTuple.size() == dstTypes.size(), "Tuple size mismatch in tuple×scalar operation");

                for (size_t i = 0; i < srcTuple.size(); ++i) {
                    RVMValue dst = RVMValue::Register(mNextVirtualRegister++, dstTypes[i]);
                    result.push_back(std::make_shared<RVMInstr3Op>(op, dst, scalarSrc, srcTuple[i]));
                    elements.push_back(dst);
                }
            } else {
                // Element-wise binary operation on tuples
                auto srcTuple1 = mapTupleValues(instr.Operands[0]);
                auto srcTuple2 = mapTupleValues(instr.Operands[1]);
                PEXPR_ASSERT(srcTuple1.size() == srcTuple2.size(), "Tuple size mismatch in binary operation");
                PEXPR_ASSERT(srcTuple1.size() == dstTypes.size(), "Tuple size mismatch with target type");

                for (size_t i = 0; i < srcTuple1.size(); ++i) {
                    RVMValue dst = RVMValue::Register(mNextVirtualRegister++, dstTypes[i]);
                    result.push_back(std::make_shared<RVMInstr3Op>(op, dst, srcTuple1[i], srcTuple2[i]));
                    elements.push_back(dst);
                }
            }
            mTupleMap[instr.Target] = std::move(elements);
        } else {
            // Scalar binary operation
            RVMValue dst  = mapValue(instr.Target);
            RVMValue src1 = mapValue(instr.Operands[0]);
            RVMValue src2 = mapValue(instr.Operands[1]);
            result.push_back(std::make_shared<RVMInstr3Op>(op, dst, src1, src2));
        }
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Access: {
        // Follow the chain of access until an elementary type is found
        PEXPR_ASSERT(instr.Operands.size() == 2, "Access expects 2 operands (tuple, index)");
        Integer index = instr.Operands[1].valueAs<Integer>();

        if (instr.Target.type().isTuple()) {
            // Compute and cache the tuple values for future accesses
            // This handles cases like inner = outer[0] where inner's values are a subset of outer's values
            mTupleMap[instr.Target] = computeAccessValues(instr.Operands[0], index);
        } else {
            RVMValue dst = mapValue(instr.Target);
            RVMValue src = accessTuple(instr.Operands[0], index);
            PEXPR_ASSERT(dst.type() == src.type(), "Type does not match after accessTuple!");

            // For RVM, tuple access should have been dissolved into direct register access
            result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        }
        break;
    }
    case ssa::SSAInstrAssign::OpKind::Cast: {
        // Type cast
        PEXPR_ASSERT(instr.Operands.size() == 1, "Cast expects 1 operand");

        if (instr.Target.type().isTuple()) {
            // Element-wise cast on tuples
            auto srcTuple = mapTupleValues(instr.Operands[0]);
            auto dstTypes = dissolveTupleType(instr.Target.type());

            PEXPR_ASSERT(srcTuple.size() == dstTypes.size(), "Tuple size mismatch in cast operation");

            std::vector<RVMValue> elements;
            for (size_t i = 0; i < srcTuple.size(); ++i) {
                RVMValue dst = RVMValue::Register(mNextVirtualRegister++, dstTypes[i]);
                RVMValue src = srcTuple[i];

                const auto& srcType = srcTuple[i].type();
                const auto& dstType = dstTypes[i];

                // Determine conversion opcode
                Opcode castOp = Opcode::MOV; // default to move if same type

                if (srcType.kind() == type::TypeKind::Integer && dstType.kind() == type::TypeKind::Number)
                    castOp = Opcode::I2F;
                else if (srcType.kind() == type::TypeKind::Number && dstType.kind() == type::TypeKind::Integer)
                    castOp = Opcode::F2I;

                result.push_back(std::make_shared<RVMInstr2Op>(castOp, dst, src));
                elements.push_back(dst);
            }
            mTupleMap[instr.Target] = std::move(elements);
        } else {
            // Scalar cast
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

            result.push_back(std::make_shared<RVMInstr2Op>(castOp, dst, src));
        }
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

    // 1. Flatten all call arguments to a list of RVM values
    std::vector<RVMValue> arguments;
    for (const auto& arg : instr.Arguments) {
        const auto vals = mapTupleValues(arg);
        arguments.insert(arguments.end(), vals.begin(), vals.end());
    }

    // 2. Determine return register count
    auto returnTypes       = dissolveTupleType(instr.Target.type());
    uint32_t numReturnRegs = (!instr.Target.type().isVoid()) ? static_cast<uint32_t>(returnTypes.size()) : 0;
    uint32_t numArgRegs    = static_cast<uint32_t>(arguments.size());

    // 3. Move arguments to physical call registers r0, r1, ...
    // No save/restore needed: the interference graph ensures virtual registers live
    // across a call are not assigned to physical call-convention register IDs.
    for (size_t i = 0; i < arguments.size(); ++i) {
        RVMValue dst = RVMValue::Register(i, arguments[i].type());
        result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, arguments[i]));
    }

    // 4. Emit call instruction
    result.push_back(std::make_shared<RVMInstrCall>(isExternal, numArgRegs, numReturnRegs, instr.FunctionName));

    // 5. Move return values from physical registers to fresh virtual registers
    if (!instr.Target.type().isVoid()) {
        if (instr.Target.type().isTuple()) {
            std::vector<RVMValue> values;
            for (size_t i = 0; i < returnTypes.size(); ++i) {
                RVMValue src = RVMValue::Register(i, returnTypes[i]);
                RVMValue dst = RVMValue::Register(mNextVirtualRegister++, returnTypes[i]);
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
                values.push_back(dst);
            }
            mTupleMap[instr.Target] = std::move(values);
        } else {
            RVMValue src = RVMValue::Register(0, instr.Target.type());
            RVMValue dst = mapValue(instr.Target); // allocates a fresh virtual register
            result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
        }
    }

    return result;
}

// Map SSA return instruction
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapReturn(const ssa::SSAInstrReturn& instr)
{
    std::vector<std::shared_ptr<RVMInstr>> result;
    size_t returnCount = 0;

    if (!instr.Value.type().isVoid()) {
        if (instr.Value.type().isTuple()) {
            // Tuple return value: use mapTupleValues which handles both cached and constant tuples
            const auto tupleValues = mapTupleValues(instr.Value);
            returnCount            = tupleValues.size();

            for (size_t i = 0; i < tupleValues.size(); ++i) {
                RVMValue src = tupleValues[i];
                RVMValue dst = RVMValue::Register(i, src.type()); // %r0, %r1, %r2, ...
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
            }
        } else {
            // Single return value: move to %r0
            returnCount  = 1;
            RVMValue src = mapValue(instr.Value);
            if (!src.isRegister() || src.regId() != 0) {                  //< Only move if source is not already %r0
                RVMValue dst = RVMValue::Register(0, instr.Value.type()); // %r0
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src));
            }
        }
    }

    // Add the actual return instruction with the correct return count
    result.push_back(std::make_shared<RVMInstrReturn>(returnCount));

    return result;
}

// Map SSA branch instruction
std::shared_ptr<RVMInstr> RVMMapper::mapBranch(const ssa::SSAInstrBranch& instr)
{
    RVMValue cond = mapValue(instr.Condition);

    // Branch if not zero (condition is true)
    return std::make_shared<RVMInstrBranch>(Opcode::JNZ, cond, instr.TargetLabel);
}

// Map SSA goto instruction
std::shared_ptr<RVMInstr> RVMMapper::mapGoto(const ssa::SSAInstrGoto& instr)
{
    return std::make_shared<RVMInstrJump>(instr.TargetLabel);
}

// Emit a conditional select sequence for a phi node
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapPhi(const ssa::SSAInstrPhi& phi)
{
    std::vector<std::shared_ptr<RVMInstr>> result;

    const size_t numConditions = phi.Conditions.size();
    const uint32_t labelBase   = mNextPhiLabelId++;
    const std::string endLabel = "phi_end_" + std::to_string(labelBase);
    const bool isTuple         = phi.Target.type().isTuple();

    // Helper to emit MOV(s) from a branch value to the phi target
    auto emitMov = [&](const ssa::SSAValue& branchValue) {
        if (isTuple) {
            auto targetVals = mapTupleValues(phi.Target);
            auto branchVals = mapTupleValues(branchValue);
            PEXPR_ASSERT(targetVals.size() == branchVals.size(),
                         "Tuple phi target and branch value size mismatch");
            for (size_t j = 0; j < targetVals.size(); ++j)
                result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, targetVals[j], branchVals[j]));
        } else {
            result.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, mapValue(phi.Target), mapValue(branchValue)));
        }
    };

    // Start with else value (last branch)
    emitMov(phi.Branches.back());

    // Emit conditional overwrites for each condition
    for (size_t i = 0; i < numConditions; ++i) {
        RVMValue cond = mapValue(phi.Conditions[i]);

        // Determine the label to jump to when this condition is false
        const std::string nextLabel = (i + 1 < numConditions)
            ? "phi_next_" + std::to_string(labelBase) + "_" + std::to_string(i)
            : endLabel;

        // JZ nextLabel, cond — skip this branch's value if condition is false
        result.push_back(std::make_shared<RVMInstrBranch>(Opcode::JZ, cond, nextLabel));

        // MOV target = branch[i] (the value for when condition[i] is true)
        emitMov(phi.Branches[i]);

        // JMP endLabel — skip remaining conditions
        if (i + 1 < numConditions)
            result.push_back(std::make_shared<RVMInstrJump>(endLabel));

        // Emit intermediate label for the next condition
        if (i + 1 < numConditions)
            result.push_back(std::make_shared<RVMInstrLabel>(nextLabel));
    }

    // End label
    result.push_back(std::make_shared<RVMInstrLabel>(endLabel));

    return result;
}

// Map SSA instructions to RVM instructions
std::vector<std::shared_ptr<RVMInstr>> RVMMapper::mapInstructions(const std::vector<std::shared_ptr<ssa::SSAInstr>>& ssaInstrs, const ssa::SSAProgram& ssaProgram)
{
    std::vector<std::shared_ptr<RVMInstr>> result;

    for (const auto& ssaInstr : ssaInstrs) {
        // Handle label instructions
        if (auto* label = dynamic_cast<ssa::SSAInstrLabel*>(ssaInstr.get())) {
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

        // Handle phi instructions — emit a self-contained conditional select
        if (auto* phi = dynamic_cast<ssa::SSAInstrPhi*>(ssaInstr.get())) {
            auto instrs = mapPhi(*phi);
            result.insert(result.end(), instrs.begin(), instrs.end());
            continue;
        }
    }

    // If no instruction was emitted, or the last instruction is NOT a return (e.g., void).
    // Inject a zero return.
    if (result.empty() || dynamic_cast<RVMInstrReturn*>(result.back().get()) == nullptr)
        result.push_back(std::make_shared<RVMInstrReturn>(0));

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
                comment += ssaFunc.Parameters[i].name() + ":" + ssaFunc.Parameters[i].type().toString();
            }
            comment += ") : " + ssaFunc.ReturnType.toString();

            rvmProgram.push_back(std::make_shared<RVMInstrComment>(comment));
        }
    }

    // (3) Map main program body
    rvmProgram.push_back(std::make_shared<RVMInstrComment>("Main Body:"));
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
                signature += ssaFunc.Parameters[i].name() + ":" + ssaFunc.Parameters[i].type().toString();
            }
            signature += ") : " + ssaFunc.ReturnType.toString();
            rvmProgram.push_back(std::make_shared<RVMInstrComment>(signature));

            // Add label for the function
            rvmProgram.push_back(std::make_shared<RVMInstrLabel>(ssaFunc.Name));

            // Handle parameters
            size_t paramIndex = 0;
            for (const auto& param : ssaFunc.Parameters) {
                if (param.type().isTuple()) {
                    // Multiple return values. Essentially a virtual tuple instruction
                    const auto innerTypes = dissolveTupleType(param.type());
                    std::vector<RVMValue> values;
                    for (size_t i = 0; i < innerTypes.size(); ++i) {
                        size_t srcRegId = paramIndex++;
                        RVMValue src    = RVMValue::Register(srcRegId, innerTypes[i]); // %rI holds return value
                        values.push_back(src);
                    }
                    mTupleMap[param] = std::move(values);
                } else {
                    size_t srcRegId = paramIndex++;
                    RVMValue src    = RVMValue::Register(srcRegId, param.type()); // %rI holds return value

                    // Store the mapping for future reference
                    mSSAtoRVMMap[param.name()] = src;
                }
            }
            mNextVirtualRegister = kVirtualBase;
            mNextPhiLabelId     = 0;

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