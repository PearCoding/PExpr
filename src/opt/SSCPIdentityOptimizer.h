#pragma once

#include "OptimizerOptions.h"
#include "ast/Enums.h"

#include <memory>
#include <unordered_map>

namespace PExpr::ssa {
class SSAContext;
class SSAInstr;
class SSAInstrCall;
class SSAValue;
} // namespace PExpr::ssa

namespace PExpr::opt {
/// Identity optimizer applies mathematical identities to SSA instructions
/// to simplify expressions. Unlike constant folding, this works on any values,
/// not just constants.
class SSCPIdentityOptimizer {
public:
    using InstructionList = std::vector<std::shared_ptr<ssa::SSAInstr>>;

    inline explicit SSCPIdentityOptimizer(const OptimizerOptions& opts)
        : mOptions(opts)
    {
    }

    /// Apply identity optimizations to the instruction list
    /// Returns true if any changes were made
    [[nodiscard]] bool applyIdentities(ssa::SSAContext* ctx, InstructionList& instructions);

private:
    /// Pattern matching for specific identities
    [[nodiscard]] bool tryApplyIdentity(ssa::SSAContext* ctx, InstructionList& instructions, size_t currentIndex);

    [[nodiscard]] bool matchBasicMathIdentities(ssa::SSAContext* ctx, InstructionList& instructions, size_t currentIndex);    // a + 0 = a, a - 0 = a, a * 1 = a, a / 1 = a, 0 * a = 0, 1 * a = a
    [[nodiscard]] bool matchUnaryIdentity(ssa::SSAContext* ctx, InstructionList& instructions, size_t currentIndex);         // -(-a) = a, +a = a, !!a = a
    [[nodiscard]] bool matchPowerToSquareIdentity(ssa::SSAContext* ctx, InstructionList& instructions, size_t currentIndex); // a^2 = a*a

    [[nodiscard]] bool matchPythagoreanIdentity(ssa::SSAContext* ctx, InstructionList& instructions, size_t currentIndex);          // sin(a)^2 + cos(a)^2 = 1
    [[nodiscard]] bool matchInverseTrigonometricIdentity(ssa::SSAContext* ctx, InstructionList& instructions, size_t currentIndex); // sin(asin(a)) = a, etc.

    [[nodiscard]] bool matchAngleAdditionIdentity(ssa::SSAContext* ctx, InstructionList& instructions, size_t currentIndex);  // sin(a)*cos(b) +/- cos(a)*sin(b)
    [[nodiscard]] bool matchDoubleAngleIdentity(ssa::SSAContext* ctx, InstructionList& instructions, size_t currentIndex);    // 2*sin(a)*cos(a) = sin(2*a)
    [[nodiscard]] bool matchPowerReductionIdentity(ssa::SSAContext* ctx, InstructionList& instructions, size_t currentIndex); // (1-cos(2*a))/2 = sin(a)^2

    /// Helper to check if an assignment is a function call
    [[nodiscard]] bool isCallToIntrinsic(const ssa::SSAValue& val, std::string_view funcName) const;

    /// Helper to check if callback is an intrinsic
    [[nodiscard]] bool isIntrinsic(const ssa::SSAInstrCall* call, std::string_view funcName) const;

    /// Helper to check if two values are the same (same name or both constant with same value)
    [[nodiscard]] bool isSameValue(const ssa::SSAValue& a, const ssa::SSAValue& b) const;

    /// Helper to find the assignment that defines a given value
    [[nodiscard]] const ssa::SSAInstr* findDefinition(const std::string& name) const;

    /// Helper to check if a value is a binary operation with specific operator
    [[nodiscard]] bool isBinaryOp(const ssa::SSAValue& val, ast::BinaryOperation op, ssa::SSAValue& left, ssa::SSAValue& right) const;

    /// Helper to check if a value is a power operation
    [[nodiscard]] bool isPowerOp(const ssa::SSAValue& val, ssa::SSAValue& base, ssa::SSAValue& exponent) const;

    /// Helper to check if a value is a constant number
    [[nodiscard]] bool isConstantNumber(const ssa::SSAValue& val, Number& outValue) const;

    [[nodiscard]] const ssa::SSAInstr* getDefinition(const ssa::SSAValue& val) const;

    /// Map from variable name to its defining assignment
    std::unordered_map<std::string, const ssa::SSAInstr*> mDefinitions;

    const OptimizerOptions mOptions;
};

} // namespace PExpr::opt
