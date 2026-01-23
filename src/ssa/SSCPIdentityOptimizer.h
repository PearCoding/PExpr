#pragma once

#include "SSAMapper.h"
#include "SSAOptions.h"

#include <memory>
#include <unordered_map>

namespace PExpr::ssa {
class SSAContext;

/// Identity optimizer applies mathematical identities to SSA instructions
/// to simplify expressions. Unlike constant folding, this works on any values,
/// not just constants.
class SSCPIdentityOptimizer {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    inline explicit SSCPIdentityOptimizer(const SSAOptions& opts)
        : mOptions(opts)
    {
    }

    /// Apply identity optimizations to the instruction list
    /// Returns true if any changes were made
    [[nodiscard]] bool applyIdentities(SSAContext* ctx, InstructionList& instructions);

private:
    /// Pattern matching for specific identities
    [[nodiscard]] bool tryApplyIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex);

    // Priority 1 identities (simple, always beneficial)
    [[nodiscard]] bool matchPythagoreanIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex);          // sin(a)^2 + cos(a)^2 = 1
    [[nodiscard]] bool matchSquareToPowerIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex);        // a*a = a^2
    [[nodiscard]] bool matchInverseTrigonometricIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex); // sin(asin(a)) = a, etc.

    // Priority 2 identities (more complex, may or may not be beneficial)
    [[nodiscard]] bool matchAngleAdditionIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex);  // sin(a)*cos(b) +/- cos(a)*sin(b)
    [[nodiscard]] bool matchDoubleAngleIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex);    // 2*sin(a)*cos(a) = sin(2*a)
    [[nodiscard]] bool matchPowerReductionIdentity(SSAContext* ctx, InstructionList& instructions, size_t currentIndex); // (1-cos(2*a))/2 = sin(a)^2

    /// Helper to check if an assignment is a function call
    [[nodiscard]] bool isCallToIntrinsic(const SSAValue& val, std::string_view funcName) const;

    /// Helper to check if callback is an intrinsic
    [[nodiscard]] bool isIntrinsic(const SSAInstrCall* call, std::string_view funcName) const;

    /// Helper to check if two values are the same (same name or both constant with same value)
    [[nodiscard]] bool isSameValue(const SSAValue& a, const SSAValue& b) const;

    /// Helper to find the assignment that defines a given value
    [[nodiscard]] const SSAInstr* findDefinition(const std::string& name) const;

    /// Helper to check if a value is a binary operation with specific operator
    [[nodiscard]] bool isBinaryOp(const SSAValue& val, BinaryOperation op, SSAValue& left, SSAValue& right) const;

    /// Helper to check if a value is a power operation
    [[nodiscard]] bool isPowerOp(const SSAValue& val, SSAValue& base, SSAValue& exponent) const;

    /// Helper to check if a value is a constant number
    [[nodiscard]] bool isConstantNumber(const SSAValue& val, Number& outValue) const;

    /// Map from variable name to its defining assignment
    std::unordered_map<std::string, const SSAInstr*> mDefinitions;

    const SSAOptions mOptions;
};

} // namespace PExpr::ssa
