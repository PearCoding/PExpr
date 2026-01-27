#pragma once

#include "OptimizerOptions.h"
#include "ssa/SSAInstruction.h"
#include "ssa/BasicBlockAnalyzer.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PExpr::opt {

/// Partial Redundancy Elimination (PRE) optimizer for SSA IR.
/// Moves computations to dominate all uses, eliminating partial redundancies
/// (computations that are redundant on some but not all paths).
class SSCPPreOptimizer {
public:
    using InstructionList = std::vector<std::shared_ptr<ssa::SSAInstr>>;

    inline explicit SSCPPreOptimizer(const OptimizerOptions& opts)
        : mOptions(opts)
    {
    }

    /// Apply partial redundancy elimination to the instruction list
    /// Returns true if any changes were made
    [[nodiscard]] bool applyPRE(ssa::SSAContext* ctx, InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions);

private:
    /// Expression information for redundancy analysis
    struct ExpressionInfo {
        size_t expressionId;                           // Unique ID for this expression
        std::unordered_set<size_t> availableAtEntry;   // Blocks where expression is available at entry
        std::unordered_set<size_t> availableAtExit;    // Blocks where expression is available at exit
        std::unordered_set<size_t> anticipatedAtEntry; // Blocks where expression is anticipated at entry
        std::unordered_set<size_t> anticipatedAtExit;  // Blocks where expression is anticipated at exit
        std::unordered_set<size_t> earliest;           // Earliest placement
        std::unordered_set<size_t> latest;             // Latest placement
        std::unordered_set<size_t> insert;             // Insertion points
        std::unordered_set<size_t> delete_;            // Deletion points

        // Expression value for this computation
        ssa::SSAValue value;
    };

    /// Identify redundant expressions
    void identifyExpressions(const InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions);

    /// Check if expression is available at block entry
    void computeAvailability();

    /// Check if expression is anticipated at block entry
    void computeAnticipability();

    /// Compute earliest placement
    void computeEarliestPlacement();

    /// Compute latest placement
    void computeLatestPlacement();

    /// Compute insertion and deletion points
    void computeInsertionDeletionPoints();

    /// Apply code motion based on analysis
    bool applyCodeMotion(ssa::SSAContext* ctx, InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions);

    /// Check if two instructions compute the same expression
    [[nodiscard]] bool areExpressionsEquivalent(const ssa::SSAInstr* a, const ssa::SSAInstr* b) const;

    /// Get expression ID for an instruction
    [[nodiscard]] size_t getExpressionId(const ssa::SSAInstr* instr) const;

    /// Check if instruction is safe to move (no side effects, etc.)
    [[nodiscard]] bool isSafeToMove(const ssa::SSAInstr* instr, const std::unordered_set<std::string>& sideEffectedFunctions) const;

    /// Check if instruction dominates all its uses
    [[nodiscard]] bool dominatesAllUses(size_t blockIdx, const ssa::SSAInstr* instr) const;

    // Expression information map
    std::unordered_map<size_t, ExpressionInfo> mExpressionInfo;

    // Map from instruction to expression ID
    std::unordered_map<const ssa::SSAInstr*, size_t> mInstructionToExpression;

    ssa::BasicBlockAnalyzer mBlockAnalyzer;
    const OptimizerOptions mOptions;
};

} // namespace PExpr::opt
