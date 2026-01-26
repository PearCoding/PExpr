#pragma once

#include "Closure.h"
#include "Lookup.h"
#include "Reporter.h"

namespace PExpr {
/// Main class for parsing and transpiling.
class Environment {
public:
    /// Creates an empty environment.
    Environment();
    /// Destroys an environment.
    ~Environment();

    /// Register an inmutable variable with a specific type.
    void registerVariable(const std::string& name, const Type& type);

    /// Register an external function.
    void registerFunction(const std::string& name, const std::vector<Type>& parameterTypes, const Type& returnType, bool hasSideEffect = true);

    /// Parse the stream until eof and return the corresponding AST tree.
    /// If an error was detected, a nullptr will be returned instead.
    Ptr<Closure> parse(std::istream& stream);

    /// Parse the given string and return the corresponding AST tree.
    /// If an error was detected, a nullptr will be returned instead.
    Ptr<Closure> parse(std::string_view str);

    [[nodiscard]] inline const Reporter& reporter() const { return mReporter; }
    [[nodiscard]] inline Reporter& reporter() { return mReporter; }

private:
    /// If no error was found, true will be returned, false otherwise.
    bool doTypeChecking(const Ptr<Closure>& closure);

    internal::SymbolTable mGlobals;
    Reporter mReporter;
};
} // namespace PExpr
