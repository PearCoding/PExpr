#pragma once

#include "Type.h"

#include <string>
#include <vector>

namespace PExpr::type {

/// A simple parameter representation used by function declarations and definitions
struct Parameter {
    std::string Name;
    Type ParamType;
    bool IsMutable;
};

using ParameterList = std::vector<Parameter>;

} // namespace PExpr::type
