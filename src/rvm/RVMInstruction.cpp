#include "RVMInstruction.h"

namespace PExpr::rvm {

RVMInstr2Op::RVMInstr2Op(Opcode op, RVMValue dst, RVMValue src)
    : mOpcode(op)
    , mDst(dst)
    , mSrc(src)
{
}

RVMInstr3Op::RVMInstr3Op(Opcode op, RVMValue dst, RVMValue src1, RVMValue src2)
    : mOpcode(op)
    , mDst(dst)
    , mSrc1(src1)
    , mSrc2(src2)
{
}

RVMInstrBranch::RVMInstrBranch(Opcode cond, RVMValue src, const std::string& targetLabel)
    : mCond(cond)
    , mSrc(src)
    , mTargetLabel(targetLabel)
{
}

RVMInstrJump::RVMInstrJump(const std::string& targetLabel)
    : mTargetLabel(targetLabel)
{
}

RVMInstrComment::RVMInstrComment(const std::string& msg)
    : mMessage(msg)
{
}

RVMInstrLabel::RVMInstrLabel(const std::string& name)
    : mName(name)
{
}

RVMInstrCall::RVMInstrCall(bool isExternal, size_t numParams, size_t numReturns, const std::string& funcName)
    : mParameterCount(numParams)
    , mReturnCount(numReturns)
    , mFuncName(funcName)
    , mIsExternal(isExternal)
{
}

void RVMInstrCall::forEachDestination(const std::function<void(RVMValue&)>& visitor)
{
    for (RegId i = 0; i < mReturnCount; ++i) {
        auto val = RVMValue::Register(i, type::Type(type::TypeKind::Unspecified));
        visitor(val);
    }
}

void RVMInstrCall::forEachDestination(const std::function<void(const RVMValue&)>& visitor) const
{
    for (RegId i = 0; i < mReturnCount; ++i)
        visitor(RVMValue::Register(i, type::Type(type::TypeKind::Unspecified)));
}

void RVMInstrCall::forEachSource(const std::function<void(RVMValue&)>& visitor)
{
    for (RegId i = 0; i < mParameterCount; ++i) {
        auto val = RVMValue::Register(i, type::Type(type::TypeKind::Unspecified));
        visitor(val);
    }
}

void RVMInstrCall::forEachSource(const std::function<void(const RVMValue&)>& visitor) const
{
    for (RegId i = 0; i < mParameterCount; ++i)
        visitor(RVMValue::Register(i, type::Type(type::TypeKind::Unspecified)));
}

RVMInstrReturn::RVMInstrReturn(size_t returnCount)
    : mReturnCount(returnCount)
{
}

void RVMInstrReturn::forEachSource(const std::function<void(RVMValue&)>& visitor)
{
    for (RegId i = 0; i < mReturnCount; ++i) {
        auto val = RVMValue::Register(i, type::Type(type::TypeKind::Unspecified));
        visitor(val);
    }
}

void RVMInstrReturn::forEachSource(const std::function<void(const RVMValue&)>& visitor) const
{
    for (RegId i = 0; i < mReturnCount; ++i)
        visitor(RVMValue::Register(i, type::Type(type::TypeKind::Unspecified)));
}

RVMInstrStringLiteral::RVMInstrStringLiteral(RVMValue dst, const std::string& str)
    : mDst(dst)
    , mString(str)
{
}

} // namespace PExpr::rvm
