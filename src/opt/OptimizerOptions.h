#pragma once

#include "PExpr.h"

namespace PExpr::opt {
struct OptimizerOptions {
    bool EnableConstantFolding         = true;
    bool EnableConstantFoldingNumber   = true;
    bool RemoveDeadCode                = true;
    bool InlineFunctions               = true;
    bool ApplyMathIdentities           = true; // < Standard math identities
    bool ApplyTrigonometricIdentities  = true; // < Trigonometric identities (sin, cos, ...)
    bool EliminateCommonSubexpressions = true; // < Common subexpression elimination (CSE)
    bool EliminatePartialRedundancies  = true; // < Partial redundancy elimination (PRE)

    [[nodiscard]] inline static OptimizerOptions None()
    {
        return OptimizerOptions{
            .EnableConstantFolding         = false,
            .EnableConstantFoldingNumber   = false,
            .RemoveDeadCode                = false,
            .InlineFunctions               = false,
            .ApplyMathIdentities           = false,
            .ApplyTrigonometricIdentities  = false,
            .EliminateCommonSubexpressions = false,
            .EliminatePartialRedundancies  = false,
        };
    }

    [[nodiscard]] inline static OptimizerOptions Low()
    {
        auto opts                          = None();
        opts.EnableConstantFolding         = true;
        opts.RemoveDeadCode                = true;
        opts.EliminateCommonSubexpressions = true;
        return opts;
    }

    [[nodiscard]] inline static OptimizerOptions Medium()
    {
        auto opts                         = Low();
        opts.InlineFunctions              = true;
        opts.EliminatePartialRedundancies = true;
        return opts;
    }

    [[nodiscard]] inline static OptimizerOptions High()
    {
        auto opts                         = Medium();
        opts.EnableConstantFoldingNumber  = true;
        opts.ApplyMathIdentities          = true;
        opts.ApplyTrigonometricIdentities = true;
        return opts;
    }
};
} // namespace PExpr::ssa