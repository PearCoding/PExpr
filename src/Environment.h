#pragma once

#include "Definitions.h"
#include "Expression.h"

namespace PExpr {
class Environment {
public:
    Environment();
    ~Environment();

    void registerDef(const VariableDef& def);
    void registerDef(const FunctionDef& def);

    Ptr<Expression> parse(std::istream& stream, bool skipTypeChecking = false) const;

private:
    std::unique_ptr<class EnvironmentPrivate> mInternal;
};
} // namespace PExpr
