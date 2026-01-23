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
    [[nodiscard]] bool tryApplyIdentity(SSAContext* ctx, SSAInstrAssign* asg, InstructionList& instructions, size_t currentIndex);

    // Priority 1 identities (simple, always beneficial)
    [[nodiscard]] std::optional<SSAValue> matchPythagoreanIdentity(const SSAInstrAssign* asg);                       // sin(a)^2 + cos(a)^2 = 1
    [[nodiscard]] std::optional<std::pair<SSAValue, SSAValue>> matchSquareToPoweIdentity(const SSAInstrAssign* asg); // a*a = a^2
    [[nodiscard]] std::optional<SSAValue> matchInverseTrigoIdentity(const SSAInstrAssign* asg);                      // sin(asin(a)) = a, etc.

    // Priority 2 identities (more complex, may or may not be beneficial)
    [[nodiscard]] std::optional<SSAValue> matchAngleAdditionIdentity(const SSAInstrAssign* asg);                       // sin(a)*cos(b) +/- cos(a)*sin(b)
    [[nodiscard]] std::optional<SSAValue> matchDoubleAngleIdentity(const SSAInstrAssign* asg);                         // 2*sin(a)*cos(a) = sin(2*a)
    [[nodiscard]] std::optional<std::pair<SSAValue, SSAValue>> matchPowerReductionIdentity(const SSAInstrAssign* asg); // (1-cos(2*a))/2 = sin(a)^2

    /// Helper to check if an assignment is a function call
    [[nodiscard]] bool isCallToIntrinsic(const SSAValue& val, std::string_view funcName) const;

    /// Helper to check if two values are the same (same name or both constant with same value)
    [[nodiscard]] bool isSameValue(const SSAValue& a, const SSAValue& b) const;

    /// Helper to find the assignment that defines a given value
    [[nodiscard]] const SSAInstrAssign* findDefinition(const std::string& name) const;

    /// Helper to check if a value is a binary operation with specific operator
    [[nodiscard]] bool isBinaryOp(const SSAValue& val, BinaryOperation op, SSAValue& left, SSAValue& right) const;

    /// Helper to check if a value is a power operation
    [[nodiscard]] bool isPowerOp(const SSAValue& val, SSAValue& base, SSAValue& exponent) const;

    /// Helper to check if a value is a constant number
    [[nodiscard]] bool isConstantNumber(const SSAValue& val, Number& outValue) const;

    /// Helper to create a new temporary assignment
    [[nodiscard]] SSAValue createTempAssignment(SSAContext* ctx,
                                                InstructionList& instructions, size_t insertPos,
                                                SSAInstrAssign::OpKind opKind,
                                                const std::vector<SSAValue>& operands,
                                                ElementaryType type);

    /// Map from variable name to its defining assignment
    std::unordered_map<std::string, const SSAInstrAssign*> mDefinitions;

    const SSAOptions mOptions;
};

} // namespace PExpr::ssa
