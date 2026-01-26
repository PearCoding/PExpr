#include "SSAValidator.h"

namespace PExpr::ssa {

bool SSAValidator::checkIfTyped(const SSAProgram* program)
{
    for (const auto& instr : program->Body) {
        if (!checkIfTyped(instr.get()))
            return false;
    }
    for (const auto& func : program->Functions) {
        if (!checkIfTyped(&func))
            return false;
    }
    return true;
}

bool SSAValidator::checkIfTyped(const SSAFunction* func)
{
    for (const auto& instr : func->Body) {
        if (!checkIfTyped(instr.get()))
            return false;
    }
    return true;
}

bool SSAValidator::checkIfTyped(const SSAInstr* instr)
{
    bool bad = false;
    instr->forEachValue([&bad](const SSAValue& val) {
        if (!checkIfTyped(val))
            bad = true;
    });
    return !bad;
}

bool SSAValidator::checkIfTyped(const SSAValue& value)
{
    return value.type().kind() != TypeKind::Unspecified && value.type().kind() != TypeKind::Error;
}
} // namespace PExpr::ssa
