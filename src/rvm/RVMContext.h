#pragma once

#include "RVMTypes.h"

namespace PExpr::rvm {

/// RVM context for linear register management
class RVMContext {
public:
    RVMContext();

    /// Allocate a new register
    [[nodiscard]] inline RegId allocateRegister() { return mNextRegId++; }

    /// Reset all registers
    inline void reset() { mNextRegId = 0; }

    [[nodiscard]] inline bool isRegisterUsed(RegId id) const { return id < mNextRegId; }

private:
    RegId mNextRegId = 0;
};

} // namespace PExpr::rvm
