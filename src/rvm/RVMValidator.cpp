#include "RVMValidator.h"

namespace PExpr::rvm {

bool RVMValidator::checkIfElementary(const RVMProgram* program)
{
    for (const auto& instr : program->Body) {
        if (!checkIfElementary(instr.get()))
            return false;
    }
    return true;
}

bool RVMValidator::checkIfElementary(const RVMInstr* instr)
{
    bool bad = false;
    instr->forEachValue([&bad](const RVMValue& val) {
        if (!checkIfElementary(val))
            bad = true;
    });
    return !bad;
}

bool RVMValidator::checkIfElementary(const RVMValue& value)
{
    return value.type().isSpecified() && !value.type().isTuple();
}
} // namespace PExpr::rvm
