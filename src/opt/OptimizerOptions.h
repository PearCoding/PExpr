#pragma once

#include "PExpr.h"

namespace PExpr::opt {
struct OptimizerOptions {
    bool EnableConstantFolding         = false;
    bool EnableConstantFoldingNumber   = false;
    bool RemoveDeadCode                = false;
    bool InlineFunctions               = false;
    bool ForceInlineFunctions          = false; // < Force inline all internal functions, eliminating all functions from IR
    bool ApplyMathIdentities           = false; // < Standard math identities
    bool ApplyTrigonometricIdentities  = false; // < Trigonometric identities (sin, cos, ...)
    bool EliminateCommonSubexpressions = false; // < Common subexpression elimination (CSE)
    bool EliminatePartialRedundancies  = false; // < Partial redundancy elimination (PRE)
    bool OptimizeTailCalls             = false; // < Tail call optimization

    /// No option is enabled. Using --no-optimization
    [[nodiscard]] inline static OptimizerOptions None()
    {
        return OptimizerOptions{};
    }

    /// The absolute minimum is enabled. This is the default without flags or -O0.
    [[nodiscard]] inline static OptimizerOptions Minimum()
    {
        auto opts                          = None();
        opts.RemoveDeadCode                = true;
        return opts;
    }

    /// Some easy optimizations. This is -O1.
    [[nodiscard]] inline static OptimizerOptions Low()
    {
        auto opts                          = Minimum();
        opts.EnableConstantFolding         = true;
        opts.RemoveDeadCode                = true;
        opts.EliminateCommonSubexpressions = true;
        return opts;
    }

    /// Some medium optimizations which might increase the code size. This is -O2.
    [[nodiscard]] inline static OptimizerOptions Medium()
    {
        auto opts                         = Low();
        opts.InlineFunctions              = true;
        opts.EliminatePartialRedundancies = true;
        opts.OptimizeTailCalls            = true;
        return opts;
    }

    /// Some large optimizations. Ignores IEEE-754 compiliance. 
    /// This is -O3 (and resembles --fast-math in some other compilers)
    [[nodiscard]] inline static OptimizerOptions High()
    {
        auto opts                         = Medium();
        opts.EnableConstantFoldingNumber  = true;
        opts.ApplyMathIdentities          = true;
        opts.ApplyTrigonometricIdentities = true;
        return opts;
    }
};
} // namespace PExpr::opt