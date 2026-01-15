#include <cmath>
#include <fstream>
#include <iostream>

#include <CLI/CLI.hpp>

#include "PExpr.h"

using namespace PExpr;

static void handleListCLIOptions(const CLI::App& app)
{
    const auto options = app.get_options();
    for (auto option : options) {
        const auto shortNames = option->get_snames();
        for (const auto& s : shortNames)
            std::cout << "-" << s << std::endl;

        const auto longNames = option->get_lnames();
        for (const auto& s : longNames)
            std::cout << "--" << s << std::endl;
    }
}

int main(int argc, char** argv)
{
    CLI::App app{ "PExpr compiler", argc >= 1 ? argv[0] : "pexpr" };
    argv = app.ensure_utf8(argv);

    app.positionals_at_end(false);
    app.set_version_flag("--version", "0.1");
    app.set_help_flag("-h,--help", "Shows help message and exit");

    std::filesystem::path inputFile;
    app.add_option("file", inputFile, "File to compile.")->required(true)->check(CLI::ExistingFile);

    std::filesystem::path outputFile;
    app.add_option("-o,--output", outputFile, "Output file.");

    bool useStdOutput = false;
    app.add_flag("--std-output", useStdOutput, "Dump the result into the standard output");

    bool emitAST = false;
    app.add_flag("--emit-ast", emitAST, "Emit AST instead of the IR");

    uint32_t warningFlags = RT_WARNING_DEFAULT;
    bool warningAsError   = false;

    const auto handleWarningCmd = [&](const std::string& name) {
        if (name == "all")
            warningFlags = RT_WARNING_ALL;
        else if (name == "default")
            warningFlags = RT_WARNING_DEFAULT;
        else if (name == "none")
            warningFlags = 0;
        else if (name == "error")
            warningAsError = true;
        else if (name == "trailing-semicolon")
            warningFlags |= RT_WARNING_TRAILING_SEMICOLON;
        else if (name == "no-trailing-semicolon")
            warningFlags &= ~RT_WARNING_TRAILING_SEMICOLON;
        else if (name == "implicit-cast")
            warningFlags |= RT_WARNING_IMPLICIT_CAST;
        else if (name == "no-implicit-cast")
            warningFlags &= ~RT_WARNING_IMPLICIT_CAST;
        else
            throw CLI::RuntimeError();
    };
    app.add_option_function<std::string>("-W,--warning", handleWarningCmd, "Set warnings");
    app.add_flag_callback("--no-warnings", [&]() { warningFlags = 0; }, "Disable all warnings");

    ssa::SSAOptions optimizationOptions;
    app.add_option_function<int>("-O", [&](int opt) { 
        if (opt == 0)
            optimizationOptions = ssa::SSAOptions{
                .EnableConstantFolding = false,
                .EnableConstantFoldingNumber = false,
                .RemoveDeadCode = false,
                .InlineFunctions = false,
            };
        else if (opt == 1)
            optimizationOptions = ssa::SSAOptions{
                .EnableConstantFolding = true,
                .EnableConstantFoldingNumber = false,
                .RemoveDeadCode = true,
                .InlineFunctions = false,
            };
       else 
            optimizationOptions = ssa::SSAOptions{
                .EnableConstantFolding = true,
                .EnableConstantFoldingNumber = true,
                .RemoveDeadCode = true,
                .InlineFunctions = true,
            }; }, "Set optimization level");

    app.add_flag("--opt-constant-folding,!--no-opt-constant-folding", optimizationOptions.EnableConstantFolding, "Enable constant folding");
    app.add_flag("--opt-math-folding,!--no-opt-math-folding", optimizationOptions.EnableConstantFoldingNumber, "Enable constant folding on numbers");
    app.add_flag("--opt-dead-code,!--no-opt-dead-code", optimizationOptions.RemoveDeadCode, "Enable removal of dead code");
    app.add_flag("--opt-inline-functions,!--no-opt-inline-functions", optimizationOptions.InlineFunctions, "Attemp to inline functions");

    bool skipOptimizationPass = false;
    app.add_flag("--skip-optimization,!--no-skip-optimization", skipOptimizationPass, "Skip the optimization pass. Not recommended");

    // Add some hidden commandline parameters
    bool listCLI = false;
    auto grp     = app.add_option_group("");
    grp->add_flag("--list-cli-options", listCLI);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        app.exit(e);
        return EXIT_FAILURE;
    }

    if (listCLI) {
        handleListCLIOptions(app);
        return EXIT_SUCCESS;
    }

    if (inputFile.empty()) {
        std::cout << "No file given" << std::endl;
        return EXIT_SUCCESS;
    }

    if (outputFile.empty()) {
        outputFile = inputFile;
        if (emitAST)
            outputFile.replace_filename(inputFile.stem().generic_string() + std::string("-ast.pexpr"));
        else
            outputFile.replace_extension(".pexprir");
    }

    const auto dumpOutput = [&](const std::string& result) {
        if (useStdOutput) {
            std::cout << result;
        } else {
            std::ofstream f(outputFile);
            f << result;
        }
    };

    // Handle source files
    std::stringstream sourceFile;
    {
        std::ifstream f(inputFile);
        sourceFile << f.rdbuf();
    }

    // Parse
    Environment env;
    env.reporter().setOutputMask(warningFlags);

    auto ast = env.parse(sourceFile);

    if (ast == nullptr)
        return env.reporter().errorCount();

    if (warningAsError && env.reporter().warningCount() > 0) {
        std::cerr << "Terminating as a warning was generated" << std::endl;
        return env.reporter().warningCount();
    }

    if (emitAST) {
        dumpOutput(StringVisitor::visit(ast) + "\n");
        return env.reporter().errorCount();
    }

    // Map to IR
    ssa::SSAMapper mapper;
    auto program = mapper.map(ast);

    if (warningAsError && env.reporter().warningCount() > 0) {
        std::cerr << "Terminating as a warning was generated" << std::endl;
        return env.reporter().warningCount();
    }

    if (!ssa::SSAValidator::checkIfTyped(&program)) {
        std::cerr << "The SSA will be invalid due to unspecified typing!" << std::endl;
        return env.reporter().errorCount();
    }

    if (skipOptimizationPass) {
        dumpOutput(program.dump());
        return env.reporter().errorCount();
    }

    // Optimize
    ssa::SSAPassSSCP::Run(optimizationOptions, program);

    if (warningAsError && env.reporter().warningCount() > 0) {
        std::cerr << "Terminating as a warning was generated" << std::endl;
        return env.reporter().warningCount();
    }

    dumpOutput(program.dump());

    if (!ssa::SSAValidator::checkIfTyped(&program)) {
        std::cerr << "Computed SSA is invalid due to unspecified typing!" << std::endl;
        return env.reporter().errorCount();
    }

    return env.reporter().errorCount();
}