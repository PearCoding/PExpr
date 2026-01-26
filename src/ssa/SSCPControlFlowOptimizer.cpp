#include "SSCPControlFlowOptimizer.h"
#include "SSAMapper.h"

#include <algorithm>
#include <ranges>

namespace PExpr::ssa {

bool SSCPControlFlowOptimizer::removeEmptyBranches(InstructionList& instructions)
{
    if (instructions.empty())
        return false;

    for (size_t i = 0; i < instructions.size() - 1; ++i) {
        if (!instructions[i])
            continue;

        // Check for the following:
        //   lbl.1:
        //   goto lbl.2
        // and
        //   lbl.1:
        //   lbl.2:
        if (auto l = dynamic_cast<const SSAInstrLabel*>(instructions[i].get())) {
            // Is the following instruction a basic jump?
            if (auto g = dynamic_cast<const SSAInstrGoto*>(instructions[i + 1].get())) {
                // Replace all necessary stuff in this program
                for (size_t j = 0; j < instructions.size(); ++j) {
                    if (i == j)
                        continue;
                    // Check branches/gotos
                    if (auto br = dynamic_cast<SSAInstrBranch*>(instructions[j].get())) {
                        if (br->TargetLabel == l->Name)
                            br->TargetLabel = g->TargetLabel;
                    } else if (auto gt = dynamic_cast<SSAInstrGoto*>(instructions[j].get())) {
                        if (gt->TargetLabel == l->Name)
                            gt->TargetLabel = g->TargetLabel;
                    }
                }

                // Remove the label and the goto
                instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
                return true;
            } else if (auto l2 = dynamic_cast<const SSAInstrLabel*>(instructions[i + 1].get())) {
                // Replace all necessary stuff in this program
                for (size_t j = 0; j < instructions.size(); ++j) {
                    if (i == j)
                        continue;
                    // Check branches/gotos
                    if (auto br = dynamic_cast<SSAInstrBranch*>(instructions[j].get())) {
                        if (br->TargetLabel == l->Name)
                            br->TargetLabel = l2->Name;
                    } else if (auto gt = dynamic_cast<SSAInstrGoto*>(instructions[j].get())) {
                        if (gt->TargetLabel == l->Name)
                            gt->TargetLabel = l2->Name;
                    }
                }

                // Remove the first label
                instructions.erase(instructions.begin() + i);
                return true;
            }
        }

        // Check for the following:
        //   goto lbl.1
        //   lbl.1:
        if (auto g = dynamic_cast<const SSAInstrGoto*>(instructions[i].get())) {
            // Is the following instruction a label?
            if (auto l = dynamic_cast<const SSAInstrLabel*>(instructions[i + 1].get())) {
                // The goto follows strict the label
                if (g->TargetLabel == l->Name) {
                    // Delete the goto
                    instructions.erase(instructions.begin() + i);
                    return true;
                }
            }
        }

        // Check for the following:
        //   br x -> lbl.1
        //   lbl.1:
        if (auto br = dynamic_cast<const SSAInstrBranch*>(instructions[i].get())) {
            // Is the following instruction a label?
            if (auto l = dynamic_cast<const SSAInstrLabel*>(instructions[i + 1].get())) {
                // The goto follows strict the label
                if (br->TargetLabel == l->Name) {
                    // Delete the branching
                    instructions.erase(instructions.begin() + i);
                    return true;
                }
            }
        }
    }

    return false;
}

bool SSCPControlFlowOptimizer::removeObsoleteLabels(InstructionList& instructions)
{
    // Count the usage of the labels
    std::unordered_map<std::string, size_t> counter;
    std::ranges::for_each(instructions, [&counter](const auto& instrPtr) {
        if (auto g = dynamic_cast<const SSAInstrGoto*>(instrPtr.get()))
            counter[g->TargetLabel]++;
        else if (auto br = dynamic_cast<const SSAInstrBranch*>(instrPtr.get()))
            counter[br->TargetLabel]++;
    });

    // Remove labels without usage
    const auto pred = [&counter](const std::shared_ptr<SSAInstr>& instrPtr) -> bool {
        if (auto l = dynamic_cast<const SSAInstrLabel*>(instrPtr.get()))
            return !counter.contains(l->Name) || counter.at(l->Name) == 0;
        return false;
    };

    const auto removed = std::erase_if(instructions, pred);
    return removed > 0;
}

bool SSCPControlFlowOptimizer::collapsePhiNodes(InstructionList& instructions)
{
    bool changed = false;
    for (auto& instrPtr : instructions) {
        if (!instrPtr)
            continue;

        if (auto phi = dynamic_cast<SSAInstrPhi*>(instrPtr.get())) {
            // Check if there are some obsolete branches?
            std::vector<size_t> removableBranches;
            for (size_t i = 0; i < phi->Conditions.size(); ++i) {
                const auto& cond = phi->Conditions[i];
                if (cond.isConstant() && cond.type().kind() == TypeKind::Boolean) {
                    const bool condVal = cond.valueAs<bool>();
                    if (!condVal)
                        removableBranches.push_back(i);
                }
            }

            if (!removableBranches.empty())
                changed = true;

            // Remove the obsolete ones from back to front (for the iterator to work)
            for (auto it = removableBranches.rbegin(); it != removableBranches.rend(); ++it) {
                phi->Branches.erase(phi->Branches.begin() + *it);
                phi->Conditions.erase(phi->Conditions.begin() + *it);
            }

            // Only the 'else' statement survived
            if (phi->Conditions.empty()) {
                SSAInstrAssign asg;
                asg.Target   = phi->Target;
                asg.Operator = SSAInstrAssign::OpKind::Assign;
                asg.Operands = { phi->Branches.at(0) };
                instrPtr     = std::make_shared<SSAInstrAssign>(std::move(asg));
                changed      = true;
                continue;
            }

            // Check if the first entry is truely 'true' -> remove phi and use that one
            const auto& firstCond = phi->Conditions.at(0);
            if (firstCond.isConstant() && firstCond.type().kind() == TypeKind::Boolean) {
                const bool condVal = firstCond.valueAs<bool>();
                PEXPR_ASSERT(condVal, "Expected a 'true' phi condition as all 'false' ones should be erased");
                SSAInstrAssign asg;
                asg.Target   = phi->Target;
                asg.Operator = SSAInstrAssign::OpKind::Assign;
                asg.Operands = { phi->Branches.at(0) };
                instrPtr     = std::make_shared<SSAInstrAssign>(std::move(asg));
                changed      = true;
            }
        }
    }

    return changed;
}

} // namespace PExpr::ssa
