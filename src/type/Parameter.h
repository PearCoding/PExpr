#pragma once

#include "Type.h"

#include <string>
#include <vector>

namespace PExpr::type {

/// A simple parameter representation used by function declarations and definitions
struct Parameter {
    std::string Name;
    Type ParamType;
};

using ParameterList = std::vector<Parameter>;

} // namespace PExpr::type
