#pragma once

#include "RVMInstruction.h"

namespace PExpr::rvm {

/// RVM program
class RVMProgram {
public:
    std::vector<std::shared_ptr<RVMInstr>> Body;
};

} // namespace PExpr::rvm
