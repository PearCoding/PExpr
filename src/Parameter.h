#pragma once

#include "Type.h"

#include <string>
#include <vector>

namespace PExpr {

/// A simple parameter representation used by function declarations and definitions
struct Parameter {
    std::string Name;
    Type Type;
};

using ParameterList = std::vector<Parameter>;

} // namespace PExpr
