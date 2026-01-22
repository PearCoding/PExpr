#pragma once

#include "SSAMapper.h"
#include "SSAOptions.h"

#include <unordered_map>
#include <unordered_set>

namespace PExpr::ssa {

using SSAIntrinsicInlineCallback = std::function<std::optional<ExtendedValueVariant>(const std::vector<ExtendedValueVariant>& args)>;

class SSCPFunctionInliner {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    inline explicit SSCPFunctionInliner(const SSAOptions& opts)
        : mOptions(opts)
    {
    }

    void analyzeCallGraph(const SSAProgram& program);
    bool attempFunctionInlining(SSAProgram& program, SSAFunction& func);
    void removeUnusedFunctions(SSAProgram& program);

    inline void addIntrinsic(const FunctionDef& func, SSAIntrinsicInlineCallback callback)
    {
        mIntrinsics.insert({ func.name(), FunctionInlinePair{ func, callback } });
    }

private:
    bool inlineFunctionCall(SSAInstrCall* call, SSAFunction& func, InstructionList& instructions, size_t callIndex);
    bool shouldInlineFunctionCall(SSAInstrCall* call, SSAFunction& func);
    bool attemptAdvancedInlining(SSAInstrCall* call, SSAFunction& func, InstructionList& instructions, size_t callIndex);
    bool isSimplerAfterOptimization(const InstructionList& originalBody, const InstructionList& inlinedBody);

    bool tryInlineIntrinsic(SSAInstrCall* call, const SSAFunction& func, InstructionList& instructions, size_t callIndex);

    const SSAOptions mOptions;
    std::unordered_map<std::string, int> mCallCounts;

    struct FunctionInlinePair {
        FunctionDef Definition;
        SSAIntrinsicInlineCallback Callback;
    };
    std::unordered_multimap<std::string, FunctionInlinePair> mIntrinsics;

    // Advanced inlining state
    struct InlineAttemptInfo {
        int attempts   = 0;
        bool succeeded = false;
        bool failed    = false;
    };
    std::unordered_map<std::string, InlineAttemptInfo> mInlineAttempts;
    static constexpr int MAX_INLINE_ATTEMPTS = 16;
};

} // namespace PExpr::ssa
