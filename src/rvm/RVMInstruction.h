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

    /// Visit all RVMValues in this instruction
    inline void forEachValue(const std::function<void(RVMValue&)>& visitor)
    {
        forEachDestination(visitor);
        forEachSource(visitor);
    }

    inline void forEachValue(const std::function<void(const RVMValue&)>& visitor) const
    {
        forEachDestination(visitor);
        forEachSource(visitor);
    }

    /// Visit destination RVMValue in this instruction
    virtual void forEachDestination(const std::function<void(RVMValue&)>& visitor) { PEXPR_UNUSED(visitor); }
    virtual void forEachDestination(const std::function<void(const RVMValue&)>& visitor) const { PEXPR_UNUSED(visitor); }

    /// Visit all source RVMValues in this instruction
    virtual void forEachSource(const std::function<void(RVMValue&)>& visitor) { PEXPR_UNUSED(visitor); }
    virtual void forEachSource(const std::function<void(const RVMValue&)>& visitor) const { PEXPR_UNUSED(visitor); }
};

/// 2-operand instruction: dst = op src (unary ops, conversions)
class RVMInstr2Op : public RVMInstr {
public:
    RVMInstr2Op(Opcode op, RVMValue dst, RVMValue src);

    [[nodiscard]] Opcode opcode() const override { return mOpcode; }

    [[nodiscard]] inline const RVMValue& destination() const { return mDst; }
    [[nodiscard]] inline RVMValue& destination() { return mDst; }
    [[nodiscard]] inline const RVMValue& source() const { return mSrc; }
    [[nodiscard]] inline RVMValue& source() { return mSrc; }

    virtual void forEachDestination(const std::function<void(RVMValue&)>& visitor) override { visitor(mDst); }
    virtual void forEachDestination(const std::function<void(const RVMValue&)>& visitor) const override { visitor(mDst); }
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

    [[nodiscard]] inline const RVMValue& destination() const { return mDst; }
    [[nodiscard]] inline RVMValue& destination() { return mDst; }
    [[nodiscard]] inline const RVMValue& source1() const { return mSrc1; }
    [[nodiscard]] inline RVMValue& source1() { return mSrc1; }
    [[nodiscard]] inline const RVMValue& source2() const { return mSrc2; }
    [[nodiscard]] inline RVMValue& source2() { return mSrc2; }

    virtual void forEachDestination(const std::function<void(RVMValue&)>& visitor) override { visitor(mDst); }
    virtual void forEachDestination(const std::function<void(const RVMValue&)>& visitor) const override { visitor(mDst); }

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
    [[nodiscard]] const std::string& targetLabel() const { return mTargetLabel; }

    [[nodiscard]] inline const RVMValue& condition() const { return mSrc; }
    [[nodiscard]] inline RVMValue& condition() { return mSrc; }

    virtual void forEachSource(const std::function<void(RVMValue&)>& visitor) override { visitor(mSrc); }
    virtual void forEachSource(const std::function<void(const RVMValue&)>& visitor) const override { visitor(mSrc); }

private:
    Opcode mCond; // JZ, JNZ
    RVMValue mSrc;
    std::string mTargetLabel; // Label name
};

/// Jump instruction
class RVMInstrJump : public RVMInstr {
public:
    RVMInstrJump(const std::string& targetLabel);

    [[nodiscard]] Opcode opcode() const override { return Opcode::JMP; }
    [[nodiscard]] const std::string& targetLabel() const { return mTargetLabel; }

private:
    std::string mTargetLabel; // Label name
};

/// Comment instruction (just a friendly comment for the reader)
class RVMInstrComment : public RVMInstr {
public:
    RVMInstrComment(const std::string& msg);

    [[nodiscard]] Opcode opcode() const override { return Opcode::RET; } // Placeholder, comments don't execute
    [[nodiscard]] const std::string& message() const { return mMessage; }

private:
    std::string mMessage;
};

/// Label instruction (marks a position in code for jumps/branches)
class RVMInstrLabel : public RVMInstr {
public:
    RVMInstrLabel(const std::string& name);

    [[nodiscard]] Opcode opcode() const override { return Opcode::RET; } // Placeholder, labels don't execute
    [[nodiscard]] const std::string& labelName() const { return mName; }

private:
    std::string mName;
};

/// Call instruction
class RVMInstrCall : public RVMInstr {
public:
    RVMInstrCall(bool isExternal, size_t numParams, size_t numReturns, const std::string& funcName);

    [[nodiscard]] Opcode opcode() const override { return mIsExternal ? Opcode::CALL_EXTERNAL : Opcode::CALL_INTERNAL; }
    [[nodiscard]] const std::string& functionName() const { return mFuncName; }

    [[nodiscard]] inline size_t parameterCount() const { return mParameterCount; }
    [[nodiscard]] inline size_t returnCount() const { return mReturnCount; }
    [[nodiscard]] inline bool isExternal() const { return mIsExternal; }

    virtual void forEachDestination(const std::function<void(RVMValue&)>& visitor) override;
    virtual void forEachDestination(const std::function<void(const RVMValue&)>& visitor) const override;
    virtual void forEachSource(const std::function<void(RVMValue&)>& visitor) override;
    virtual void forEachSource(const std::function<void(const RVMValue&)>& visitor) const override;

private:
    size_t mParameterCount;
    size_t mReturnCount;
    std::string mFuncName;
    bool mIsExternal;
};

/// Return instruction
class RVMInstrReturn : public RVMInstr {
public:
    RVMInstrReturn(size_t numReturns);

    [[nodiscard]] Opcode opcode() const override { return Opcode::RET; }

    [[nodiscard]] inline size_t returnCount() const { return mReturnCount; }

    virtual void forEachSource(const std::function<void(RVMValue&)>& visitor) override;
    virtual void forEachSource(const std::function<void(const RVMValue&)>& visitor) const override;

private:
    size_t mReturnCount;
};

/// String literal instruction - loads a string literal into a register
class RVMInstrStringLiteral : public RVMInstr {
public:
    RVMInstrStringLiteral(RVMValue dst, const std::string& str);

    [[nodiscard]] Opcode opcode() const override { return Opcode::LOAD_STRING; }
    [[nodiscard]] const std::string& stringValue() const { return mString; }

    [[nodiscard]] inline const RVMValue& destination() const { return mDst; }
    [[nodiscard]] inline RVMValue& destination() { return mDst; }

    virtual void forEachDestination(const std::function<void(RVMValue&)>& visitor) override { visitor(mDst); }
    virtual void forEachDestination(const std::function<void(const RVMValue&)>& visitor) const override { visitor(mDst); }

private:
    RVMValue mDst;
    std::string mString;
};

} // namespace PExpr::rvm
