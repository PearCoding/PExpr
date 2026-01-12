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
    if (const auto ptr = dynamic_cast<const SSAInstrAssign*>(instr)) {
        if (!checkIfTyped(ptr->Target))
            return false;
        for (const auto& instr : ptr->Operands) {
            if (!checkIfTyped(instr))
                return false;
        }
    } else if (const auto ptr = dynamic_cast<const SSAInstrCall*>(instr)) {
        if (!checkIfTyped(ptr->Target))
            return false;
        for (const auto& instr : ptr->Arguments) {
            if (!checkIfTyped(instr))
                return false;
        }
    } else if (const auto ptr = dynamic_cast<const SSAInstrBranch*>(instr)) {
        if (!checkIfTyped(ptr->Condition))
            return false;
    } else if (dynamic_cast<const SSAInstrGoto*>(instr)) {
        /* Ignore */
    } else if (dynamic_cast<const SSAInstrLabel*>(instr)) {
        /* Ignore */
    } else if (const auto ptr = dynamic_cast<const SSAInstrReturn*>(instr)) {
        if (!checkIfTyped(ptr->Value))
            return false;
    } else if (const auto ptr = dynamic_cast<const SSAInstrPhi*>(instr)) {
        if (!checkIfTyped(ptr->Target))
            return false;
        for (const auto& instr : ptr->Conditions) {
            if (!checkIfTyped(instr))
                return false;
        }
        for (const auto& instr : ptr->Branches) {
            if (!checkIfTyped(instr))
                return false;
        }
    } else {
        PEXPR_ASSERT(false, "Non exhaustive checkIfTyped");
        return false;
    }

    return true;
}

bool SSAValidator::checkIfTyped(const SSAValue& value)
{
    return value.Type != ElementaryType::Unspecified;
}
} // namespace PExpr::ssa
