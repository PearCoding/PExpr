#pragma once

#include "OptimizerOptions.h"
#include "ssa/BasicBlockAnalyzer.h"

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PExpr::ssa {
class SSAContext;
class SSAInstr;
class SSAValue;
} // namespace PExpr::ssa

namespace PExpr::opt {

/// Partial Redundancy Elimination (PRE) optimizer.
/// Eliminates computations that are redundant on some or all control-flow paths by inserting
/// computations on missing paths and replacing the originals with the pre-computed value.
///
/// Handles two cases:
/// - Global CSE: expression available on ALL paths to a block (fully redundant cross-block)
/// - Partial redundancy: expression available on SOME paths; insert on missing paths
class SSCPPreOptimizer {
public:
    using InstructionList = std::vector<std::shared_ptr<ssa::SSAInstr>>;

    inline explicit SSCPPreOptimizer(const OptimizerOptions& opts)
        : mOptions(opts)
    {
    }

    /// Apply PRE to the instruction list. Returns true if any changes were made.
    [[nodiscard]] bool applyPRE(ssa::SSAContext* ctx, InstructionList& instructions,
                                const std::unordered_set<std::string>& sideEffectedFunctions);

private:
    /// Expression fingerprint for identifying equivalent expressions across blocks
    struct ExpressionHash {
        struct Hash {
            size_t operator()(const ExpressionHash& h) const { return h.hash; }
        };

        size_t hash;
        type::Type type;

        inline bool operator==(const ExpressionHash& other) const { return hash == other.hash && type == other.type; }
    };

    /// Per-expression analysis data
    struct ExpressionInfo {
        ExpressionHash hash;
        std::shared_ptr<ssa::SSAInstr> exemplar; // Representative instruction

        // Per-block local properties
        std::vector<bool> ueExpr;   // Upward exposed: expression computed before any operand kill
        std::vector<bool> deExpr;   // Downward exposed: expression computed and not killed after
        std::vector<bool> exprKill; // Expression killed: an operand is redefined

        // Dataflow results (indexed by block)
        std::vector<bool> antIn;    // Anticipated at block entry
        std::vector<bool> antOut;   // Anticipated at block exit
        std::vector<bool> availIn;  // Available at block entry
        std::vector<bool> availOut; // Available at block exit
    };

    /// Hash an SSA instruction for PRE (excludes target name)
    [[nodiscard]] std::optional<ExpressionHash> hashInstruction(const ssa::SSAInstr* instr) const;

    /// Collect the set of SSA names that an expression's operands reference
    void collectOperandNames(const ssa::SSAInstr* instr, std::unordered_set<std::string>& names) const;

    /// Phase 1: Identify candidate expressions and build per-block gen/kill sets
    void identifyExpressions(const InstructionList& instructions,
                             const std::vector<ssa::BasicBlock>& blocks,
                             const std::unordered_set<std::string>& sideEffectedFunctions);

    /// Phase 2: Backward dataflow — anticipated expressions
    void computeAnticipated(const std::vector<ssa::BasicBlock>& blocks);

    /// Phase 3: Forward dataflow — available expressions
    void computeAvailable(const std::vector<ssa::BasicBlock>& blocks);

    /// Phase 4: Identify global CSE / partial redundancy opportunities and apply
    [[nodiscard]] bool applyTransformations(ssa::SSAContext* ctx, InstructionList& instructions,
                                            const std::vector<ssa::BasicBlock>& blocks);

    /// Clone the exemplar instruction with a new target
    [[nodiscard]] std::shared_ptr<ssa::SSAInstr> cloneExemplar(const ExpressionInfo& expr,
                                                               const ssa::SSAValue& target) const;

    /// Insert "preTarget = originalTarget" after the last computation of expr in blockIdx
    void addCopyAfterComputation(const ExpressionInfo& expr, size_t blockIdx,
                                 const ssa::SSAValue& preTarget,
                                 const InstructionList& instructions,
                                 const std::vector<ssa::BasicBlock>& blocks,
                                 std::vector<std::pair<size_t, std::shared_ptr<ssa::SSAInstr>>>& insertions) const;

    /// Replace the first computation of expr in blockIdx with "originalTarget = preTarget"
    void replaceComputation(const ExpressionInfo& expr, size_t blockIdx,
                            const ssa::SSAValue& preTarget,
                            InstructionList& instructions,
                            const std::vector<ssa::BasicBlock>& blocks) const;

    // Expression analysis data
    std::vector<ExpressionInfo> mExpressions;
    std::unordered_map<ExpressionHash, size_t, ExpressionHash::Hash> mHashToExprIndex;

    ssa::BasicBlockAnalyzer mBlockAnalyzer;
    const OptimizerOptions mOptions;
};

} // namespace PExpr::opt
