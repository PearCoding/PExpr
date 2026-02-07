#pragma once

#include "RVMInstruction.h"
#include "RVMStringTable.h"

namespace PExpr::rvm {

/// RVM function definition
class RVMFunction {
public:
    std::string Name;
    std::vector<type::Type> Parameters;
    std::vector<std::shared_ptr<RVMInstr>> Body;
    type::Type ReturnType = type::Type::Unspecified();

    bool External      = false;
    bool HasSideEffect = false;
};

/// RVM program
class RVMProgram {
public:
    std::vector<std::shared_ptr<RVMInstr>> Body;
    std::vector<RVMFunction> Functions;
    std::shared_ptr<RVMStringTable> StringTable;
};

} // namespace PExpr::rvm
