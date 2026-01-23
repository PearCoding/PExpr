#pragma once

#include "SSAMapper.h"

namespace PExpr::ssa {

class SSCPControlFlowOptimizer {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    bool removeEmptyBranches(InstructionList& instructions);
    bool removeObsoleteLabels(InstructionList& instructions);
    bool collapsePhiNodes(InstructionList& instructions);
};

} // namespace PExpr::ssa
