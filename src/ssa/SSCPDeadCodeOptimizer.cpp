#include "SSCPDeadCodeOptimizer.h"
#include "SSAMapper.h"

namespace PExpr::ssa {

void SSCPDeadCodeOptimizer::resetAndCountUses(const InstructionList& instructions)
{
    mUseCount.clear();
    for (const auto& instrPtr : instructions) {
        if (!instrPtr)
            continue;

        countUsesInInstr(instrPtr.get());
    }
}

bool SSCPDeadCodeOptimizer::removeDeadAssigns(InstructionList& instructions, const std::unordered_set<std::string>& sideEffectedFunctions)
{
    resetAndCountUses(instructions);

    bool changed = false;

    for (auto it = instructions.begin(); it != instructions.end();) {
        if (!*it) {
            ++it;
            continue;
        }

        SSAValue target;
        if (auto asg = dynamic_cast<SSAInstrAssign*>(it->get())) {
            target = asg->Target;
        } else if (auto c = dynamic_cast<SSAInstrCall*>(it->get())) {
            target = c->Target;
        } else if (auto phi = dynamic_cast<SSAInstrPhi*>(it->get())) {
            target = phi->Target;
        } else {
            ++it;
            continue;
        }

        int uses = 0;
        if (auto uit = mUseCount.find(target.Name); uit != mUseCount.end())
            uses = uit->second;

        if (uses == 0 && !instrHasSideEffects(it->get(), sideEffectedFunctions)) {
            changed = true;
            it      = instructions.erase(it);
            continue;
        }
        ++it;
    }

    return changed;
}

void SSCPDeadCodeOptimizer::countUsesInInstr(const SSAInstr* instr)
{
    if (!instr)
        return;

    instr->forEachOperand([this](const SSAValue& val) {
        if (val.Kind != SSAValue::Kind::Constant)
            ++mUseCount[val.Name];
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
