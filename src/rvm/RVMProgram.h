#pragma once

#include "RVMInstruction.h"
#include "RVMStringTable.h"

namespace PExpr::rvm {

/// RVM program
class RVMProgram {
public:
    std::vector<std::shared_ptr<RVMInstr>> Body;
    std::shared_ptr<RVMStringTable> StringTable;
};

} // namespace PExpr::rvm
