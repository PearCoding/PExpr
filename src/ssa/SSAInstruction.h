#pragma once

#include "SSAValue.h"

#include <functional>
#include <memory>
#include <vector>

namespace PExpr::ssa {
struct SSAInstr {
    virtual ~SSAInstr() = default;

    /// Compute a hash for this instruction
    [[nodiscard]] virtual size_t hash(bool includeTargetName = true) const = 0;

    /// Check if two instructions are equivalent
    [[nodiscard]] virtual bool isEquivalent(const SSAInstr* other) const = 0;

    /// Visit all SSAValues contained in this instruction
    /// @param visitor A function that will be called for each SSAValue reference
    inline void forEachValue(const std::function<void(SSAValue&)>& visitor)
    {
        forEachTarget(visitor);
        forEachOperand(visitor);
    };
    inline void forEachValue(const std::function<void(const SSAValue&)>& visitor) const
    {
        forEachTarget(visitor);
        forEachOperand(visitor);
    };

    /// Visit all SSAValues used as operands/arguments contained in this instruction
    /// @param visitor A function that will be called for each SSAValue reference
    virtual void forEachOperand(const std::function<void(SSAValue&)>& visitor) { PEXPR_UNUSED(visitor); };
    virtual void forEachOperand(const std::function<void(const SSAValue&)>& visitor) const { PEXPR_UNUSED(visitor); };

    /// Visit all SSAValues used as targets contained in this instruction
    /// @param visitor A function that will be called for each SSAValue reference
    virtual void forEachTarget(const std::function<void(SSAValue&)>& visitor) { PEXPR_UNUSED(visitor); };
    virtual void forEachTarget(const std::function<void(const SSAValue&)>& visitor) const { PEXPR_UNUSED(visitor); };
};

struct SSAInstrAssign : public SSAInstr {
    enum class OpKind { Assign,
                        Unary,
                        Binary,
                        Swizzle,
                        Access,
                        Vector,
                        Cast,
                        Nop,
                        Phi };

    SSAValue Target;
    OpKind Operator;

    // OpKind specific data
    UnaryOperation UnaryOp   = UnaryOperation::Pos;
    BinaryOperation BinaryOp = BinaryOperation::Add;
    std::string Swizzle;
    std::vector<SSAValue> Operands;

    [[nodiscard]] size_t hash(bool includeTargetName = true) const override;
    [[nodiscard]] bool isEquivalent(const SSAInstr* other) const override;
    void forEachOperand(const std::function<void(SSAValue&)>& visitor) override;
    void forEachOperand(const std::function<void(const SSAValue&)>& visitor) const override;
    void forEachTarget(const std::function<void(SSAValue&)>& visitor) override;
    void forEachTarget(const std::function<void(const SSAValue&)>& visitor) const override;
};

struct SSAInstrCall : public SSAInstr {
    SSAValue Target;
    std::string FunctionName;       ///< Mangled unique name
    std::string PublicFunctionName; ///< User given name
    std::vector<SSAValue> Arguments;

