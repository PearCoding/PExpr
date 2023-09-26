#pragma once

#include "Lookup.h"
#include "Closure.h"
#include "internal/Transpiler.h"

namespace PExpr {
/// Main class for parsing and transpiling.
class Environment {
public:
    /// Creates an empty environment.
    Environment();
    /// Destroys an environment.
    ~Environment();

    /// Register a variable lookup function.
    /// Callback has to return a valid variable definition if variable exists.
    void registerVariableLookupFunction(const VariableLookupFunction& def);

    /// Register a function lookup function.
    /// Callback has to return a valid function definition if function exists with exact or convertible signature.
    void registerFunctionLookupFunction(const FunctionLookupFunction& def);

    /// Parse the stream until eof and return the corresponding AST tree.
    /// If skipTypeChecking is true, no typechecking will be performed and no variables or functions have to be defined in advance.
    /// This is useful, as no returnType() will be specified and further exploration can be done at later stages.
    /// If an error was detected, a nullptr will be returned instead.
    Ptr<Closure> parse(std::istream& stream, bool skipTypeChecking = false) const;

    /// Parse the given string and return the corresponding AST tree.
    /// If skipTypeChecking is true, no typechecking will be performed and no variables or functions have to be defined in advance.
    /// This is useful, as no returnType() will be specified and further exploration can be done at later stages.
    /// If an error was detected, a nullptr will be returned instead.
    Ptr<Closure> parse(const std::string& str, bool skipTypeChecking = false) const;

    /// A late type checking.
    /// If no error was found, true will be returned, false otherwise.
    bool doTypeChecking(const Ptr<Closure>& closure) const;

    /// Together will the mandatory visitor the given AST will be transpiled.
    /// The template payload has to be defined by the user.
    template <typename Payload>
    inline Payload transpile(const Ptr<Closure>& closure, TranspileVisitor<Payload>* visitor) const
    {
        internal::Transpiler<Payload> transpiler(mDefinitions, visitor);
        return transpiler.handle(closure);
    }

private:
    internal::DefContainer mDefinitions;
};
} // namespace PExpr
