#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <CLI/CLI.hpp>

#include "Environment.h"
#include "opt/SSAOptimizer.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMOptimizer.h"
#include "rvm/RVMSerializer.h"
#include "rvm/RVMValue.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"
#include "type/Mangler.h"
#include "type/Type.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;

// Helper function to recursively print ValueVariant
void printValueVariant(const ValueVariant& value, std::ostream& os = std::cout)
{
    if (std::holds_alternative<bool>(value)) {
        os << (std::get<bool>(value) ? "true" : "false");
    } else if (std::holds_alternative<Integer>(value)) {
        os << std::get<Integer>(value);
    } else if (std::holds_alternative<Number>(value)) {
        os << std::get<Number>(value);
    } else if (std::holds_alternative<std::string>(value)) {
        os << std::get<std::string>(value);
    } else if (std::holds_alternative<Tuple>(value)) {
        auto tuple = std::get<Tuple>(value);
        os << "[";
        for (size_t i = 0; i < tuple->elements.size(); ++i) {
            if (i > 0)
                os << ", ";
            printValueVariant(tuple->elements[i], os);
        }
        os << "]";
    }
}

// Interpreter state
class RVMInterpreter {
public:
    struct RegisterValue {
        ValueVariant value;
        Type type;
    };

    RVMInterpreter() = default;

    void registerExternalFunction(const std::string& name,
                                  std::function<ValueVariant(const std::vector<ValueVariant>&)> func)
    {
        externalFunctions[name] = func;
    }

    // Helper to count elementary elements in a type
    size_t countElementaryElements(const Type& type)
    {
        if (type.isTuple()) {
            size_t count = 0;
            for (const auto& comp : type.components())
                count += countElementaryElements(comp);
            return count;
        } else {
            // Elementary type (bool, int, num, string)
            return 1;
        }
    }

    // Helper to reconstruct tuple from linearized registers
    ValueVariant reconstructTuple(const Type& type, size_t& regIndex)
    {
        if (type.isTuple()) {
            auto tuple = std::make_shared<TupleVariant>();
            for (const auto& comp : type.components())
                tuple->elements.push_back(reconstructTuple(comp, regIndex));
            return ValueVariant{ tuple };
        } else {
            // Elementary type
            if (registers.find(regIndex) != registers.end()) {
                ValueVariant val = registers[regIndex].value;
                regIndex++;
                return val;
            } else {
                // Missing register, return default
                regIndex++;
                return getDefaultValue(type);
            }
        }
    }

