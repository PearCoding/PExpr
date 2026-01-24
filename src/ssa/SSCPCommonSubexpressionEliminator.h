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

private:
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

    /// Generate hash for an assignment instruction
    [[nodiscard]] std::optional<ExpressionHash> hashAssign(const SSAInstrAssign* asg) const;

    /// Generate hash for a call instruction
    [[nodiscard]] std::optional<ExpressionHash> hashCall(const SSAInstrCall* call) const;

    /// Generate hash for a binary operation
    [[nodiscard]] size_t hashBinaryOp(BinaryOperation op, const SSAValue& left, const SSAValue& right) const;

    /// Generate hash for a unary operation
    [[nodiscard]] size_t hashUnaryOp(UnaryOperation op, const SSAValue& operand) const;

    /// Generate hash for a swizzle operation
    [[nodiscard]] size_t hashSwizzle(const SSAValue& operand, const std::string& swizzle) const;

    /// Generate hash for an access operation
    [[nodiscard]] size_t hashAccess(const SSAValue& operand, const SSAValue& index) const;

    /// Generate hash for a vector construction
    [[nodiscard]] size_t hashVector(const std::vector<SSAValue>& operands) const;

    /// Generate hash for a cast operation
    [[nodiscard]] size_t hashCast(const SSAValue& operand, ElementaryType targetType) const;

    /// Generate hash for a phi operation
    [[nodiscard]] size_t hashPhi(const std::vector<SSAValue>& conditions, const std::vector<SSAValue>& branches) const;

    /// Hash a value (either constant or variable reference)
    [[nodiscard]] size_t hashValue(const SSAValue& val) const;

    /// Hash a constant value
    [[nodiscard]] size_t hashConstant(const ExtendedValueVariant& val, ElementaryType type) const;

    /// Hash a string (for function names, swizzles, etc.)
    [[nodiscard]] size_t hashString(const std::string& str) const;

    /// Combine hashes
    [[nodiscard]] inline size_t combineHashes(size_t h1, size_t h2) const { return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2)); }

    /// Check if two instructions are equivalent (same operation and operands)
    [[nodiscard]] bool areInstructionsEquivalent(const SSAInstr* a, const SSAInstr* b) const;

    /// Check if two assignments are equivalent
    [[nodiscard]] bool areAssignsEquivalent(const SSAInstrAssign* a, const SSAInstrAssign* b) const;

    /// Check if two calls are equivalent
    [[nodiscard]] bool areCallsEquivalent(const SSAInstrCall* a, const SSAInstrCall* b) const;

    /// Check if two values are equivalent
    [[nodiscard]] bool areValuesEquivalent(const SSAValue& a, const SSAValue& b) const;

    /// Map from expression hash to the SSA value that computes it
    std::unordered_map<ExpressionHash, SSAValue, ExpressionHash::Hash> mExpressionMap;

    /// Map from variable name to its hash (for fast lookup)
    std::unordered_map<std::string, ExpressionHash> mValueHashes;

    BasicBlockAnalyzer mBlockAnalyzer;
    const SSAOptions mOptions;
};

} // namespace PExpr::ssa