#include "SSCPDeadCodeOptimizer.h"
#include "SSAMapper.h"

#include <algorithm>
#include <ranges>

namespace PExpr::ssa {

void SSCPDeadCodeOptimizer::resetAndCountUses(const InstructionList& instructions)
{
    mUseCount.clear();
    std::ranges::for_each(instructions, [this](const auto& instrPtr) { countUsesInInstr(instrPtr.get()); });
}

bool SSCPDeadCodeOptimizer::removeDeadAssigns(InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions)
{
    resetAndCountUses(instructions);

    const auto pred = [this, &sideEffectedFunctions](const std::shared_ptr<SSAInstr>& instrPtr) -> bool {
        SSAValue target;
        if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
            target = asg->Target;
        } else if (auto c = dynamic_cast<SSAInstrCall*>(instrPtr.get())) {
            target = c->Target;
        } else if (auto phi = dynamic_cast<SSAInstrPhi*>(instrPtr.get())) {
            target = phi->Target;
        } else {
            return false;
        }

        int uses = 0;
        if (auto uit = mUseCount.find(target.name()); uit != mUseCount.end())
            uses = uit->second;

        return uses == 0 && !instrHasSideEffects(instrPtr.get(), sideEffectedFunctions);
    };

    const auto removed = std::erase_if(instructions, pred);
    return removed > 0;
}

void SSCPDeadCodeOptimizer::countUsesInInstr(const SSAInstr* instr)
{
    if (!instr)
        return;

    instr->forEachOperand([this](const SSAValue& val) {
        if (!val.isConstant())
            ++mUseCount[val.name()];
    });
}

bool SSCPDeadCodeOptimizer::instrHasSideEffects(const SSAInstr* instr, const std::unordered_set<std::string>& sideEffectFunctions) const
{
    if (!instr)
        return false;

    // Calls have side-effects only if the callee is known to be side-effecting.
    if (auto c = dynamic_cast<const SSAInstrCall*>(instr)) {
        if (sideEffectFunctions.find(c->FunctionName) != sideEffectFunctions.end())
            return true;
    }

    // other instructions (assign, phi) are assumed side-effect free
    return false;
}

} // namespace PExpr::ssa
