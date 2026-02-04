#pragma once

#include "RVMTypes.h"

namespace PExpr::rvm {

/// RVM context for register management
class RVMContext {
public:
    RVMContext();

    /// Allocate a new register with given type
    [[nodiscard]] RegId allocateRegister(const type::Type& type);

    /// Get type of a register
    [[nodiscard]] const type::Type& getRegisterType(RegId reg) const;

    /// Free a register (mark as available)
    void freeRegister(RegId reg);

    /// Reset all registers
    void reset();

private:
    RegId mNextRegId = 0;
    std::unordered_map<RegId, type::Type> mRegisterTypes;
};

} // namespace PExpr::rvm
