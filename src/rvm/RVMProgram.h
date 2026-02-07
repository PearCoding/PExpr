#pragma once

#include "RVMInstruction.h"

namespace PExpr::rvm {

/// RVM program
using RVMProgram = std::vector<std::shared_ptr<RVMInstr>>;

} // namespace PExpr::rvm
