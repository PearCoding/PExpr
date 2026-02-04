#include "RVMContext.h"

namespace PExpr::rvm {

RVMContext::RVMContext()
    : mNextRegId(0)
{
}

RegId RVMContext::allocateRegister(const type::Type& type)
{
    RegId reg           = mNextRegId++;
    mRegisterTypes[reg] = type;
    return reg;
}

const type::Type& RVMContext::getRegisterType(RegId reg) const
{
    auto it = mRegisterTypes.find(reg);
    PEXPR_ASSERT(it != mRegisterTypes.end(), "Unknown register");
    return it->second;
}

void RVMContext::freeRegister(RegId reg)
{
    mRegisterTypes.erase(reg);
}

void RVMContext::reset()
{
    mNextRegId = 0;
    mRegisterTypes.clear();
}

} // namespace PExpr::rvm