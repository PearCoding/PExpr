#pragma once

#include "Expression.h"

namespace PExpr {
class Environment {
public:
    Environment();
    ~Environment();

    void registerVariable(const std::string& name, ElementaryType type);
    Ptr<Expression> parse(std::istream& stream, bool skipTypeChecking = false) const;

private:
    std::unique_ptr<class EnvironmentPrivate> mInternal;
};
} // namespace PExpr
