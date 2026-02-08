#include "RVMOptimizer.h"
#include "RVMMoveSimplifier.h"
#include "RVMRedundantMoveEliminator.h"

namespace PExpr::rvm {

bool RVMOptimizer::optimize(const opt::OptimizerOptions& options, RVMProgram& program)
{
    bool changedAtAll = false;

    // Repeat until no changes are made
    while (true) {
        // Apply move-related optimizations
        bool changed = false;

        // Apply identity MOV elimination (simplifies MOV chains)
        if (options.OptimizeMoveChains)
            changed |= RVMMoveSimplifier::simplify(program);

        // Apply redundant MOV elimination (removes MOVs overwritten before being read)
        if (options.OptimizeRedundantMoves)
            changed |= RVMRedundantMoveEliminator::eliminate(program);

        changedAtAll |= changed;
        if (!changed)
            break;
    }

    return changedAtAll;
}
} // namespace PExpr::rvm