#include "RVMRegisterLinearizer.h"
#include "RVMInstruction.h"
#include "RVMValue.h"

#include <algorithm>
#include <vector>

namespace PExpr::rvm {

bool RVMRegisterLinearizer::linearize(RVMProgram& program)
{
    if (program.empty())
        return false;

    // Collect all registers used in the program
    std::unordered_set<RegId> registers = collectRegisters(program);
    if (registers.empty())
        return false;

    // Create linear mapping preserving order
    std::unordered_map<RegId, RegId> regMap = createLinearMapping(registers);

    // Check if any changes needed
    bool changesNeeded = false;
    for (const auto& [oldReg, newReg] : regMap) {
        if (oldReg != newReg) {
            changesNeeded = true;
            break;
        }
    }

    if (!changesNeeded)
        return false;

    // Rewrite program with new register assignments
    rewriteProgram(program, regMap);

    return true;
}

std::unordered_set<RegId> RVMRegisterLinearizer::collectRegisters(const RVMProgram& program)
{
    std::unordered_set<RegId> registers;

    for (const auto& instr : program) {
        instr->forEachValue([&](const RVMValue& val) {
            if (val.isRegister())
                registers.insert(val.regId());
        });
    }

    return registers;
}

std::unordered_map<RegId, RegId> RVMRegisterLinearizer::createLinearMapping(const std::unordered_set<RegId>& registers)
{
    if (registers.empty())
        return {};

    // Sort registers to preserve order
    std::vector<RegId> sortedRegisters(registers.begin(), registers.end());
    std::sort(sortedRegisters.begin(), sortedRegisters.end());

    // Create mapping: old register -> new sequential register starting from 0
    std::unordered_map<RegId, RegId> regMap;
    RegId nextReg = 0;
    for (RegId oldReg : sortedRegisters) 
        regMap[oldReg] = nextReg++;

    return regMap;
}

void RVMRegisterLinearizer::rewriteProgram(RVMProgram& program, const std::unordered_map<RegId, RegId>& regMap)
{
    for (auto& instr : program) {
        instr->forEachValue([&regMap](RVMValue& value) {
            if (value.isRegister()) {
                RegId oldReg = value.regId();
                if (auto it = regMap.find(oldReg); it != regMap.end() && it->second != oldReg) 
                    // Create new register value with same type
                    value = RVMValue::Register(it->second, value.type());
            }
        });
    }
}

} // namespace PExpr::rvm