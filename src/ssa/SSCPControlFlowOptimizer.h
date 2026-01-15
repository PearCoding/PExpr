#pragma once

#include "SSAMapper.h"

#include <unordered_map>
#include <unordered_set>

namespace PExpr::ssa {

class SSCPControlFlowOptimizer {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;
    
    SSCPControlFlowOptimizer(std::unordered_map<std::string, int>& useCount);
    
    bool removeEmptyBranches(InstructionList& instructions);
    bool removeObsoleteLabels(InstructionList& instructions);
    bool collapsePhiNodes(InstructionList& instructions);
    void countUsesInInstr(const SSAInstr* instr);
    bool instrHasSideEffects(const SSAInstr* instr, const std::unordered_set<std::string>& sideEffectFunctions) const;
    
private:
    std::unordered_map<std::string, int>& mUseCount;
};

} // namespace PExpr::ssa