    ValueVariant execute(const RVMProgram& program, const Type& returnType)
    {
        registers.clear();
        stringTable.clear();

        // Map from label name to instruction index
        std::unordered_map<std::string, size_t> labelMap;
        for (size_t i = 0; i < program.size(); ++i) {
            if (auto* label = dynamic_cast<RVMInstrLabel*>(program[i].get()))
                labelMap[label->labelName()] = i;
        }

        std::vector<size_t> returnStack;

        size_t pc = 0;
        while (pc < program.size()) {
            auto& instr = program[pc];

            if (dynamic_cast<RVMInstrLabel*>(instr.get()) || dynamic_cast<RVMInstrComment*>(instr.get())) {
                pc++;
                continue;
            }

            if (dynamic_cast<RVMInstrReturn*>(instr.get())) {
                if (returnStack.empty())
                    break;
                pc = returnStack.back();
                returnStack.pop_back();
                pc++;
                continue;
            }

            if (auto* branch = dynamic_cast<RVMInstrBranch*>(instr.get())) {
                auto cond         = evaluateValue(branch->srcs()[0]);
                bool shouldBranch = false;

                if (branch->opcode() == Opcode::BR)
                    shouldBranch = std::get<bool>(cond);
                else if (branch->opcode() == Opcode::BRZ)
                    shouldBranch = isZero(cond);
                else if (branch->opcode() == Opcode::BRNZ)
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

            if (auto* pushFrame = dynamic_cast<RVMInstrPushFrame*>(instr.get())) {
                // Save registers to stack
                for (uint32_t i = 1; i <= pushFrame->registerCount(); ++i) {
                    RegId reg = i - 1;
                    if (registers.find(reg) != registers.end())
                        registerStack.push_back({ reg, registers[reg] });
                }
                pc++;
                continue;
            }

            if (auto* popFrame = dynamic_cast<RVMInstrPopFrame*>(instr.get())) {
                // Restore registers from stack
                for (uint32_t i = popFrame->registerCount(); i >= 1; --i) {
                    RegId reg = i - 1;
                    if (!registerStack.empty() && registerStack.back().first == reg) {
                        registers[reg] = registerStack.back().second;
                        registerStack.pop_back();
                    }
                }
                pc++;
                continue;
            }

            if (auto* internal_call = dynamic_cast<RVMInstrInternalCall*>(instr.get())) {
                // A call is just an annotated jump
                auto it = labelMap.find(internal_call->functionName());
                if (it != labelMap.end()) {
                    returnStack.push_back(pc);
                    pc = it->second;
                    continue;
                }
                pc++;
                continue;
            }

            if (auto* external_call = dynamic_cast<RVMInstrExternalCall*>(instr.get())) {
                std::vector<ValueVariant> args;
                for (const auto& arg : external_call->srcs())
                    args.push_back(evaluateValue(arg));

                auto it = externalFunctions.find(external_call->functionName());
                if (it != externalFunctions.end()) {
                    ValueVariant result = it->second(args);
                    if (external_call->dst().has_value())
                        setRegister(external_call->dst().value(), result);
                } else {
                    std::cerr << "Error: Unknown external function '" << external_call->functionName() << "'" << std::endl;
                }
                pc++;
                continue;
            }

            if (auto* strLit = dynamic_cast<RVMInstrStringLiteral*>(instr.get())) {
                stringTable[strLit->dst().value().stringId()] = strLit->stringValue();
                pc++;
                continue;
            }

            if (auto* mov = dynamic_cast<RVMInstr2Op*>(instr.get())) {
                if (mov->opcode() == Opcode::MOV) {
                    ValueVariant srcVal = evaluateValue(mov->srcs()[0]);
                    setRegister(mov->dst().value(), srcVal);
                } else {
                    // Unary operation
                    ValueVariant srcVal = evaluateValue(mov->srcs()[0]);
                    ValueVariant result = applyUnaryOp(mov->opcode(), srcVal);
                    setRegister(mov->dst().value(), result);
                }
                pc++;
                continue;
            }

            if (auto* arith = dynamic_cast<RVMInstr3Op*>(instr.get())) {
                ValueVariant src1   = evaluateValue(arith->srcs()[0]);
                ValueVariant src2   = evaluateValue(arith->srcs()[1]);
                ValueVariant result = applyBinaryOp(arith->opcode(), src1, src2);
                setRegister(arith->dst().value(), result);
                pc++;
                continue;
            }

            // Unknown instruction
            std::cerr << "Error: Unknown instruction at PC=" << pc << std::endl;
            pc++;
        }

        // Debug: print register contents
        if (returnType.isTuple()) {
            // Reconstruct tuple from linearized registers
            size_t regIndex = 0;
            return reconstructTuple(returnType, regIndex);
        } else {
            // Single value return
            if (registers.find(0) != registers.end())
                return registers[0].value;
            return ValueVariant{ Integer(0) };
        }
    }

private:
    std::unordered_map<RegId, RegisterValue> registers;
    std::unordered_map<uint32_t, std::string> stringTable;
    std::vector<std::pair<RegId, RegisterValue>> registerStack;
    std::unordered_map<std::string, std::function<ValueVariant(const std::vector<ValueVariant>&)>> externalFunctions;

    ValueVariant evaluateValue(const RVMValue& value)
    {
        if (value.isConstant()) {
            return value.constantValue();
        } else if (value.isRegister()) {
            RegId reg = value.regId();
            if (auto it = registers.find(reg); it != registers.end())
                return it->second.value;
            // Return default value based on type
            return getDefaultValue(value.type());
        } else if (value.isStringRef()) {
            uint32_t strId = value.stringId();
            if (auto it = stringTable.find(strId); it != stringTable.end())
                return it->second;

            return std::string("");
        }
        return ValueVariant{ Integer(0) };
    }

    void setRegister(const RVMValue& dst, const ValueVariant& value)
    {
        if (dst.isRegister())
            registers[dst.regId()] = { value, dst.type() };
    }

    ValueVariant getDefaultValue(const Type& type)
    {
        switch (type.kind()) {
        case TypeKind::Boolean:
            return ValueVariant{ false };
        case TypeKind::Integer:
            return ValueVariant{ Integer(0) };
        case TypeKind::Number:
            return ValueVariant{ Number(0.0) };
        case TypeKind::String:
            return ValueVariant{ std::string("") };
        default:
            return ValueVariant{ Integer(0) };
        }
    }

    bool isZero(const ValueVariant& val)
    {
        if (std::holds_alternative<bool>(val))
            return !std::get<bool>(val);
        if (std::holds_alternative<Integer>(val))
            return std::get<Integer>(val) == 0;
        if (std::holds_alternative<Number>(val))
            return std::get<Number>(val) == 0.0;
        return false;
    }

    ValueVariant applyUnaryOp(Opcode op, const ValueVariant& src)
    {
        switch (op) {
        case Opcode::I2F:
            if (std::holds_alternative<Integer>(src))
                return ValueVariant{ Number(static_cast<double>(std::get<Integer>(src))) };
            break;
        case Opcode::F2I:
            if (std::holds_alternative<Number>(src))
                return ValueVariant{ Integer(static_cast<Integer>(std::get<Number>(src))) };
            break;
        default:
            break;
        }
        return src;
    }

    ValueVariant applyBinaryOp(Opcode op, const ValueVariant& src1, const ValueVariant& src2)
    {
        // Handle numeric operations
        if (op >= Opcode::ADD && op <= Opcode::POW) {
            // Convert to numbers if needed
            double a = 0.0, b = 0.0;
            if (std::holds_alternative<Integer>(src1))
                a = static_cast<double>(std::get<Integer>(src1));
            else if (std::holds_alternative<Number>(src1))
                a = std::get<Number>(src1);
            else if (std::holds_alternative<bool>(src1))
                a = std::get<bool>(src1) ? 1.0 : 0.0;

            if (std::holds_alternative<Integer>(src2))
                b = static_cast<double>(std::get<Integer>(src2));
            else if (std::holds_alternative<Number>(src2))
                b = std::get<Number>(src2);
            else if (std::holds_alternative<bool>(src2))
                b = std::get<bool>(src2) ? 1.0 : 0.0;

            switch (op) {
            case Opcode::ADD:
                return ValueVariant{ a + b };
            case Opcode::SUB:
                return ValueVariant{ a - b };
            case Opcode::MUL:
                return ValueVariant{ a * b };
            case Opcode::DIV:
                if (b == 0.0)
                    return ValueVariant{ std::numeric_limits<double>::infinity() };
                return ValueVariant{ a / b };
            case Opcode::MOD:
                if (b == 0.0)
                    return ValueVariant{ 0.0 };
                return ValueVariant{ std::fmod(a, b) };
            case Opcode::POW:
                return ValueVariant{ std::pow(a, b) };
            default:
                break;
            }
        }

        // Handle comparisons
        if (op >= Opcode::CMP_EQ && op <= Opcode::CMP_GE) {
            // For simplicity, compare as doubles
            double a = 0.0, b = 0.0;
            if (std::holds_alternative<Integer>(src1))
                a = static_cast<double>(std::get<Integer>(src1));
            else if (std::holds_alternative<Number>(src1))
                a = std::get<Number>(src1);
            else if (std::holds_alternative<bool>(src1))
                a = std::get<bool>(src1) ? 1.0 : 0.0;
            else if (std::holds_alternative<std::string>(src1)) {
                // String comparison
                if (std::holds_alternative<std::string>(src2)) {
                    bool result = false;
                    switch (op) {
                    case Opcode::CMP_EQ:
                        result = std::get<std::string>(src1) == std::get<std::string>(src2);
                        break;
                    case Opcode::CMP_NE:
                        result = std::get<std::string>(src1) != std::get<std::string>(src2);
                        break;
                    default:
                        result = false;
                    }
                    return ValueVariant{ result };
                }
                return ValueVariant{ false };
            }

            if (std::holds_alternative<Integer>(src2))
                b = static_cast<double>(std::get<Integer>(src2));
            else if (std::holds_alternative<Number>(src2))
                b = std::get<Number>(src2);
            else if (std::holds_alternative<bool>(src2))
                b = std::get<bool>(src2) ? 1.0 : 0.0;

            bool result = false;
            switch (op) {
            case Opcode::CMP_EQ:
                result = a == b;
                break;
            case Opcode::CMP_NE:
                result = a != b;
                break;
            case Opcode::CMP_LT:
                result = a < b;
                break;
            case Opcode::CMP_LE:
                result = a <= b;
                break;
            case Opcode::CMP_GT:
                result = a > b;
                break;
            case Opcode::CMP_GE:
                result = a >= b;
                break;
            default:
                break;
            }
            return ValueVariant{ result };
        }

        return ValueVariant{ Integer(0) };
    }
};

// Built-in functions
ValueVariant math_sqrt(const std::vector<ValueVariant>& args)
{
    if (args.empty())
        return ValueVariant{ Number(0.0) };
    Number val = 0.0;
    if (std::holds_alternative<Integer>(args[0]))
        val = static_cast<Number>(std::get<Integer>(args[0]));
    else if (std::holds_alternative<Number>(args[0]))
        val = std::get<Number>(args[0]);
    else if (std::holds_alternative<bool>(args[0]))
        val = std::get<bool>(args[0]) ? 1.0 : 0.0;
    return ValueVariant{ Number(std::sqrt(val)) };
}

ValueVariant math_sin(const std::vector<ValueVariant>& args)
{
    if (args.empty())
        return ValueVariant{ Number(0.0) };
    Number val = 0.0;
    if (std::holds_alternative<Integer>(args[0]))
        val = static_cast<Number>(std::get<Integer>(args[0]));
    else if (std::holds_alternative<Number>(args[0]))
        val = std::get<Number>(args[0]);
    else if (std::holds_alternative<bool>(args[0]))
        val = std::get<bool>(args[0]) ? 1.0 : 0.0;
    return ValueVariant{ Number(std::sin(val)) };
}

ValueVariant math_cos(const std::vector<ValueVariant>& args)
{
    if (args.empty())
        return ValueVariant{ Number(0.0) };
    Number val = 0.0;
    if (std::holds_alternative<Integer>(args[0]))
        val = static_cast<Number>(std::get<Integer>(args[0]));
    else if (std::holds_alternative<Number>(args[0]))
        val = std::get<Number>(args[0]);
    else if (std::holds_alternative<bool>(args[0]))
        val = std::get<bool>(args[0]) ? 1.0 : 0.0;
    return ValueVariant{ Number(std::cos(val)) };
}

ValueVariant math_tan(const std::vector<ValueVariant>& args)
{
    if (args.empty())
        return ValueVariant{ Number(0.0) };
    Number val = 0.0;
    if (std::holds_alternative<Integer>(args[0]))
        val = static_cast<Number>(std::get<Integer>(args[0]));
    else if (std::holds_alternative<Number>(args[0]))
        val = std::get<Number>(args[0]);
    else if (std::holds_alternative<bool>(args[0]))
        val = std::get<bool>(args[0]) ? 1.0 : 0.0;
    return ValueVariant{ Number(std::tan(val)) };
}

ValueVariant math_log(const std::vector<ValueVariant>& args)
{
    if (args.empty())
        return ValueVariant{ Number(0.0) };
    Number val = 0.0;
    if (std::holds_alternative<Integer>(args[0]))
        val = static_cast<Number>(std::get<Integer>(args[0]));
    else if (std::holds_alternative<Number>(args[0]))
        val = std::get<Number>(args[0]);
    else if (std::holds_alternative<bool>(args[0]))
        val = std::get<bool>(args[0]) ? 1.0 : 0.0;
    if (val <= 0.0)
        return ValueVariant{ Number(-std::numeric_limits<Number>::infinity()) };
    return ValueVariant{ Number(std::log(val)) };
}

ValueVariant math_exp(const std::vector<ValueVariant>& args)
{
    if (args.empty())
        return ValueVariant{ Number(1.0) };
    Number val = 0.0;
    if (std::holds_alternative<Integer>(args[0]))
        val = static_cast<Number>(std::get<Integer>(args[0]));
    else if (std::holds_alternative<Number>(args[0]))
        val = std::get<Number>(args[0]);
    else if (std::holds_alternative<bool>(args[0]))
        val = std::get<bool>(args[0]) ? 1.0 : 0.0;
    return ValueVariant{ Number(std::exp(val)) };
}

ValueVariant io_readInteger(const std::vector<ValueVariant>& args)
{
    PEXPR_UNUSED(args);
    Integer val;
    std::cin >> val;
    return ValueVariant{ val };
}

ValueVariant io_readNumber(const std::vector<ValueVariant>& args)
{
    PEXPR_UNUSED(args);
    Number val;
    std::cin >> val;
    return ValueVariant{ val };
}

ValueVariant io_readString(const std::vector<ValueVariant>& args)
{
    PEXPR_UNUSED(args);
    std::string val;
    std::cin.ignore();
    std::getline(std::cin, val);
    return ValueVariant{ val };
}

ValueVariant io_print(const std::vector<ValueVariant>& args)
{
    for (const auto& arg : args)
        printValueVariant(arg);

    return ValueVariant{};
}

static const char* SRC_HEADER = R"(
//!location 1 "Header"
[[extern, pure]] fn sqrt(x:num) -> num;
[[extern, pure]] fn sin(x:num) -> num;
[[extern, pure]] fn cos(x:num) -> num;
[[extern, pure]] fn tan(x:num) -> num;
[[extern, pure]] fn log(x:num) -> num;
[[extern, pure]] fn exp(x:num) -> num;
[[extern]] fn readInteger() -> int;
[[extern]] fn readNumber() -> num;
[[extern]] fn readString() -> str;
[[extern]] fn print(b: bool) -> void;
[[extern]] fn print(n: num) -> void;
[[extern]] fn print(s: str) -> void;
let Pi = 3.141592;
)";

