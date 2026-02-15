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

RVMInstrReturn::RVMInstrReturn(size_t returnCount)
    : mReturnCount(returnCount)
{
}

RVMInstrPushFrame::RVMInstrPushFrame(size_t registerCount)
    : mRegisterCount(registerCount)
{
}

RVMInstrPopFrame::RVMInstrPopFrame(size_t registerCount)
    : mRegisterCount(registerCount)
{
}

RVMInstrStringLiteral::RVMInstrStringLiteral(RVMValue dst, const std::string& str)
    : mDst(dst)
    , mString(str)
{
}

} // namespace PExpr::rvm
