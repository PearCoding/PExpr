#pragma once

#include "RVMStructs.h"

namespace PExpr::rvm {

/// RVMValidator validates a RVMProgram
class RVMValidator {
public:
    /// Check if all values are elementary
    [[nodiscard]] static bool checkIfElementary(const RVMProgram& program);
    [[nodiscard]] static bool checkIfElementary(const RVMInstr& instr);
    [[nodiscard]] static bool checkIfElementary(const RVMValue& value);
};

} // namespace PExpr::rvm
