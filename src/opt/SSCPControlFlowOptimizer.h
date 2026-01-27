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
    bool collapsePhiNodes(InstructionList& instructions);
};

} // namespace PExpr::opt
