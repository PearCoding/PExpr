#pragma once

#include "ssa/SSAFunction.h"

#include <unordered_set>

namespace PExpr::opt {

class SSCPSideEffectAnalyzer {
public:
    void propagateSideEffects(const ssa::SSAProgram& program);
    bool functionHasSideEffects(const std::string& functionName) const;
    const std::unordered_set<std::string>& getSideEffectFunctions() const { return mSideEffectFunctions; }

private:
    std::unordered_set<std::string> mSideEffectFunctions;
};

} // namespace PExpr::opt
