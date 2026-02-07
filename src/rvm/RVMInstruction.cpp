#include "RVMInstruction.h"

namespace PExpr::rvm {

void RVMInstr::forEachValue(const std::function<void(RVMValue&)>& visitor)
{
    if (auto d = dst()) {
        visitor(*d);
    }
    auto srcValues = srcs();
    for (auto& src : srcValues) {
        visitor(src);
    }
}

void RVMInstr::forEachValue(const std::function<void(const RVMValue&)>& visitor) const
{
    if (auto d = dst()) {
        visitor(*d);
    }
    auto srcValues = srcs();
    for (const auto& src : srcValues) {
        visitor(src);
    }
}

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

RVMInstrExternalCall::RVMInstrExternalCall(std::optional<RVMValue> dst, const std::string& funcName,
                                           const std::vector<RVMValue>& args)
    : mDst(dst)
    , mFuncName(funcName)
    , mArgs(args)
{
}

RVMInstrInternalCall::RVMInstrInternalCall(const std::string& funcName)
    : mFuncName(funcName)
{
}

RVMInstrReturn::RVMInstrReturn(std::optional<RVMValue> retVal)
    : mRetVal(retVal)
{
}

RVMInstrPushFrame::RVMInstrPushFrame(uint32_t registerCount)
    : mRegisterCount(registerCount)
{
}

RVMInstrPopFrame::RVMInstrPopFrame(uint32_t registerCount)
    : mRegisterCount(registerCount)
{
}

} // namespace PExpr::rvm