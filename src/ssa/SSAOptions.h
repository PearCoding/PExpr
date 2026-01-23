#pragma once

#include "SSAMapper.h"

namespace PExpr::ssa {
struct SSAOptions {
    bool EnableConstantFolding       = true;
    bool EnableConstantFoldingNumber = true;
    bool RemoveDeadCode              = true;
    bool InlineFunctions             = true;
    bool ApplyMathIdentities         = true;
};
} // namespace PExpr::ssa