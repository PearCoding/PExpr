#include <cmath>
#include <fstream>
#include <iostream>

#include <CLI/CLI.hpp>

#include "Environment.h"
#include "opt/SSAOptimizer.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMOptimizer.h"
#include "rvm/RVMSerializer.h"
#include "rvm/RVMValidator.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"
#include "ssa/SSAValidator.h"
#include "utils/StringVisitor.h"

using namespace PExpr;
using namespace PExpr::utils;

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
    app.add_flag("--emit-ast", emitAST, "Emit AST instead of the SSA IR");

    bool emitRVM = false;
    app.add_flag("--emit-rvm", emitRVM, "Emit RVM (register-based virtual machine) IR instead of the SSA IR");

    bool readSSAIR = false;
    app.add_flag("--input-ir", readSSAIR, "Read SSA IR produced by a previous run instead of a file with PExpr syntax");

    bool readRVMIR = false;
    app.add_flag("--input-rvm", readRVMIR, "Read RVM IR produced by a previous run instead of a file with PExpr syntax");

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
            warningFlags &= ~(uint32_t)RT_WARNING_TRAILING_SEMICOLON;
        else if (name == "implicit-cast")
            warningFlags |= RT_WARNING_IMPLICIT_CAST;
        else if (name == "no-implicit-cast")
            warningFlags &= ~(uint32_t)RT_WARNING_IMPLICIT_CAST;
        else if (name == "implicit-cast-int")
            warningFlags |= RT_WARNING_IMPLICIT_CAST_INT;
        else if (name == "no-implicit-cast-int")
            warningFlags &= ~(uint32_t)RT_WARNING_IMPLICIT_CAST_INT;
        else if (name == "pure-internal-function")
            warningFlags |= RT_WARNING_PURE_INTERNAL_FUNCTIONS;
        else if (name == "no-pure-internal-function")
            warningFlags &= ~(uint32_t)RT_WARNING_PURE_INTERNAL_FUNCTIONS;
        else if (name == "unused-return")
            warningFlags |= RT_WARNING_UNUSED_CLOSURE_RETURN;
        else if (name == "no-unused-return")
            warningFlags &= ~(uint32_t)RT_WARNING_UNUSED_CLOSURE_RETURN;
        else
            throw CLI::RuntimeError();
    };
    app.add_option_function<std::string>("-W,--warning", handleWarningCmd, "Set warnings");
    app.add_flag_callback("--no-warnings", [&]() { warningFlags = 0; }, "Disable all warnings");

    opt::OptimizerOptions optimizationOptions = opt::OptimizerOptions::None();
    app.add_option_function<int>("-O", [&](int opt) { 
        if (opt == 0)
            optimizationOptions = opt::OptimizerOptions::None();
        else if (opt == 1)
            optimizationOptions = opt::OptimizerOptions::Low();
        else if (opt == 2)
            optimizationOptions = opt::OptimizerOptions::Medium();
       else 
            optimizationOptions = opt::OptimizerOptions::High(); }, "Set optimization level");

    app.add_flag("--opt-constant-folding,!--no-opt-constant-folding", optimizationOptions.EnableConstantFolding, "Enable constant folding");
    app.add_flag("--opt-math-folding,!--no-opt-math-folding", optimizationOptions.EnableConstantFoldingNumber, "Enable constant folding on numbers");
    app.add_flag("--opt-dead-code,!--no-opt-dead-code", optimizationOptions.RemoveDeadCode, "Enable removal of dead code");
    app.add_flag("--opt-inline-functions,!--no-opt-inline-functions", optimizationOptions.InlineFunctions, "Attempt to inline functions");
    app.add_flag("--opt-force-inline-functions,!--no-opt-force-inline-functions", optimizationOptions.ForceInlineFunctions, "Force inline all internal functions, eliminating all functions from IR");
    app.add_flag("--opt-math-identities,!--no-opt-math-identities", optimizationOptions.ApplyMathIdentities, "Apply math identities");
    app.add_flag("--opt-trigonometric-identities,!--no-opt-trigonometric-identities", optimizationOptions.ApplyTrigonometricIdentities, "Apply trigonometric identities");
    app.add_flag("--opt-cse,!--no-opt-cse", optimizationOptions.EliminateCommonSubexpressions, "Eliminate common subexpressions");
    app.add_flag("--opt-pre,!--no-opt-pre", optimizationOptions.EliminatePartialRedundancies, "Eliminate partial redundancies");
    app.add_flag("--opt-dissolve-tuples,!--no-opt-dissolve-tuples", optimizationOptions.DissolveTuples, "Dissolve tuples");
    app.add_flag("--opt-rvm-identity-moves,!--no-opt-rvm-identity-moves", optimizationOptions.OptimizeIdentityMoves, "Optimize RVM identity moves (redundant code)");
    app.add_flag("--opt-rvm-move-chains,!--no-opt-rvm-move-chains", optimizationOptions.OptimizeMoveChains, "Optimize RVM move chains");
    app.add_flag("--opt-rvm-redundant-moves,!--no-opt-rvm-redundant-moves", optimizationOptions.OptimizeRedundantMoves, "Eliminate redundant MOV instructions in RVM");
    app.add_flag("--opt-rvm-constant-folding,!--no-opt-rvm-constant-folding", optimizationOptions.OptimizeConstantPropagation, "Enable constant folding in RVM");
    app.add_flag("--opt-rvm-register-allocation,!--no-opt-rvm-register-allocation", optimizationOptions.EnableRegisterAllocation, "Enable register allocation to minimize register count");

    bool skipOptimizationPass = false;
    app.add_flag("--skip-optimization", skipOptimizationPass, "Skip the optimization pass. Not recommended");

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

    if (readSSAIR & emitAST) {
        std::cerr << "Can't read SSA IR and emit AST afterwards" << std::endl;
        return 1;
    }

    if (readRVMIR & (emitAST || !emitRVM)) {
        std::cerr << "Can't read RVM IR and emit AST or SSA IR afterwards" << std::endl;
        return 1;
    }

    if (emitRVM) {
        if (skipOptimizationPass) {
            std::cerr << "Can't skip optimization when emitting RVM" << std::endl;
            return 1;
        }
        if (!optimizationOptions.DissolveTuples) {
            // std::cout << "Enabled '--opt-dissolve-tuples' for RVM" << std::endl;
            optimizationOptions.DissolveTuples = true;
            optimizationOptions.RemoveDeadCode = true; //< Needed to dissolve tuples
        }
    }

    if (outputFile.empty()) {
        outputFile = inputFile;
        if (emitAST)
            outputFile.replace_filename(inputFile.stem().generic_string() + std::string("-ast.pexpr"));
        else if (emitRVM)
            outputFile.replace_filename(inputFile.stem().generic_string() + std::string(".pexprrvm"));
        else
            outputFile.replace_extension(".pexprir");
    }

    const auto dumpOutput = [&](const std::string& result) {
        if (useStdOutput) {
            std::cout << result << std::flush;
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

    rvm::RVMProgram rvmProgram;
    if (!readRVMIR) {
        ssa::SSAProgram ssaProgram;
        if (!readSSAIR) {
            auto ast = env.parse(sourceFile, std::filesystem::absolute(inputFile));

            if (ast == nullptr)
                return env.reporter().errorCount();

            if (warningAsError && env.reporter().warningCount() > 0) {
                PEXPR_LOG_ERROR << "Terminating as a warning was generated" << std::endl;
                return env.reporter().warningCount();
            }

            if (emitAST) {
                dumpOutput(StringVisitor::visit(ast) + "\n");
                return env.reporter().errorCount();
            }

            // Map to IR
            ssaProgram = env.map(ast);
        } else {
            ssaProgram = ssa::SSASerializer::read(sourceFile);
        }

        if (!ssa::SSAValidator::checkIfTyped(&ssaProgram)) {
            PEXPR_LOG_ERROR << "The SSA will be invalid due to unspecified typing!" << std::endl;
            dumpOutput(ssa::SSASerializer::serialize(ssaProgram));
            return env.reporter().errorCount() + 1;
        }

        if (!skipOptimizationPass) {
            // Optimize
            opt::SSAOptimizer::Run(optimizationOptions, ssaProgram);

            if (!ssa::SSAValidator::checkIfTyped(&ssaProgram)) {
                PEXPR_LOG_ERROR << "Computed SSA is invalid due to unspecified typing!" << std::endl;
                return env.reporter().errorCount() + 1;
            }
        }

        if (!emitRVM) {
            dumpOutput(ssa::SSASerializer::serialize(ssaProgram));
            return env.reporter().errorCount();
        }

        rvm::RVMMapper mapper;
        rvmProgram = mapper.mapProgram(ssaProgram);
    } else {
        auto rvmProgOpt = rvm::RVMSerializer::read(sourceFile);
        if (!rvmProgOpt.has_value()) {
            PEXPR_LOG_ERROR << "Failed to parse given RVM file" << std::endl;
            return env.reporter().errorCount() + 1;
        }
        rvmProgram = rvmProgOpt.value();
    }

    if (!rvm::RVMValidator::checkIfElementary(rvmProgram))
        PEXPR_LOG_WARNING << "Constructed RVM program is invalid due to non-elementary types in registers" << std::endl;

    // Apply RVM optimizations
    rvm::RVMOptimizer::optimize(optimizationOptions, rvmProgram);

    dumpOutput(rvm::RVMSerializer::serialize(rvmProgram));

    return env.reporter().errorCount();
}