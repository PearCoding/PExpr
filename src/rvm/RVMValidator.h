#pragma once

#include "RVMStructs.h"
#include "type/Type.h"

namespace PExpr::rvm {

/// RVMValidator validates a RVMProgram
class RVMValidator {
public:
    /// Check if all values are elementary
    [[nodiscard]] static bool checkIfElementary(const RVMProgram& program);
    [[nodiscard]] static bool checkIfElementary(const RVMInstr& instr);
    [[nodiscard]] static bool checkIfElementary(const RVMValue& value);

    /// Validate that two programs are semantically equivalent by interpreting them.
    /// This assumes the programs take no arguments and return the same type.
    [[nodiscard]] static bool validateOptimizations(const RVMProgram& original,
                                                  const RVMProgram& optimized,
                                                  const type::Type& returnType);
};

} // namespace PExpr::rvm
