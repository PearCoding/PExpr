#include "SSCPTailCallOptimizer.h"
#include "ssa/SSAStructs.h"

#include <algorithm>

namespace PExpr::opt {
using namespace ssa;

bool SSCPTailCallOptimizer::optimizeTailCalls(SSAProgram& program)
{
    SSAContext ctx;
    ctx.analyze(program);

    bool changed = false;

    // Optimize main program body
    if (optimizeTailCalls(&ctx, program.Body, nullptr))
        changed = true;

    // Optimize all functions
    for (auto& func : program.Functions) {
        if (optimizeTailCalls(&ctx, func.Body, &func))
            changed = true;
    }

    return changed;
}

bool SSCPTailCallOptimizer::optimizeTailCalls(SSAContext* ctx, InstructionList& instructions, const SSAFunction* currentFunction)
{
    if (instructions.size() < 2)
        return false;

    bool changed = false;

    // We need to iterate through instructions looking for call-return patterns
    for (size_t i = 0; i < instructions.size() - 1; ++i) {
        auto& callInstr = instructions[i];
        auto& nextInstr = instructions[i + 1];

        if (!callInstr || !nextInstr)
            continue;

        // Check if this is a tail call pattern
        if (isTailCall(callInstr, nextInstr)) {
            auto* call = dynamic_cast<SSAInstrCall*>(callInstr.get());
            auto* ret  = dynamic_cast<SSAInstrReturn*>(nextInstr.get());

            if (!call || !ret)
                continue;

            // Check if this is a recursive tail call
            if (isRecursiveCall(call, currentFunction)) {
                if (transformTailRecursion(ctx, instructions, i, call, ret))
                    changed = true;
            } else {
                if (transformTailCall(ctx, instructions, i, call, ret))
                    changed = true;
            }
        }
    }

    return changed;
}

bool SSCPTailCallOptimizer::isTailCall(const std::shared_ptr<SSAInstr>& callInstr,
                                       const std::shared_ptr<SSAInstr>& nextInstr) const
{
    // A tail call is a function call immediately followed by a return
    // where the return value is the result of the call

    auto* call = dynamic_cast<const SSAInstrCall*>(callInstr.get());
    auto* ret  = dynamic_cast<const SSAInstrReturn*>(nextInstr.get());

    if (!call || !ret)
        return false;

    // The return should return the result of the call
    // In SSA, we need to check if the return value matches the call's target
    return ret->Value == call->Target;
}

bool SSCPTailCallOptimizer::isRecursiveCall(const SSAInstrCall* callInstr,
                                            const SSAFunction* currentFunction) const
{
    if (!callInstr || !currentFunction)
        return false;

    // Check if the call is to the same function
    return callInstr->FunctionName == currentFunction->Name;
}

bool SSCPTailCallOptimizer::transformTailRecursion(SSAContext* ctx,
                                                   InstructionList& instructions,
                                                   size_t callIndex,
                                                   SSAInstrCall* callInstr,
                                                   SSAInstrReturn* returnInstr)
{
    return false; // TODO: Not implemented yet

    // For tail recursion, we transform:
    //   target = call func(args...)
    //   return target
    // Into a loop structure

    // We need to create a loop label and update parameters

    // Create a unique label for the loop using SSAContext
    std::string loopLabel = ctx->fresh("tailrec_loop");
    std::string exitLabel = ctx->fresh("tailrec_exit");

    // Create new instructions to replace the call-return pattern
    std::vector<std::shared_ptr<SSAInstr>> newInstrs;

    // 1. Add loop label
    auto loopLabelInstr  = std::make_shared<SSAInstrLabel>();
    loopLabelInstr->Name = loopLabel;
    newInstrs.push_back(loopLabelInstr);

    // 2. Create parameter update instructions
    // TODO

    // 3. Create conditional branch to exit
    // TODO

    // 4. Create goto to loop start
    auto gotoInstr         = std::make_shared<SSAInstrGoto>();
    gotoInstr->TargetLabel = loopLabel;
    newInstrs.push_back(gotoInstr);

    // 5. Add exit label
    auto exitLabelInstr  = std::make_shared<SSAInstrLabel>();
    exitLabelInstr->Name = exitLabel;
    newInstrs.push_back(exitLabelInstr);

    // 6. Create return with the appropriate value
    // TODO

    // Replace the call-return instructions with our new loop
    instructions.erase(instructions.begin() + callIndex, instructions.begin() + callIndex + 2);
    instructions.insert(instructions.begin() + callIndex, newInstrs.begin(), newInstrs.end());

    return true;
}

bool SSCPTailCallOptimizer::transformTailCall(SSAContext* ctx,
                                              InstructionList& instructions,
                                              size_t callIndex,
                                              SSAInstrCall* callInstr,
                                              SSAInstrReturn* returnInstr)
{
    return false; // TODO: Not implemented yet
}

} // namespace PExpr::opt