#pragma once

#include "BasicBlockAnalyzer.h"
#include "SSAOptions.h"
#include "SSAStructs.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace PExpr::ssa {

/// Common Subexpression Elimination (CSE) optimizer for SSA IR.
/// Eliminates redundant computations by identifying identical expressions
/// and reusing their results.
/// Operates on basic blocks to respect control flow boundaries.
class SSCPCommonSubexpressionEliminator {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    inline explicit SSCPCommonSubexpressionEliminator(const SSAOptions& opts)
        : mOptions(opts)
    {
    }

    /// Apply common subexpression elimination by processing each basic block separately
    /// Returns true if any changes were made
    [[nodiscard]] bool applyCSE(SSAContext* ctx, InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions);

private:
    /// Apply CSE to a specific subrange of instructions [begin, end)
    [[nodiscard]] bool applyCSEToRange(SSAContext* ctx, InstructionList::iterator begin, InstructionList::iterator end, const std::unordered_set<std::string>& sideEffectedFunctions);

    /// Hash structure for expression fingerprinting
    struct ExpressionHash {
        struct Hash {
            size_t operator()(const ExpressionHash& h) const { return h.hash; }
        };

        size_t hash;
        ElementaryType type;

        inline bool operator==(const ExpressionHash& other) const { return hash == other.hash && type == other.type; }
    };

    /// Generate hash for an SSA instruction
    [[nodiscard]] std::optional<ExpressionHash> hashInstruction(const SSAInstr* instr) const;

    /// Map from expression hash to the SSA value that computes it
    std::unordered_map<ExpressionHash, SSAValue, ExpressionHash::Hash> mExpressionMap;

    /// Map from variable name to its hash (for fast lookup)
    std::unordered_map<std::string, ExpressionHash> mValueHashes;

    BasicBlockAnalyzer mBlockAnalyzer;
    const SSAOptions mOptions;
};

} // namespace PExpr::ssa