#include "RVMInterpreter.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>

namespace PExpr::rvm {

void RVMInterpreter::registerExternalFunction(const std::string& mangledName,
                                              std::function<ValueVariant(const std::vector<ValueVariant>&)> func)
{
    externalFunctions[mangledName] = std::move(func);
}

ValueVariant RVMInterpreter::execute(const RVMProgram& program, const type::Type& returnType)
{
    registers.clear();
    stringTable.clear();
    callStack.clear();

    // Map from label name to instruction index
    std::unordered_map<std::string, size_t> labelMap;
    for (size_t i = 0; i < program.size(); ++i) {
        if (auto* label = dynamic_cast<RVMInstrLabel*>(program[i].get()))
            labelMap[label->labelName()] = i;
    }

    size_t pc = 0;
    while (pc < program.size()) {
        auto& instr = program[pc];

        if (dynamic_cast<RVMInstrLabel*>(instr.get()) || dynamic_cast<RVMInstrComment*>(instr.get())) {
            pc++;
            continue;
        }

        if (auto ret = dynamic_cast<RVMInstrReturn*>(instr.get())) {
            if (callStack.empty())
                break;
            pc = popCallFrame(ret->returnCount());
            pc++;
            continue;
        }

        if (auto* branch = dynamic_cast<RVMInstrBranch*>(instr.get())) {
            auto cond         = evaluateValue(branch->condition());
            bool shouldBranch = false;

            if (branch->opcode() == Opcode::JZ)
                shouldBranch = isZero(cond);
            else if (branch->opcode() == Opcode::JNZ)
                shouldBranch = !isZero(cond);

            if (shouldBranch) {
                auto it = labelMap.find(branch->targetLabel());
                if (it != labelMap.end()) {
                    pc = it->second;
                    continue;
                }
            }
            pc++;
            continue;
        }

        if (auto* jump = dynamic_cast<RVMInstrJump*>(instr.get())) {
            auto it = labelMap.find(jump->targetLabel());
            if (it != labelMap.end()) {
                pc = it->second;
                continue;
            }
            pc++;
            continue;
        }

        if (auto* call = dynamic_cast<RVMInstrCall*>(instr.get())) {
            if (call->isExternal()) {
                auto it = externalFunctions.find(call->functionName());
                if (it != externalFunctions.end()) {
                    std::vector<ValueVariant> args;
                    args.reserve(call->parameterCount());
                    for (size_t i = 0; i < call->parameterCount(); ++i)
                        args.push_back(registers[i].value);
                    ValueVariant result = it->second(args);
                    // RVM calls store results in registers starting from 0
                    if (call->returnCount() > 0) {
                        if (call->returnCount() > 1 && std::holds_alternative<Tuple>(result)) {
                            // Flatten the (possibly nested) tuple depth-first into
                            // consecutive registers, matching the flat layout that
                            // reconstructTuple reads back. A naive top-level unpack
                            // would put a sub-tuple into a scalar register and leave
                            // later registers unset.
                            size_t regIndex                                  = 0;
                            std::function<void(const ValueVariant&)> flatten = [&](const ValueVariant& v) {
                                if (std::holds_alternative<Tuple>(v)) {
                                    for (const auto& element : std::get<Tuple>(v)->elements)
                                        flatten(element);
                                } else {
                                    registers[regIndex++] = { v, type::Type(type::TypeKind::Unspecified) };
                                }
                            };
                            flatten(result);
                        } else {
                            registers[0] = { result, type::Type(type::TypeKind::Unspecified) };
                        }
                    }

                } else {
                    // External function not found - this is an error
                    std::cerr << "Error: External function '" << call->functionName() << "' not registered" << std::endl;
                    // Set default values for return registers
                    for (size_t i = 0; i < call->returnCount(); ++i) {
                        registers[i] = { getDefaultValue(type::Type(type::TypeKind::Number)),
                                         type::Type(type::TypeKind::Unspecified) };
                    }
                }
            } else {
                auto it = labelMap.find(call->functionName());
                if (it != labelMap.end()) {
                    pushCallFrame(pc);
                    pc = it->second;
                    continue;
                }
            }
            pc++;
            continue;
        }

        if (auto* strLit = dynamic_cast<RVMInstrStringLiteral*>(instr.get())) {
            stringTable[strLit->destination().stringId()] = strLit->stringValue();
            pc++;
            continue;
        }

        if (auto* mov = dynamic_cast<RVMInstr2Op*>(instr.get())) {
            ValueVariant srcVal = evaluateValue(mov->source());
            if (mov->opcode() == Opcode::MOV) {
                setRegister(mov->destination(), srcVal);
            } else {
                ValueVariant result = applyUnaryOp(mov->opcode(), srcVal);
                setRegister(mov->destination(), result);
            }
            pc++;
            continue;
        }

        if (auto* arith = dynamic_cast<RVMInstr3Op*>(instr.get())) {
            ValueVariant src1   = evaluateValue(arith->source1());
            ValueVariant src2   = evaluateValue(arith->source2());
            ValueVariant result = applyBinaryOp(arith->opcode(), src1, src2);
            setRegister(arith->destination(), result);
            pc++;
            continue;
        }

        pc++;
    }

    if (returnType.isTuple()) {
        size_t regIndex = 0;
        return reconstructTuple(returnType, regIndex);
    } else if (returnType.isVoid()) {
        return ValueVariant{};
    } else {
        if (registers.find(0) != registers.end())
            return registers[0].value;
        return getDefaultValue(returnType);
    }
}

ValueVariant RVMInterpreter::parseValue(const std::string& str)
{
    if (str == "true")
        return true;
    if (str == "false")
        return false;

    // Check for number (has dot or 'f' suffix)
    if (str.find('.') != std::string::npos || (str.back() == 'f' && str.size() > 1 && std::isdigit(str[str.size() - 2]))) {
        std::string s = str;
        if (s.back() == 'f')
            s.pop_back();
        try {
            return std::stod(s);
        } catch (...) {
            return 0.0;
        }
    }

    // Try integer
    try {
        return static_cast<Integer>(std::stoll(str));
    } catch (...) {
        // Fallback to string if anything else
        return str;
    }
}

void RVMInterpreter::pushCallFrame(size_t pc)
{
    callStack.push_back({ registers, pc });
}

size_t RVMInterpreter::popCallFrame(size_t retCount)
{
    auto cf = callStack.back();
    callStack.pop_back();

    // Restore caller's registers, but skip return value registers (0..retCount-1)
    for (const auto& [reg, val] : cf.Registers) {
        if (reg >= retCount)
            registers[reg] = val;
    }

    return cf.ReturnPC;
}

ValueVariant RVMInterpreter::evaluateValue(const RVMValue& value)
{
    if (value.isConstant()) {
        return value.constantValue();
    } else if (value.isRegister()) {
        RegId reg = value.regId();
        if (auto it = registers.find(reg); it != registers.end())
            return it->second.value;
        return getDefaultValue(value.type());
    } else if (value.isStringRef()) {
        uint32_t strId = value.stringId();
        if (auto it = stringTable.find(strId); it != stringTable.end())
            return it->second;
        return std::string("");
    }
    return Integer(0);
}

void RVMInterpreter::setRegister(const RVMValue& dst, const ValueVariant& value)
{
    if (dst.isRegister())
        registers[dst.regId()] = { value, dst.type() };
}

ValueVariant RVMInterpreter::getDefaultValue(const type::Type& type)
{
    switch (type.kind()) {
    case type::TypeKind::Boolean:
        return false;
    case type::TypeKind::Integer:
        return Integer(0);
    case type::TypeKind::Number:
        return 0.0;
    case type::TypeKind::String:
        return std::string("");
    default:
        return Integer(0);
    }
}

bool RVMInterpreter::isZero(const ValueVariant& val)
{
    if (std::holds_alternative<bool>(val))
        return !std::get<bool>(val);
    if (std::holds_alternative<Integer>(val))
        return std::get<Integer>(val) == 0;
    if (std::holds_alternative<Number>(val))
        return std::get<Number>(val) == 0.0;
    return false;
}

ValueVariant RVMInterpreter::applyUnaryOp(Opcode op, const ValueVariant& src)
{
    switch (op) {
    case Opcode::I2F:
        if (std::holds_alternative<Integer>(src))
            return static_cast<Number>(std::get<Integer>(src));
        break;
    case Opcode::F2I:
        if (std::holds_alternative<Number>(src))
            return static_cast<Integer>(std::get<Number>(src));
        break;
    default:
        break;
    }
    return src;
}

ValueVariant RVMInterpreter::applyBinaryOp(Opcode op, const ValueVariant& src1, const ValueVariant& src2)
{
    if (op >= Opcode::ADD && op <= Opcode::POW) {
        // Check if both operands are integers for integer arithmetic
        bool bothIntegers = std::holds_alternative<Integer>(src1) && std::holds_alternative<Integer>(src2);

        if (bothIntegers) {
            Integer a = std::get<Integer>(src1);
            Integer b = std::get<Integer>(src2);

            switch (op) {
            case Opcode::ADD:
                return a + b;
            case Opcode::SUB:
                return a - b;
            case Opcode::MUL:
                return a * b;
            case Opcode::DIV:
                if (b == 0)
                    return static_cast<Integer>(0);
                if (a == std::numeric_limits<Integer>::min() && b == -1)
                    return a; // avoid INT_MIN / -1 overflow (well-defined two's-complement result)
                return a / b; // Integer division
            case Opcode::MOD:
                if (b == 0)
                    return static_cast<Integer>(0);
                if (a == std::numeric_limits<Integer>::min() && b == -1)
                    return static_cast<Integer>(0);
                return a % b;
            case Opcode::POW:
                return static_cast<Integer>(std::pow(static_cast<Number>(a), static_cast<Number>(b)));
            default:
                break;
            }
        } else {
            // Floating-point arithmetic
            Number a = 0.0, b = 0.0;
            if (std::holds_alternative<Integer>(src1))
                a = static_cast<Number>(std::get<Integer>(src1));
            else if (std::holds_alternative<Number>(src1))
                a = std::get<Number>(src1);

            if (std::holds_alternative<Integer>(src2))
                b = static_cast<Number>(std::get<Integer>(src2));
            else if (std::holds_alternative<Number>(src2))
                b = std::get<Number>(src2);

            switch (op) {
            case Opcode::ADD:
                return a + b;
            case Opcode::SUB:
                return a - b;
            case Opcode::MUL:
                return a * b;
            case Opcode::DIV:
                return (b == 0.0) ? std::numeric_limits<Number>::infinity() : a / b;
            case Opcode::MOD:
                return (b == 0.0) ? 0.0 : std::fmod(a, b);
            case Opcode::POW:
                return std::pow(a, b);
            default:
                break;
            }
        }
    }

    if (op >= Opcode::CMP_EQ && op <= Opcode::CMP_GE) {
        if (std::holds_alternative<std::string>(src1) && std::holds_alternative<std::string>(src2)) {
            const auto& s1 = std::get<std::string>(src1);
            const auto& s2 = std::get<std::string>(src2);
            if (op == Opcode::CMP_EQ)
                return s1 == s2;
            if (op == Opcode::CMP_NE)
                return s1 != s2;
            return false;
        }

        Number a = 0.0, b = 0.0;
        if (std::holds_alternative<Integer>(src1))
            a = static_cast<Number>(std::get<Integer>(src1));
        else if (std::holds_alternative<Number>(src1))
            a = std::get<Number>(src1);

        if (std::holds_alternative<Integer>(src2))
            b = static_cast<Number>(std::get<Integer>(src2));
        else if (std::holds_alternative<Number>(src2))
            b = std::get<Number>(src2);

        switch (op) {
        case Opcode::CMP_EQ:
            return a == b;
        case Opcode::CMP_NE:
            return a != b;
        case Opcode::CMP_LT:
            return a < b;
        case Opcode::CMP_LE:
            return a <= b;
        case Opcode::CMP_GT:
            return a > b;
        case Opcode::CMP_GE:
            return a >= b;
        default:
            break;
        }
    }

    if (op >= Opcode::AND && op <= Opcode::XOR) {
        // Logical operations on boolean truthiness. The mapper emits AND/OR for
        // the && / || operators and XOR (against 1) for the unary ! operator.
        bool a = !isZero(src1);
        bool b = !isZero(src2);
        switch (op) {
        case Opcode::AND:
            return a && b;
        case Opcode::OR:
            return a || b;
        case Opcode::XOR:
            return a != b;
        default:
            break;
        }
    }

    return Integer(0);
}

ValueVariant RVMInterpreter::reconstructTuple(const type::Type& type, size_t& regIndex)
{
    if (type.isTuple()) {
        auto tuple = std::make_shared<TupleVariant>();
        for (const auto& comp : type.components())
            tuple->elements.push_back(reconstructTuple(comp, regIndex));
        return tuple;
    } else {
        if (registers.find(regIndex) != registers.end()) {
            ValueVariant val = registers[regIndex].value;
            regIndex++;
            return val;
        }
        regIndex++;
        return getDefaultValue(type);
    }
}

} // namespace PExpr::rvm
