#pragma once

#include "SSAMapper.h"

#include <unordered_map>
#include <unordered_set>

namespace PExpr::ssa {

class SSCPDeadCodeOptimizer {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    bool removeDeadAssigns(InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions);

private:
    void resetAndCountUses(const InstructionList& instructions);
    void countUsesInInstr(const SSAInstr* instr);
    bool instrHasSideEffects(const SSAInstr* instr, const std::unordered_set<std::string>& sideEffectFunctions) const;

    std::unordered_map<std::string, int> mUseCount;
};

} // namespace PExpr::ssa