int main(int argc, char** argv)
{
    CLI::App app{ "pcalc - PExpr RVM Interpreter", argc >= 1 ? argv[0] : "pcalc" };
    argv = app.ensure_utf8(argv);

    std::filesystem::path inputFile;
    app.add_option("file", inputFile, "Input file (.pexpr)")->required(true);

    bool optimize = true;
    app.add_flag("--optimize,-O", optimize, "Enable optimization");

    bool verbose = false;
    app.add_flag("--verbose,-v", verbose, "Verbose output");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        app.exit(e);
        return EXIT_FAILURE;
    }

    if (!std::filesystem::exists(inputFile)) {
        std::cerr << "Error: Input file '" << inputFile << "' does not exist" << std::endl;
        return EXIT_FAILURE;
    }

    // Read input
    std::ifstream file(inputFile);
    std::stringstream buffer;
    buffer << SRC_HEADER << std::endl
           << "//!location 1 \"" << inputFile.generic_string() << "\"" << std::endl
           << file.rdbuf();
    std::string source = buffer.str();

    RVMProgram rvmProgram;
    type::Type returnType = type::Type(type::TypeKind::Unspecified);

    Environment env;
    auto ast = env.parse(source, inputFile);

    if (!ast) {
        std::cerr << "Parse error" << std::endl;
        return EXIT_FAILURE;
    }

    // Map to SSA
    auto ssaProgram = env.map(ast);

    // Extract return type from SSA IR (last entry is a return instruction)
    for (const auto& instr : ssaProgram.Body) {
        if (auto ret = dynamic_cast<const ssa::SSAInstrReturn*>(instr.get())) {
            returnType = ret->Value.type();
            break;
        }
    }

    // Optimize if requested
    if (optimize)
        opt::SSAOptimizer::Run(opt::OptimizerOptions::Medium(), ssaProgram);

    // Convert to RVM
    rvm::RVMMapper mapper;
    rvmProgram = mapper.mapProgram(ssaProgram);

    // Optimize RVM if requested
    if (optimize)
        rvm::RVMOptimizer::optimize(opt::OptimizerOptions::Medium(), rvmProgram);

    if (verbose) {
        std::cout << "=== RVM Program ===" << std::endl;
        std::cout << RVMSerializer::serialize(rvmProgram) << std::endl;
        std::cout << "=== Execution ===" << std::endl;
    }

    // Create interpreter and register built-in functions using mangled names
    RVMInterpreter interpreter;

    // Helper to compute mangled names (same as Environment::registerFunction does internally)
    auto mangleFunction = [](const std::string& name, const std::vector<type::Type>& paramTypes) -> std::string {
        return makeMangledNameFromTypes(name, paramTypes, nullptr);
    };

    // Math functions
    interpreter.registerExternalFunction(mangleFunction("sqrt", { type::Type(type::TypeKind::Number) }), math_sqrt);
    interpreter.registerExternalFunction(mangleFunction("sin", { type::Type(type::TypeKind::Number) }), math_sin);
    interpreter.registerExternalFunction(mangleFunction("cos", { type::Type(type::TypeKind::Number) }), math_cos);
    interpreter.registerExternalFunction(mangleFunction("tan", { type::Type(type::TypeKind::Number) }), math_tan);
    interpreter.registerExternalFunction(mangleFunction("log", { type::Type(type::TypeKind::Number) }), math_log);
    interpreter.registerExternalFunction(mangleFunction("exp", { type::Type(type::TypeKind::Number) }), math_exp);

    // I/O functions
    interpreter.registerExternalFunction(mangleFunction("readInteger", {}), io_readInteger);
    interpreter.registerExternalFunction(mangleFunction("readNumber", {}), io_readNumber);
    interpreter.registerExternalFunction(mangleFunction("readString", {}), io_readString);
    interpreter.registerExternalFunction(mangleFunction("print", { type::Type(type::TypeKind::Boolean) }), io_print);
    interpreter.registerExternalFunction(mangleFunction("print", { type::Type(type::TypeKind::Number) }), io_print);
    interpreter.registerExternalFunction(mangleFunction("print", { type::Type(type::TypeKind::String) }), io_print);

    // Execute
    try {
        ValueVariant result = interpreter.execute(rvmProgram, returnType);

        if (!returnType.isVoid()) {
            // Print result
            if (verbose)
                std::cout << "=== Result ===" << std::endl;

            printValueVariant(result);
            std::cout << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during execution: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}