#pragma once

#include "Definitions.h"
#include "Expression.h"
#include "internal/Transpiler.h"

namespace PExpr {
class Environment {
public:
    Environment();
    ~Environment();

    void registerDef(const VariableDef& def);
    void registerDef(const FunctionDef& def);

    Ptr<Expression> parse(std::istream& stream, bool skipTypeChecking = false) const;

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
