#pragma once

#include "RVMInstruction.h"
#include "RVMStringTable.h"

namespace PExpr::rvm {

/// RVM function definition
class RVMFunction {
public:
    std::string name;
    std::vector<type::Type> parameters;
    std::vector<std::shared_ptr<RVMInstr>> body;
    type::Type returnType = type::Type::Unspecified();

    bool external      = false;
    bool hasSideEffect = false;
};

/// RVM program
class RVMProgram {
public:
    std::vector<std::shared_ptr<RVMInstr>> body;
    std::vector<RVMFunction> functions;
    std::shared_ptr<RVMStringTable> stringTable;
};

} // namespace PExpr::rvm
