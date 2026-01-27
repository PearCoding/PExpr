#pragma once

#include "PExpr.h"

#include <unordered_map>
#include <unordered_set>

namespace PExpr::ssa {
class SSAInstr;
}

namespace PExpr::opt {

class SSCPDeadCodeOptimizer {
public:
    using InstructionList = std::vector<std::shared_ptr<ssa::SSAInstr>>;

    bool removeDeadAssigns(InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions);

private:
    void resetAndCountUses(const InstructionList& instructions);
    void countUsesInInstr(const ssa::SSAInstr* instr);
    bool instrHasSideEffects(const ssa::SSAInstr* instr, const std::unordered_set<std::string>& sideEffectFunctions) const;

    std::unordered_map<std::string, int> mUseCount;
};

} // namespace PExpr::opt
