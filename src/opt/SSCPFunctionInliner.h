#pragma once

#include "OptimizerOptions.h"
#include "type/Definitions.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace PExpr::ssa {
class SSAContext;
class SSAInstr;
class SSAInstrCall;
class SSAFunction;
class SSAProgram;
} // namespace PExpr::ssa

namespace PExpr::opt {

using SSAIntrinsicInlineCallback = std::function<std::optional<ValueVariant>(const std::vector<ValueVariant>& args)>;

class SSCPFunctionInliner {
public:
    using InstructionList = std::vector<std::shared_ptr<ssa::SSAInstr>>;

    inline explicit SSCPFunctionInliner(const OptimizerOptions& opts)
        : mOptions(opts)
    {
    }

    void analyzeCallGraph(const ssa::SSAProgram& program);
    [[nodiscard]] bool attempFunctionInlining(ssa::SSAContext* ctx, ssa::SSAProgram& program, ssa::SSAFunction& func);
    [[nodiscard]] bool removeUnusedFunctions(ssa::SSAProgram& program);

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
    void cloneAndMapFunctionBody(ssa::SSAContext* ctx, const ssa::SSAFunction& func, const ssa::SSAInstrCall* call,
                                 InstructionList& outInlinedBody,
                                 bool runOptimization);

    [[nodiscard]] bool inlineFunctionCall(ssa::SSAContext* ctx, ssa::SSAInstrCall* call, ssa::SSAFunction& func, InstructionList& instructions, size_t callIndex);
    [[nodiscard]] bool shouldInlineFunctionCall(ssa::SSAInstrCall* call, ssa::SSAFunction& func);
    [[nodiscard]] bool attemptAdvancedInlining(ssa::SSAContext* ctx, ssa::SSAInstrCall* call, ssa::SSAFunction& func, InstructionList& instructions, size_t callIndex);
    [[nodiscard]] bool isSimplerAfterOptimization(const InstructionList& originalBody, const InstructionList& inlinedBody);

    [[nodiscard]] bool tryInlineIntrinsic(ssa::SSAInstrCall* call, const ssa::SSAFunction& func, InstructionList& instructions, size_t callIndex);

    const OptimizerOptions mOptions;
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

    // Recursive function detection
    std::unordered_set<std::string> mRecursiveFunctions;
    void detectRecursiveFunctions(const ssa::SSAProgram& program);
};

} // namespace PExpr::opt
