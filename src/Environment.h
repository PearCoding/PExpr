#pragma once

#include "ast/Closure.h"
#include "opt/OptimizerOptions.h"
#include "utils/Reporter.h"

namespace PExpr {
namespace ssa {
class SSAProgram;
}

/// Main class for parsing and transpiling.
class Environment {
public:
    /// Creates an empty environment.
    Environment();
    /// Destroys an environment.
    ~Environment();

    /// Parse the stream until eof and return the corresponding AST tree.
    /// If an error was detected, a nullptr will be returned instead.
    /// @param filename Optional filename for error reports
    Ptr<ast::Closure> parse(std::istream& stream, const std::filesystem::path& filename = {});

    /// Parse the given string and return the corresponding AST tree.
    /// If an error was detected, a nullptr will be returned instead.
    /// @param filename Optional filename for error reports
    Ptr<ast::Closure> parse(std::string_view str, const std::filesystem::path& filename = {});

    [[nodiscard]] ssa::SSAProgram map(const Ptr<ast::Closure>& closure);

    bool optimize(ssa::SSAProgram& program, const opt::OptimizerOptions& options);

    [[nodiscard]] inline const utils::Reporter& reporter() const { return mReporter; }
    [[nodiscard]] inline utils::Reporter& reporter() { return mReporter; }

private:
    /// If no error was found, true will be returned, false otherwise.
    bool doTypeChecking(const Ptr<ast::Closure>& closure);
    utils::Reporter mReporter;
};
} // namespace PExpr
