#include "RVMOptimizer.h"
#include "RVMConstantOptimizer.h"
#include "RVMRegisterAllocator.h"

namespace PExpr::rvm {

bool RVMOptimizer::optimize(const opt::OptimizerOptions& options, RVMProgram& program)
{
    bool changedAtAll = false;

    // Apply constant propagation (may create identity MOVs or simplify code)
    while (true) {
        bool changed = RVMConstantOptimizer::optimize(options, program);
        changedAtAll |= changed;

        if (!changed)
            break;
    }

    // Apply register allocation with integrated MOV optimization
    if (options.EnableRegisterAllocation) {
        while (true) {
            auto result = RVMRegisterAllocator::allocate(program);
            changedAtAll |= result.Changed;

            if (!result.Changed)
                break;
        }
    }

    return changedAtAll;
}
} // namespace PExpr::rvm
