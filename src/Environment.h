#pragma once

#include "Definitions.h"
#include "Expression.h"
#include "internal/Transpiler.h"

namespace PExpr {
/// Main class for parsing and transpiling.
class Environment {
public:
    /// Creates an empty environment.
    Environment();
    /// Destroys an environment.
    ~Environment();

    /// Register a variable.
    /// If a variable of the same name is already present, it will be silently replaced.
    void registerDef(const VariableDef& def);
    /// Register a function.
    /// Multiple function defs with the same name are allowed, but the signature (parameter types) have to be unique.
    void registerDef(const FunctionDef& def);

    /// Parse the stream until eof and return the corresponding AST tree.
    /// If skipTypeChecking is true, no typechecking will be performed and no variables or functions have to be defined in advance.
    /// This is useful, as no returnType() will be specified and further exploration can be done at later stages.
    /// If an error was detected, a nullptr will be returned instead.
    Ptr<Expression> parse(std::istream& stream, bool skipTypeChecking = false) const;

    /// Parse the given string and return the corresponding AST tree.
    /// If skipTypeChecking is true, no typechecking will be performed and no variables or functions have to be defined in advance.
    /// This is useful, as no returnType() will be specified and further exploration can be done at later stages.
    /// If an error was detected, a nullptr will be returned instead.
    Ptr<Expression> parse(const std::string& str, bool skipTypeChecking = false) const;

    /// A late type checking.
    /// If no error was found, true will be returned, false otherwise.
    bool doTypeChecking(const Ptr<Expression>& expr) const;

    /// Together will the mandatory visitor the given AST will be transpiled.
    /// The template payload has to be defined by the user.
    template <typename Payload>
    inline Payload transpile(const Ptr<Expression>& expr, TranspileVisitor<Payload>* visitor) const
    {
        internal::Transpiler<Payload> transpiler(mDefinitions, visitor);
        return transpiler.handle(expr);
    }

private:
    internal::DefContainer mDefinitions;
};
} // namespace PExpr
