#pragma once

#include "ast/Closure.h"
#include "utils/Reporter.h"

namespace PExpr {
/// Main class for parsing and transpiling.
class Environment {
public:
    /// Creates an empty environment.
    Environment();
    /// Destroys an environment.
    ~Environment();

    /// Register an inmutable variable with a specific type.
    void registerVariable(const std::string& name, const type::Type& type);

    /// Register an external function.
    void registerFunction(const std::string& name, const std::vector<type::Type>& parameterTypes, const type::Type& returnType, bool hasSideEffect = true);

    /// Parse the stream until eof and return the corresponding AST tree.
    /// If an error was detected, a nullptr will be returned instead.
    Ptr<ast::Closure> parse(std::istream& stream);

    /// Parse the given string and return the corresponding AST tree.
    /// If an error was detected, a nullptr will be returned instead.
    Ptr<ast::Closure> parse(std::string_view str);

    [[nodiscard]] inline const utils::Reporter& reporter() const { return mReporter; }
    [[nodiscard]] inline utils::Reporter& reporter() { return mReporter; }

private:
    /// If no error was found, true will be returned, false otherwise.
    bool doTypeChecking(const Ptr<ast::Closure>& closure);

    type::SymbolTable mGlobals;
    utils::Reporter mReporter;
};
} // namespace PExpr
