#pragma once

#include "RVMValue.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace PExpr::rvm {
/// General RVM instruction
class RVMInstr {
public:
    virtual ~RVMInstr() = default;

    /// Get instruction opcode
    [[nodiscard]] virtual Opcode opcode() const = 0;

    /// Get destination register (if any)
    [[nodiscard]] virtual std::optional<RVMValue> dst() const = 0;

    /// Get source operands
    [[nodiscard]] virtual std::vector<RVMValue> srcs() const = 0;

    /// Visit all RVMValues in this instruction
    inline void forEachValue(const std::function<void(RVMValue&)>& visitor)
    {
        forDestination(visitor);
        forEachSource(visitor);
    }

    inline void forEachValue(const std::function<void(const RVMValue&)>& visitor) const
    {
        forDestination(visitor);
        forEachSource(visitor);
    }

    /// Visit destination RVMValue in this instruction
    virtual void forDestination(const std::function<void(RVMValue&)>& visitor) { PEXPR_UNUSED(visitor); }
    virtual void forDestination(const std::function<void(const RVMValue&)>& visitor) const { PEXPR_UNUSED(visitor); }

    /// Visit all source RVMValues in this instruction
    virtual void forEachSource(const std::function<void(RVMValue&)>& visitor) { PEXPR_UNUSED(visitor); }
    virtual void forEachSource(const std::function<void(const RVMValue&)>& visitor) const { PEXPR_UNUSED(visitor); }
};

/// 2-operand instruction: dst = op src (unary ops, conversions)
class RVMInstr2Op : public RVMInstr {
public:
    RVMInstr2Op(Opcode op, RVMValue dst, RVMValue src);

    [[nodiscard]] Opcode opcode() const override { return mOpcode; }
    [[nodiscard]] std::optional<RVMValue> dst() const override { return mDst; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return { mSrc }; }

    virtual void forDestination(const std::function<void(RVMValue&)>& visitor) override { visitor(mDst); }
    virtual void forDestination(const std::function<void(const RVMValue&)>& visitor) const override { visitor(mDst); }
    virtual void forEachSource(const std::function<void(RVMValue&)>& visitor) override { visitor(mSrc); }
    virtual void forEachSource(const std::function<void(const RVMValue&)>& visitor) const override { visitor(mSrc); }

private:
    Opcode mOpcode;
    RVMValue mDst;
    RVMValue mSrc;
};

/// 3-operand instruction: dst = op src1 src2 (arithmetic/logical)
class RVMInstr3Op : public RVMInstr {
public:
    RVMInstr3Op(Opcode op, RVMValue dst, RVMValue src1, RVMValue src2);

    [[nodiscard]] Opcode opcode() const override { return mOpcode; }
    [[nodiscard]] std::optional<RVMValue> dst() const override { return mDst; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return { mSrc1, mSrc2 }; }

    virtual void forDestination(const std::function<void(RVMValue&)>& visitor) override { visitor(mDst); }
    virtual void forDestination(const std::function<void(const RVMValue&)>& visitor) const override { visitor(mDst); }

    virtual void forEachSource(const std::function<void(RVMValue&)>& visitor) override
    {
        visitor(mSrc1);
        visitor(mSrc2);
    }
    virtual void forEachSource(const std::function<void(const RVMValue&)>& visitor) const override
    {
        visitor(mSrc1);
        visitor(mSrc2);
    }

private:
    Opcode mOpcode;
    RVMValue mDst;
    RVMValue mSrc1;
    RVMValue mSrc2;
};

/// Branch instruction
class RVMInstrBranch : public RVMInstr {
public:
    RVMInstrBranch(Opcode cond, RVMValue src, const std::string& targetLabel);

    [[nodiscard]] Opcode opcode() const override { return mCond; }
    [[nodiscard]] std::optional<RVMValue> dst() const override { return std::nullopt; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return { mSrc }; }
    [[nodiscard]] const std::string& targetLabel() const { return mTargetLabel; }

    virtual void forEachSource(const std::function<void(RVMValue&)>& visitor) override { visitor(mSrc); }
    virtual void forEachSource(const std::function<void(const RVMValue&)>& visitor) const override { visitor(mSrc); }

private:
    Opcode mCond; // BR, BRZ, BRNZ
    RVMValue mSrc;
    std::string mTargetLabel; // Label name
};

/// Jump instruction
class RVMInstrJump : public RVMInstr {
public:
    RVMInstrJump(const std::string& targetLabel);

    [[nodiscard]] Opcode opcode() const override { return Opcode::JMP; }
    [[nodiscard]] std::optional<RVMValue> dst() const override { return std::nullopt; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return {}; }
    [[nodiscard]] const std::string& targetLabel() const { return mTargetLabel; }

private:
    std::string mTargetLabel; // Label name
};

/// Comment instruction (just a friendly comment for the reader)
class RVMInstrComment : public RVMInstr {
public:
    RVMInstrComment(const std::string& msg);

