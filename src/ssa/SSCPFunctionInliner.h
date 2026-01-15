#pragma once

#include "SSAMapper.h"

#include <unordered_map>
#include <unordered_set>

namespace PExpr::ssa {

class SSCPFunctionInliner {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;
    
    bool attempFunctionInlining(SSAProgram& program, SSAFunction& func);
    void analyzeCallGraph(const SSAProgram& program);
    bool inlineCallsToFunction(SSAProgram& program, SSAFunction& func);
    bool inlineFunctionCall(SSAInstrCall* call, SSAFunction& func, InstructionList& instructions, size_t callIndex);
    void removeUnusedFunctions(SSAProgram& program);
    
    bool shouldInlineFunctionCall(SSAInstrCall* call, SSAFunction& func);
    bool attemptAdvancedInlining(SSAInstrCall* call, SSAFunction& func, InstructionList& instructions, size_t callIndex);
    bool isSimplerAfterOptimization(const InstructionList& originalBody, const InstructionList& inlinedBody);
    
    const std::unordered_map<std::string, int>& getCallCounts() const { return mCallCounts; }
    
private:
    std::unordered_map<std::string, int> mCallCounts;
    
    // Advanced inlining state
    struct InlineAttemptInfo {
        int attempts = 0;
        bool succeeded = false;
        bool failed = false;
    };
    std::unordered_map<std::string, InlineAttemptInfo> mInlineAttempts;
    static constexpr int MAX_INLINE_ATTEMPTS = 16;
};

} // namespace PExpr::ssa