    [[nodiscard]] size_t hash(bool includeTargetName = true) const override;
    [[nodiscard]] bool isEquivalent(const SSAInstr* other) const override;
    void forEachOperand(const std::function<void(SSAValue&)>& visitor) override;
    void forEachOperand(const std::function<void(const SSAValue&)>& visitor) const override;
    void forEachTarget(const std::function<void(SSAValue&)>& visitor) override;
    void forEachTarget(const std::function<void(const SSAValue&)>& visitor) const override;
};

struct SSAInstrReturn : public SSAInstr {
    SSAValue Value;
    [[nodiscard]] size_t hash(bool includeTargetName = true) const override { return Value.hash(includeTargetName); }
    [[nodiscard]] bool isEquivalent(const SSAInstr* other) const override
    {
        if (const auto* otherReturn = dynamic_cast<const SSAInstrReturn*>(other))
            return Value == otherReturn->Value;
        return false;
    }
    void forEachOperand(const std::function<void(SSAValue&)>& visitor) override;
    void forEachOperand(const std::function<void(const SSAValue&)>& visitor) const override;
};

// Label instruction to mark basic blocks in the SSA body. Labels are useful
// for representing control-flow boundaries (e.g., branch entry points) and
// are emitted when inlining branch/closure bodies.
struct SSAInstrLabel : public SSAInstr {
    std::string Name;
    [[nodiscard]] size_t hash(bool includeTargetName = true) const override
    {
        PEXPR_UNUSED(includeTargetName);
        return std::hash<std::string>{}(Name);
    }
    [[nodiscard]] bool isEquivalent(const SSAInstr* other) const override
    {
        if (const auto* otherLabel = dynamic_cast<const SSAInstrLabel*>(other))
            return Name == otherLabel->Name;
        return false;
    }
};

// Conditional branch instruction: if Condition is true jump to TargetLabel.
struct SSAInstrBranch : public SSAInstr {
    SSAValue Condition;
    std::string TargetLabel;
    [[nodiscard]] size_t hash(bool includeTargetName = true) const override
    {
        size_t h = Condition.hash(includeTargetName);
        h        = h * 31 + std::hash<std::string>{}(TargetLabel);
        return h;
    }
    [[nodiscard]] bool isEquivalent(const SSAInstr* other) const override
    {
        if (const auto* otherBranch = dynamic_cast<const SSAInstrBranch*>(other))
            return Condition == otherBranch->Condition && TargetLabel == otherBranch->TargetLabel;
        return false;
    }
    void forEachOperand(const std::function<void(SSAValue&)>& visitor) override;
    void forEachOperand(const std::function<void(const SSAValue&)>& visitor) const override;
};

// Unconditional jump to a label.
struct SSAInstrGoto : public SSAInstr {
    std::string TargetLabel;
    [[nodiscard]] size_t hash(bool includeTargetName = true) const override
    {
        PEXPR_UNUSED(includeTargetName);
        return std::hash<std::string>{}(TargetLabel);
    }
    [[nodiscard]] bool isEquivalent(const SSAInstr* other) const override
    {
        if (const auto* otherGoto = dynamic_cast<const SSAInstrGoto*>(other))
            return TargetLabel == otherGoto->TargetLabel;
        return false;
    }
};

struct SSAInstrPhi : public SSAInstr {
    SSAValue Target;
    std::vector<SSAValue> Conditions;
    std::vector<SSAValue> Branches; // One more than Conditions due to 'else' case
    [[nodiscard]] size_t hash(bool includeTargetName = true) const override
    {
        size_t h = Target.hash(includeTargetName);
        h        = h * 31 + std::hash<size_t>{}(Conditions.size());
        for (const auto& cond : Conditions)
            h = h * 31 + cond.hash(includeTargetName);
        h = h * 31 + std::hash<size_t>{}(Branches.size());
        for (const auto& branch : Branches)
            h = h * 31 + branch.hash(includeTargetName);
        return h;
    }
    [[nodiscard]] bool isEquivalent(const SSAInstr* other) const override
    {
        if (const auto* otherPhi = dynamic_cast<const SSAInstrPhi*>(other)) {
            if (Target != otherPhi->Target)
                return false;
            if (Conditions.size() != otherPhi->Conditions.size())
                return false;
            if (Branches.size() != otherPhi->Branches.size())
                return false;
            for (size_t i = 0; i < Conditions.size(); ++i)
                if (Conditions[i] != otherPhi->Conditions[i])
                    return false;
            for (size_t i = 0; i < Branches.size(); ++i)
                if (Branches[i] != otherPhi->Branches[i])
                    return false;
            return true;
        }
        return false;
    }
    void forEachOperand(const std::function<void(SSAValue&)>& visitor) override;
    void forEachOperand(const std::function<void(const SSAValue&)>& visitor) const override;
    void forEachTarget(const std::function<void(SSAValue&)>& visitor) override;
    void forEachTarget(const std::function<void(const SSAValue&)>& visitor) const override;
};
} // namespace PExpr::ssa