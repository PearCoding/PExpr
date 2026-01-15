#pragma once

#include "SSAMapper.h"

#include <unordered_set>

namespace PExpr::ssa {

class SSCPSideEffectAnalyzer {
public:
    void propagateSideEffects(const SSAProgram& program);
    bool functionHasSideEffects(const std::string& functionName) const;
    const std::unordered_set<std::string>& getSideEffectFunctions() const { return mSideEffectFunctions; }
    
private:
    std::unordered_set<std::string> mSideEffectFunctions;
};

} // namespace PExpr::ssa
