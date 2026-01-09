#pragma once

#include "Enums.h"

#include <string>
#include <vector>

namespace PExpr {

/// A simple parameter representation used by function declarations and definitions
struct Parameter {
    std::string Name;
    ElementaryType Type;
};

using ParameterList = std::vector<Parameter>;

} // namespace PExpr