    [[nodiscard]] Opcode opcode() const override { return Opcode::RET; } // Placeholder, comments don't execute
    [[nodiscard]] std::optional<RVMValue> dst() const override { return std::nullopt; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return {}; }
    [[nodiscard]] const std::string& message() const { return mMessage; }

private:
    std::string mMessage;
};

/// Label instruction (marks a position in code for jumps/branches)
class RVMInstrLabel : public RVMInstr {
public:
    RVMInstrLabel(const std::string& name);

    [[nodiscard]] Opcode opcode() const override { return Opcode::RET; } // Placeholder, labels don't execute
    [[nodiscard]] std::optional<RVMValue> dst() const override { return std::nullopt; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return {}; }
    [[nodiscard]] const std::string& labelName() const { return mName; }

private:
    std::string mName;
};

/// Call instruction
class RVMInstrExternalCall : public RVMInstr {
public:
    RVMInstrExternalCall(std::optional<RVMValue> dst, const std::string& funcName,
                         const std::vector<RVMValue>& args);

    [[nodiscard]] Opcode opcode() const override { return Opcode::CALL_EXTERNAL; }
    [[nodiscard]] std::optional<RVMValue> dst() const override { return mDst; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return mArgs; }
    [[nodiscard]] const std::string& functionName() const { return mFuncName; }

    virtual void forDestination(const std::function<void(RVMValue&)>& visitor) override
    {
        if (mDst.has_value())
            visitor(mDst.value());
    }
    virtual void forDestination(const std::function<void(const RVMValue&)>& visitor) const override
    {
        if (mDst.has_value())
            visitor(mDst.value());
    }

    virtual void forEachSource(const std::function<void(RVMValue&)>& visitor) override
    {
        for (auto& val : mArgs)
            visitor(val);
    }
    virtual void forEachSource(const std::function<void(const RVMValue&)>& visitor) const override
    {
        for (auto& val : mArgs)
            visitor(val);
    }

private:
    std::optional<RVMValue> mDst;
    std::string mFuncName;
    std::vector<RVMValue> mArgs;
};

class RVMInstrInternalCall : public RVMInstr {
public:
    RVMInstrInternalCall(const std::string& funcName);

    [[nodiscard]] Opcode opcode() const override { return Opcode::CALL_INTERNAL; }
    [[nodiscard]] std::optional<RVMValue> dst() const override { return std::nullopt; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return {}; }
    [[nodiscard]] const std::string& functionName() const { return mFuncName; }

private:
    std::string mFuncName;
};

/// Return instruction
class RVMInstrReturn : public RVMInstr {
public:
    RVMInstrReturn(size_t numReturns);

    [[nodiscard]] Opcode opcode() const override { return Opcode::RET; }
    [[nodiscard]] std::optional<RVMValue> dst() const override { return std::nullopt; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return {}; }

    [[nodiscard]] inline size_t returnCount() const { return mReturnCount; }

private:
    size_t mReturnCount;
};

/// Push frame instruction - saves registers %r1 to %r{count}
class RVMInstrPushFrame : public RVMInstr {
public:
    RVMInstrPushFrame(size_t registerCount);

    [[nodiscard]] Opcode opcode() const override { return Opcode::PUSH_FRAME; }
    [[nodiscard]] std::optional<RVMValue> dst() const override { return std::nullopt; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return {}; }
    [[nodiscard]] size_t registerCount() const { return mRegisterCount; }

private:
    size_t mRegisterCount; // Number of registers to save, starting from %r1
};

/// Pop frame instruction - restores registers %r1 to %r{count}
class RVMInstrPopFrame : public RVMInstr {
public:
    RVMInstrPopFrame(size_t registerCount);

    [[nodiscard]] Opcode opcode() const override { return Opcode::POP_FRAME; }
    [[nodiscard]] std::optional<RVMValue> dst() const override { return std::nullopt; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return {}; }
    [[nodiscard]] size_t registerCount() const { return mRegisterCount; }

private:
    size_t mRegisterCount; // Number of registers to restore, starting from %r1
};

/// String literal instruction - loads a string literal into a register
class RVMInstrStringLiteral : public RVMInstr {
public:
    RVMInstrStringLiteral(RVMValue dst, const std::string& str);

    [[nodiscard]] Opcode opcode() const override { return Opcode::LOAD_STRING; }
    [[nodiscard]] std::optional<RVMValue> dst() const override { return mDst; }
    [[nodiscard]] std::vector<RVMValue> srcs() const override { return {}; }
    [[nodiscard]] const std::string& stringValue() const { return mString; }

    virtual void forDestination(const std::function<void(RVMValue&)>& visitor) override { visitor(mDst); }
    virtual void forDestination(const std::function<void(const RVMValue&)>& visitor) const override { visitor(mDst); }

private:
    RVMValue mDst;
    std::string mString;
};

} // namespace PExpr::rvm
