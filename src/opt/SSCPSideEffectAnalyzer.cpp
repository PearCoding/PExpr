#include "SSCPSideEffectAnalyzer.h"

namespace PExpr::opt {
using namespace ssa;

void SSCPSideEffectAnalyzer::propagateSideEffects(const SSAProgram& program)
{
    // Build helper maps: known functions and their parameter sets. 
    std::unordered_set<std::string> knownFunctions;
    mSideEffectFunctions.clear();

    for (const auto& f : program.Functions) {
        knownFunctions.insert(f.Name);
        if (f.External && f.HasSideEffect)
            mSideEffectFunctions.insert(f.Name);
    }

    // Propagate side-effects:
    // - Any function that calls an unknown function (not in knownFunctions) is side-effecting.
    // - Any function that calls a side-effecting function is side-effecting.
    bool progress = true;
    while (progress) {
        progress = false;
        for (const auto& f : program.Functions) {
            if (mSideEffectFunctions.find(f.Name) != mSideEffectFunctions.end())
                continue;
            // inspect body
            for (const auto& instrPtr : f.Body) {
                if (!instrPtr)
                    continue;
                if (auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
                    if (knownFunctions.find(call->FunctionName) == knownFunctions.end() || mSideEffectFunctions.find(call->FunctionName) != mSideEffectFunctions.end()) {
                        mSideEffectFunctions.insert(f.Name);
                        progress = true;
                        break;
                    }
                }
            }
        }
    }
}

bool SSCPSideEffectAnalyzer::functionHasSideEffects(const std::string& functionName) const
{
    return mSideEffectFunctions.find(functionName) != mSideEffectFunctions.end();
}

} // namespace PExpr::opt
