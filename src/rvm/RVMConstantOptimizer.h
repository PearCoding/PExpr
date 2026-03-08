#pragma once

#include "RVMBasicBlockAnalyzer.h"
#include "RVMInstruction.h"
#include "RVMProgram.h"
#include "opt/OptimizerOptions.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace PExpr::rvm {

/// Constant propagation optimizer for RVM IR.
/// Propagates constants from MOV instructions within basic blocks.
/// For example:
///   "mov %r4:int 42:int
///    add %r1:int %r4:int %r2:int"
/// becomes:
///   "add %r1:int 42:int %r2:int"
/// The MOV instruction becomes redundant and will be removed by
/// the redundant move elimination pass.
class RVMConstantOptimizer {
public:
    /// Apply constant propagation to an RVM program based on options.
    /// Returns true if any changes were made.
    [[nodiscard]] static bool optimize(const opt::OptimizerOptions& options, RVMProgram& program);

private:
    /// Run constant propagation pass within basic blocks
    [[nodiscard]] static bool runConstantPropagationPass(RVMProgram& program);
};

} // namespace PExpr::rvm