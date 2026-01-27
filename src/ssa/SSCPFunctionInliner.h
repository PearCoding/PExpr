#pragma once

#include "SSAMapper.h"
#include "SSAOptions.h"

#include <unordered_map>
#include <unordered_set>

namespace PExpr::ssa {

using SSAIntrinsicInlineCallback = std::function<std::optional<ValueVariant>(const std::vector<ValueVariant>& args)>;

class SSCPFunctionInliner {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    inline explicit SSCPFunctionInliner(const SSAOptions& opts)
        : mOptions(opts)
    {
    }

    void analyzeCallGraph(const SSAProgram& program);
    [[nodiscard]] bool attempFunctionInlining(SSAContext* ctx, SSAProgram& program, SSAFunction& func);
    [[nodiscard]] bool removeUnusedFunctions(SSAProgram& program);

    inline void addIntrinsic(const type::FunctionDef& func, SSAIntrinsicInlineCallback callback)
    {
        mIntrinsics.insert({ func.mangledName(), FunctionInlinePair{ func, callback } });
    }

private:
    /// Common helper to clone and map function body with parameter substitution
    /// @oaram ctx SSA context
    /// @param func The function to inline
    /// @param call The call instruction
    /// @param outInlinedBody Output parameter for the cloned and mapped instructions
    /// @param runOptimization Apply optimization on this block only
    void cloneAndMapFunctionBody(SSAContext* ctx, const SSAFunction& func, const SSAInstrCall* call,
                                 InstructionList& outInlinedBody,
                                 bool runOptimization);

    [[nodiscard]] bool inlineFunctionCall(SSAContext* ctx, SSAInstrCall* call, SSAFunction& func, InstructionList& instructions, size_t callIndex);
    [[nodiscard]] bool shouldInlineFunctionCall(SSAInstrCall* call, SSAFunction& func);
    [[nodiscard]] bool attemptAdvancedInlining(SSAContext* ctx, SSAInstrCall* call, SSAFunction& func, InstructionList& instructions, size_t callIndex);
    [[nodiscard]] bool isSimplerAfterOptimization(const InstructionList& originalBody, const InstructionList& inlinedBody);

    [[nodiscard]] bool tryInlineIntrinsic(SSAInstrCall* call, const SSAFunction& func, InstructionList& instructions, size_t callIndex);

    const SSAOptions mOptions;
    std::unordered_map<std::string, int> mCallCounts;

    struct FunctionInlinePair {
        type::FunctionDef Definition;
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
