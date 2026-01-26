#pragma once

#include "SSAInstruction.h"

#include <memory>

namespace PExpr::ssa {
struct SSAFunction {
    std::string Name;
    std::vector<std::string> Parameters;
    std::vector<std::shared_ptr<SSAInstr>> Body;
    Type ReturnType = Type(TypeKind::Unspecified);

    // Mark whether this function is external (declared but not defined).
    bool External = false;
    // Mark whether this function has side-effects. Only external functions can have side-effects
    bool HasSideEffect = false;
};

struct SSAProgram {
    std::vector<std::shared_ptr<SSAInstr>> Body;
    std::vector<SSAFunction> Functions;
};
} // namespace PExpr::ssa