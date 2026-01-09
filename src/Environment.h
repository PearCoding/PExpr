#pragma once

#include "Closure.h"
#include "Lookup.h"
#include "Reporter.h"
#include "internal/Transpiler.h"

namespace PExpr {
/// Main class for parsing and transpiling.
class Environment {
public:
    /// Creates an empty environment.
    Environment();
    /// Destroys an environment.
    ~Environment();

    /// Register an inmutable variable with a specific type.
    void registerVariable(const std::string& name, ElementaryType type);

    /// Register an external function.
    void registerFunction(const std::string& name, const std::vector<ElementaryType>& parameterTypes, ElementaryType returnType);

    /// Parse the stream until eof and return the corresponding AST tree.
    /// If skipTypeChecking is true, no typechecking will be performed and no variables or functions have to be defined in advance.
    /// This is useful, as no returnType() will be specified and further exploration can be done at later stages.
    /// If an error was detected, a nullptr will be returned instead.
    Ptr<Closure> parse(std::istream& stream);

    /// Parse the given string and return the corresponding AST tree.
    /// If skipTypeChecking is true, no typechecking will be performed and no variables or functions have to be defined in advance.
    /// This is useful, as no returnType() will be specified and further exploration can be done at later stages.
    /// If an error was detected, a nullptr will be returned instead.
    Ptr<Closure> parse(std::string_view str);

    /// Together will the mandatory visitor the given AST will be transpiled.
    /// The template payload has to be defined by the user.
    template <typename Payload>
    inline Payload transpile(const Ptr<Closure>& closure, TranspileVisitor<Payload>* visitor) const
    {
        internal::Transpiler<Payload> transpiler(mGlobals, visitor);
        return transpiler.handle(closure);
    }

private:
    /// If no error was found, true will be returned, false otherwise.
    bool doTypeChecking(const Ptr<Closure>& closure);

    internal::SymbolTable mGlobals;
    Reporter mReporter;
};
} // namespace PExpr
