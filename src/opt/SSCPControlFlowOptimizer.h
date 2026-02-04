#pragma once

#include "PExpr.h"

namespace PExpr::ssa {
class SSAInstr;
}

namespace PExpr::opt {

class SSCPControlFlowOptimizer {
public:
    using InstructionList = std::vector<std::shared_ptr<ssa::SSAInstr>>;

    bool removeEmptyBranches(InstructionList& instructions);
    bool removeObsoleteLabels(InstructionList& instructions);
    inline bool collapse(InstructionList& instructions)
    {
        bool changed = false;
        if (collapsePhiNodes(instructions))
            changed = true;
        if (collapseBrNodes(instructions))
            changed = true;
        if (cleanupGotoNodes(instructions))
            changed = true;
        return changed;
    }

private:
    bool collapsePhiNodes(InstructionList& instructions);
    bool collapseBrNodes(InstructionList& instructions);
    bool cleanupGotoNodes(InstructionList& instructions);
};

} // namespace PExpr::opt
