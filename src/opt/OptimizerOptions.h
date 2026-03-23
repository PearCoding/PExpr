#pragma once

#include "PExpr.h"

namespace PExpr::opt {
struct OptimizerOptions {
    bool EnableConstantFolding         = false;
    bool EnableConstantFoldingNumber   = false;
    bool RemoveDeadCode                = false;
    bool InlineFunctions               = false;
    bool ForceInlineFunctions          = false; // < Force inline all internal functions, eliminating all functions from IR
    bool ApplyMathIdentities           = false; // < IEEE-754-safe math identities (a+0=a, a*1=a, a||false=a, etc.)
    bool ApplyUnsafeMathIdentities     = false; // < Non-IEEE-754-compliant identities (a-a=0, a/a=1, a==a=true — invalid for NaN)
    bool ApplyTrigonometricIdentities  = false; // < Trigonometric identities (sin, cos, ...)
    bool EliminateCommonSubexpressions = false; // < Common subexpression elimination (CSE)
    bool EliminatePartialRedundancies  = false; // < Partial redundancy elimination (PRE)
    bool OptimizeTailCalls             = false; // < Tail call optimization

    bool OptimizeIdentityMoves       = false; // < Eliminate identity MOV instructions (src == dst) in RVM
    bool OptimizeConstantPropagation = false; // < Propagate constants from MOV instructions within basic blocks
    bool EnableRegisterAllocation    = false; // < Enable register allocation to minimize register count

    /// No option is enabled. This is -O0.
    [[nodiscard]] inline static OptimizerOptions None()
    {
        return OptimizerOptions{};
    }

    /// Some easy optimizations. This is -O1.
    [[nodiscard]] inline static OptimizerOptions Low()
    {
        auto opts                          = None();
        opts.RemoveDeadCode                = true;
        opts.EnableConstantFolding         = true;
        opts.EliminateCommonSubexpressions = true;

        // Enable all RVM optimizations
        opts.OptimizeIdentityMoves       = true;
        opts.OptimizeConstantPropagation = true;
        opts.EnableRegisterAllocation    = true;
        return opts;
    }

    /// Some medium optimizations which might increase the code size. This is -O2.
    [[nodiscard]] inline static OptimizerOptions Medium()
    {
        auto opts                         = Low();
        opts.InlineFunctions              = true;
        opts.EliminatePartialRedundancies = true;
        opts.OptimizeTailCalls            = true;
        opts.ApplyMathIdentities          = true;
        return opts;
    }

    /// Some large optimizations. Ignores IEEE-754 compliance.
    /// This is -O3 (and resembles --fast-math in some other compilers)
    [[nodiscard]] inline static OptimizerOptions High()
    {
        auto opts                         = Medium();
        opts.EnableConstantFoldingNumber  = true;
        opts.ApplyUnsafeMathIdentities    = true;
        opts.ApplyTrigonometricIdentities = true;
        return opts;
    }
};
} // namespace PExpr::opt