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

    std::vector<std::filesystem::path> files;
    app.add_option("files", files, "Files to compile.")->required(true)->check(CLI::ExistingFile);

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

    uint32_t optLevel = 0;
    app.add_option_function<int>("-O", [&](int opt) { optLevel = (uint32_t)std::min(2, std::max(0, opt)); }, "Set optimization level");

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

    if (files.empty()) {
        std::cout << "No files given" << std::endl;
        return EXIT_SUCCESS;
    }

    // Handle source files
    std::stringstream sourceFiles;
    for (const auto& path : files) {
        std::ifstream f(path);
        sourceFiles << f.rdbuf();
    }

    // Parse
    Environment env;
    env.reporter().setOutputMask(warningFlags);

    auto ast = env.parse(sourceFiles);
    std::cout.flush();
    std::cerr.flush();

    if (ast == nullptr)
        return env.reporter().errorCount();

    if (warningAsError && env.reporter().warningCount() > 0) {
        std::cerr << "Terminating as a warning was generated" << std::endl;
        return env.reporter().warningCount();
    }

    if (emitAST) {
        std::cout << StringVisitor::visit(ast) << std::endl;
        return env.reporter().errorCount();
    }

    // Map to IR
    ssa::SSAMapper mapper;
    auto program = mapper.map(ast);
    std::cout.flush();
    std::cerr.flush();

    if (warningAsError && env.reporter().warningCount() > 0) {
        std::cerr << "Terminating as a warning was generated" << std::endl;
        return env.reporter().warningCount();
    }

    if (!ssa::SSAValidator::checkIfTyped(&program)) {
        std::cerr << "The SSA will be invalid due to unspecified typing!" << std::endl;
        return env.reporter().errorCount();
    }

    if (optLevel == 0) {
        std::cout << program.dump();
        return env.reporter().errorCount();
    }

    // Optimize
    ssa::SSAPassSSCP sscp;
    sscp.run(program);
    std::cout.flush();
    std::cerr.flush();

    if (warningAsError && env.reporter().warningCount() > 0) {
        std::cerr << "Terminating as a warning was generated" << std::endl;
        return env.reporter().warningCount();
    }

    std::cout << program.dump();

    if (!ssa::SSAValidator::checkIfTyped(&program)) {
        std::cerr << "Computed SSA is invalid due to unspecified typing!" << std::endl;
        return env.reporter().errorCount();
    }

    return env.reporter().errorCount();
}