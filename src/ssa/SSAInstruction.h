#pragma once

#include "SSAValue.h"
#include "ast/Enums.h"

#include <functional>
#include <memory>
#include <vector>

namespace PExpr::ssa {
class SSAInstr {
public:
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

class SSAInstrAssign : public SSAInstr {
public:
    enum class OpKind { Assign,
                        Unary,
                        Binary,
                        Access,
                        Cast };

    SSAValue Target;
    OpKind Operator;

    // OpKind specific data
    ast::UnaryOperation UnaryOp   = ast::UnaryOperation::Pos;
    ast::BinaryOperation BinaryOp = ast::BinaryOperation::Add;
    std::vector<SSAValue> Operands;

    [[nodiscard]] size_t hash(bool includeTargetName = true) const override;
    [[nodiscard]] bool isEquivalent(const SSAInstr* other) const override;
    void forEachOperand(const std::function<void(SSAValue&)>& visitor) override;
    void forEachOperand(const std::function<void(const SSAValue&)>& visitor) const override;
    void forEachTarget(const std::function<void(SSAValue&)>& visitor) override;
    void forEachTarget(const std::function<void(const SSAValue&)>& visitor) const override;
};

class SSAInstrCall : public SSAInstr {
public:
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

class SSAInstrReturn : public SSAInstr {
public:
    SSAValue Value;
    [[nodiscard]] size_t hash(bool includeTargetName = true) const override
    {
        PEXPR_UNUSED(includeTargetName);
        return Value.hash(true);
    }
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
class SSAInstrLabel : public SSAInstr {
public:
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
class SSAInstrBranch : public SSAInstr {
public:
    SSAValue Condition;
    std::string TargetLabel;
    [[nodiscard]] size_t hash(bool includeTargetName = true) const override
    {
        PEXPR_UNUSED(includeTargetName);
        size_t h = Condition.hash(true);
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
class SSAInstrGoto : public SSAInstr {
public:
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

class SSAInstrPhi : public SSAInstr {
public:
    SSAValue Target;
    std::vector<SSAValue> Conditions;
    std::vector<SSAValue> Branches; // One more than Conditions due to 'else' case
    [[nodiscard]] size_t hash(bool includeTargetName = true) const override
    {
        size_t h = Target.hash(includeTargetName);
        h        = h * 31 + std::hash<size_t>{}(Conditions.size());
        for (const auto& cond : Conditions)
            h = h * 31 + cond.hash(true);
        h = h * 31 + std::hash<size_t>{}(Branches.size());
        for (const auto& branch : Branches)
            h = h * 31 + branch.hash(true);
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