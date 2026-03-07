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
    bool DissolveTuples                = false; // < Dissolve tuples to elementary types (except call and returns). Not recommended, but necessary for RVM

    bool OptimizeMoveChains       = false; // < Optimize move chains in RVM
    bool OptimizeRedundantMoves   = false; // < Eliminate redundant MOV instructions in RVM
    bool OptimizeIdentityMoves    = false; // < Eliminate identity MOV instructions (src == dst) in RVM
    bool EnableRegisterAllocation = false; // < Enable register allocation to minimize register count

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

        opts.OptimizeMoveChains       = true;
        opts.OptimizeRedundantMoves   = true;
        opts.OptimizeIdentityMoves    = true;
        opts.EnableRegisterAllocation = true;
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

    /// Some large optimizations. Ignores IEEE-754 compliance.
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