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
#include "rvm/RVMInterpreter.h"
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
@[extern, pure] fn sqrt(x:num) -> num;
@[extern, pure] fn sin(x:num) -> num;
@[extern, pure] fn cos(x:num) -> num;
@[extern, pure] fn tan(x:num) -> num;
@[extern, pure] fn log(x:num) -> num;
@[extern, pure] fn exp(x:num) -> num;
@[extern] fn readInteger() -> int;
@[extern] fn readNumber() -> num;
@[extern] fn readString() -> str;
@[extern] fn print(b: bool) -> void;
@[extern] fn print(n: num) -> void;
@[extern] fn print(s: str) -> void;
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
    type::Type returnType = type::Type(type::TypeKind::Void);

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