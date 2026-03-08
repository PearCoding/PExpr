#include "RVMOptimizer.h"
#include "RVMConstantOptimizer.h"
#include "RVMMoveOptimizer.h"
#include "RVMRegisterAllocator.h"
#include "RVMRegisterLinearizer.h"

namespace PExpr::rvm {

bool RVMOptimizer::optimize(const opt::OptimizerOptions& options, RVMProgram& program)
{
    bool changedAtAll = false;

    // Apply register linearization first (syntax-only pass)
    // changedAtAll |= RVMRegisterLinearizer::linearize(program);

    // Repeat until no changes are made
    while (true) {
        bool changed = false;

        // Apply constant propagation first (makes MOVs with constants redundant)
        changed |= RVMConstantOptimizer::optimize(options, program);
        
        // Apply combined move optimizations (identity, chain collapsing, redundant)
        changed |= RVMMoveOptimizer::optimize(options, program);

        changedAtAll |= changed;
        if (!changed)
            break;
    }

    // Apply register allocation after optimizations
    if (options.EnableRegisterAllocation)
        changedAtAll |= RVMRegisterAllocator::allocate(program);

    // Repeat until no changes are made
    while (true) {
        bool changed = false;

        // Apply combined move optimizations (identity, chain collapsing, redundant)
        changed |= RVMMoveOptimizer::optimize(options, program);

        changedAtAll |= changed;
        if (!changed)
            break;
    }

    // Apply register linearization at the end (syntax-only pass)
    // changedAtAll |= RVMRegisterLinearizer::linearize(program);

    return changedAtAll;
}
} // namespace PExpr::rvm
