#pragma once

#include "SSAMapper.h"

namespace PExpr::ssa {
struct SSAOptions {
    bool EnableConstantFolding         = true;
    bool EnableConstantFoldingNumber   = true;
    bool RemoveDeadCode                = true;
    bool InlineFunctions               = true;
    bool ApplyMathIdentities           = true; // < Standard math identities
    bool ApplyTrigonometricIdentities  = true; // < Trigonometric identities (sin, cos, ...)
    bool EliminateCommonSubexpressions = true; // < Common subexpression elimination (CSE)
    bool EliminatePartialRedundancies  = true; // < Partial redundancy elimination (PRE)

    [[nodiscard]] inline static SSAOptions None()
    {
        return SSAOptions{
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

    [[nodiscard]] inline static SSAOptions Low()
    {
        auto opts                          = None();
        opts.EnableConstantFolding         = true;
        opts.RemoveDeadCode                = true;
        opts.EliminateCommonSubexpressions = true;
        return opts;
    }

    [[nodiscard]] inline static SSAOptions Medium()
    {
        auto opts                         = Low();
        opts.InlineFunctions              = true;
        opts.EliminatePartialRedundancies = true;
        return opts;
    }

    [[nodiscard]] inline static SSAOptions High()
    {
        auto opts                         = Medium();
        opts.EnableConstantFoldingNumber  = true;
        opts.ApplyMathIdentities          = true;
        opts.ApplyTrigonometricIdentities = true;
        return opts;
    }
};
} // namespace PExpr::ssa