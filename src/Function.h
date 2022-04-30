#pragma once

#include "Enums.h"
#include <vector>

namespace PExpr {
using FunctionTypeChecker = ElementaryType (*)(const std::vector<ElementaryType>&);
} // namespace PExpr
