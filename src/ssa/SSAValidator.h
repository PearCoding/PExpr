#pragma once

#include "SSAMapper.h"

namespace PExpr::ssa {

/// SSAValidator validates a SSAProgram or SSAFunction
class SSAValidator {
public:
    /// Check if typed
    [[nodiscard]] static bool checkIfTyped(const SSAProgram* program);
    [[nodiscard]] static bool checkIfTyped(const SSAFunction* func);
    [[nodiscard]] static bool checkIfTyped(const SSAInstr* instr);
    [[nodiscard]] static bool checkIfTyped(const SSAValue& value);
};

} // namespace PExpr::ssa
