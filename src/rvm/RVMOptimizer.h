#pragma once

#include "RVMInstruction.h"
#include "RVMProgram.h"
#include "opt/OptimizerOptions.h"

namespace PExpr::rvm {

/// Combined optimizer for RVM (register-based virtual machine) optimizations.
/// Applies various RVM-specific optimizations in a coordinated manner.
class RVMOptimizer {
public:
    /// Apply RVM optimizations to a program based on the given options.
    /// Returns true if any changes were made.
    static bool optimize(const opt::OptimizerOptions& options, RVMProgram& program);
};

} // namespace PExpr::rvm