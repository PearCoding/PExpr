#include "SSCPFunctionInliner.h"
#include "SSAMapper.h"

namespace PExpr::ssa {

bool SSCPFunctionInliner::attempFunctionInlining(SSAProgram& program, SSAFunction& func)
{
    // Check if function should be inlined
    if (const auto callCountIt = mCallCounts.find(func.Name);
        callCountIt != mCallCounts.end() && callCountIt->second == 1) {
        // Find and inline all calls to this function
        return inlineCallsToFunction(program, func);
    }
    return false;
}

void SSCPFunctionInliner::analyzeCallGraph(const SSAProgram& program)
{
    mCallCounts.clear();

    // Count all calls to functions
    auto countCallsInBody = [&](const InstructionList& body) {
        for (const auto& instrPtr : body) {
            if (!instrPtr)
                continue;
            if (auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get()))
                ++mCallCounts[call->FunctionName];
        }
    };

    // Count in main body
    countCallsInBody(program.Body);

    // Count in function bodies
    for (const auto& func : program.Functions)
        countCallsInBody(func.Body);
}

bool SSCPFunctionInliner::inlineCallsToFunction(SSAProgram& program, SSAFunction& func)
{
    bool inlinedAny = false;

    // Helper to inline calls in a body
    auto inlineInBody = [&](InstructionList& body) -> bool {
        bool changed = false;
        for (size_t i = 0; i < body.size(); ++i) {
            if (!body[i])
                continue;
            if (auto call = dynamic_cast<SSAInstrCall*>(body[i].get())) {
                if (call->FunctionName == func.Name) {
                    if (shouldInlineFunctionCall(call, func)) {
                        // Try advanced inlining first (for constant parameters)
                        if (attemptAdvancedInlining(call, func, body, i)) {
                            changed = true;
                            break;
                        }
                    } else if (mCallCounts[func.Name] == 1) {
                        // Fall back to basic inlining for single-call functions
                        if (inlineFunctionCall(call, func, body, i)) {
                            changed = true;
                            break;
                        }
                    }
                }
            }
        }
        return changed;
    };

    // Inline in main body
    if (inlineInBody(program.Body))
        inlinedAny = true;

    // Inline in other functions
    for (auto& otherFunc : program.Functions) {
        if (&otherFunc == &func)
            continue;
        if (inlineInBody(otherFunc.Body))
            inlinedAny = true;
    }

    return inlinedAny;
}

bool SSCPFunctionInliner::inlineFunctionCall(SSAInstrCall* call, SSAFunction& func, InstructionList& instructions, size_t callIndex)
{
    PEXPR_ASSERT(func.Parameters.size() == call->Arguments.size(), "Call parameters must match function parameters at this point");

    // Create a mapping from parameter names to argument values
    std::unordered_map<std::string, SSAValue> paramMap;
    for (size_t i = 0; i < func.Parameters.size(); ++i)
        paramMap[func.Parameters[i]] = call->Arguments[i];

    // Create instructions for the inlined body
    InstructionList inlinedInstructions;
    inlinedInstructions.reserve(func.Body.size());

    SSAValue returnValue;

    for (const auto& instrPtr : func.Body) {
        if (!instrPtr)
            continue;

        // Clone the instruction
        std::shared_ptr<SSAInstr> cloned;

        if (auto asg = dynamic_cast<const SSAInstrAssign*>(instrPtr.get())) {
            auto newAsg = std::make_shared<SSAInstrAssign>(*asg);

            // Map operand names (parameters to arguments)
            for (auto& op : newAsg->Operands) {
                if (op.Kind != SSAValue::Kind::Constant) {
                    if (auto paramIt = paramMap.find(op.Name); paramIt != paramMap.end())
                        op = paramIt->second;
                }
            }

            cloned = newAsg;
        } else if (auto ret = dynamic_cast<const SSAInstrReturn*>(instrPtr.get())) {
            returnValue = ret->Value;

            // Map the return value if needed
            if (returnValue.Kind != SSAValue::Kind::Constant) {
                if (auto paramIt = paramMap.find(returnValue.Name); paramIt != paramMap.end())
                    returnValue = paramIt->second;
            }

            // Don't add the return instruction to the inlined body
            continue;
        } else if (auto br = dynamic_cast<const SSAInstrBranch*>(instrPtr.get())) {
            auto newBr = std::make_shared<SSAInstrBranch>(*br);

            // Map condition
            if (newBr->Condition.Kind != SSAValue::Kind::Constant) {
                if (auto paramIt = paramMap.find(newBr->Condition.Name); paramIt != paramMap.end())
                    newBr->Condition = paramIt->second;
            }

            cloned = newBr;
        } else if (auto callInstr = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
            auto newCall = std::make_shared<SSAInstrCall>(*callInstr);

            // Map arguments
            for (auto& arg : newCall->Arguments) {
                if (arg.Kind != SSAValue::Kind::Constant) {
                    if (auto paramIt = paramMap.find(arg.Name); paramIt != paramMap.end())
                        arg = paramIt->second;
                }
            }

            cloned = newCall;
        } else if (auto phi = dynamic_cast<const SSAInstrPhi*>(instrPtr.get())) {
            auto newPhi = std::make_shared<SSAInstrPhi>(*phi);

            // Map conditions and branches
            for (auto& cond : newPhi->Conditions) {
                if (cond.Kind != SSAValue::Kind::Constant) {
                    if (auto paramIt = paramMap.find(cond.Name); paramIt != paramMap.end())
                        cond = paramIt->second;
                }
            }

            for (auto& branch : newPhi->Branches) {
                if (branch.Kind != SSAValue::Kind::Constant) {
                    if (auto paramIt = paramMap.find(branch.Name); paramIt != paramMap.end())
                        branch = paramIt->second;
                }
            }

            cloned = newPhi;
        } else {
            cloned = instrPtr;
        }

        if (cloned)
            inlinedInstructions.push_back(cloned);
    }

    // Replace the call with the inlined instructions
    // Create an assignment from the return value to the call's target

    auto assign      = std::make_shared<SSAInstrAssign>();
    assign->Target   = call->Target;
    assign->Operator = SSAInstrAssign::OpKind::Assign;
    assign->Operands = { returnValue };
    inlinedInstructions.push_back(assign);

    // Replace the call with inlined instructions
    instructions.erase(instructions.begin() + callIndex);
    instructions.insert(instructions.begin() + callIndex, inlinedInstructions.begin(), inlinedInstructions.end());

    return true;
}

void SSCPFunctionInliner::removeUnusedFunctions(SSAProgram& program)
{
    // Remove functions that are never called and are not external
    auto it = program.Functions.begin();
    while (it != program.Functions.end()) {
        if (it->External) {
            ++it;
            continue;
        }

        auto callCountIt = mCallCounts.find(it->Name);
        if (callCountIt == mCallCounts.end() || callCountIt->second == 0)
            it = program.Functions.erase(it);
        else
            ++it;
    }
}

bool SSCPFunctionInliner::shouldInlineFunctionCall(SSAInstrCall* call, SSAFunction& func)
{
    if (func.External)
        return false;

    // Check if we've already attempted inlining this function too many times
    auto& attemptInfo = mInlineAttempts[func.Name];
    if (attemptInfo.attempts >= MAX_INLINE_ATTEMPTS) {
        attemptInfo.failed = true;
        return false;
    }

    // Check if all arguments are constants
    for (const auto& arg : call->Arguments) {
        if (arg.Kind != SSAValue::Kind::Constant)
            return false;
    }

    return true;
}

bool SSCPFunctionInliner::isSimplerAfterOptimization(const InstructionList& originalBody, const InstructionList& inlinedBody)
{
    // TODO: Due to SSA value name clashes we can not simply inline larger blocks of code
    PEXPR_UNUSED(originalBody);
    return inlinedBody.size() == 1;
    // Count effective instructions (excluding labels, gotos that will be optimized)
    // size_t originalEffective = 0;
    // size_t inlinedEffective = 0;

    // for (const auto& instr : originalBody) {
    //     if (!instr) continue;
    //     if (dynamic_cast<const SSAInstrLabel*>(instr.get())) continue;
    //     if (dynamic_cast<const SSAInstrGoto*>(instr.get())) continue;
    //     ++originalEffective;
    // }

    // for (const auto& instr : inlinedBody) {
    //     if (!instr) continue;
    //     if (dynamic_cast<const SSAInstrLabel*>(instr.get())) continue;
    //     if (dynamic_cast<const SSAInstrGoto*>(instr.get())) continue;
    //     ++inlinedEffective;
    // }

    // // Significant size reduction
    // return inlinedEffective == 1 || inlinedEffective < originalEffective / 2;
}

bool SSCPFunctionInliner::attemptAdvancedInlining(SSAInstrCall* call, SSAFunction& func, InstructionList& instructions, size_t callIndex)
{
    auto& attemptInfo = mInlineAttempts[func.Name];
    attemptInfo.attempts++;

    // Create a mapping from parameter names to argument values
    std::unordered_map<std::string, SSAValue> paramMap;
    for (size_t i = 0; i < func.Parameters.size(); ++i)
        paramMap[func.Parameters[i]] = call->Arguments[i];

    // Create instructions for the inlined body
    InstructionList inlinedInstructions;
    inlinedInstructions.reserve(func.Body.size());

    SSAValue returnValue;

    for (const auto& instrPtr : func.Body) {
        if (!instrPtr)
            continue;

        // Clone the instruction
        std::shared_ptr<SSAInstr> cloned;

        if (auto asg = dynamic_cast<const SSAInstrAssign*>(instrPtr.get())) {
            auto newAsg = std::make_shared<SSAInstrAssign>(*asg);

            // Map operand names (parameters to arguments)
            for (auto& op : newAsg->Operands) {
                if (op.Kind != SSAValue::Kind::Constant) {
                    if (auto paramIt = paramMap.find(op.Name); paramIt != paramMap.end())
                        op = paramIt->second;
                }
            }

            cloned = newAsg;
        } else if (auto ret = dynamic_cast<const SSAInstrReturn*>(instrPtr.get())) {
            returnValue = ret->Value;

            // Map the return value if needed
            if (returnValue.Kind != SSAValue::Kind::Constant) {
                if (auto paramIt = paramMap.find(returnValue.Name); paramIt != paramMap.end())
                    returnValue = paramIt->second;
            }

            // Don't add the return instruction to the inlined body
            continue;
        } else if (auto br = dynamic_cast<const SSAInstrBranch*>(instrPtr.get())) {
            auto newBr = std::make_shared<SSAInstrBranch>(*br);

            // Map condition
            if (newBr->Condition.Kind != SSAValue::Kind::Constant) {
                if (auto paramIt = paramMap.find(newBr->Condition.Name); paramIt != paramMap.end())
                    newBr->Condition = paramIt->second;
            }

            cloned = newBr;
        } else if (auto callInstr = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
            auto newCall = std::make_shared<SSAInstrCall>(*callInstr);

            // Map arguments
            for (auto& arg : newCall->Arguments) {
                if (arg.Kind != SSAValue::Kind::Constant) {
                    if (auto paramIt = paramMap.find(arg.Name); paramIt != paramMap.end())
                        arg = paramIt->second;
                }
            }

            cloned = newCall;
        } else if (auto phi = dynamic_cast<const SSAInstrPhi*>(instrPtr.get())) {
            auto newPhi = std::make_shared<SSAInstrPhi>(*phi);

            // Map conditions and branches
            for (auto& cond : newPhi->Conditions) {
                if (cond.Kind != SSAValue::Kind::Constant) {
                    if (auto paramIt = paramMap.find(cond.Name); paramIt != paramMap.end())
                        cond = paramIt->second;
                }
            }

            for (auto& branch : newPhi->Branches) {
                if (branch.Kind != SSAValue::Kind::Constant) {
                    if (auto paramIt = paramMap.find(branch.Name); paramIt != paramMap.end())
                        branch = paramIt->second;
                }
            }

            cloned = newPhi;
        } else {
            cloned = instrPtr;
        }

        if (cloned)
            inlinedInstructions.push_back(cloned);
    }

    // Create an assignment from the return value to the call's target
    auto assign      = std::make_shared<SSAInstrAssign>();
    assign->Target   = call->Target;
    assign->Operator = SSAInstrAssign::OpKind::Assign;
    assign->Operands = { returnValue };
    inlinedInstructions.push_back(assign);

    // Note: We need to process the body here, but we don't have access to the full context
    // This will be handled by the main SSAPassSSCP::processBody call later
    // For now, we'll just check size

    // Check if optimization resulted in something simpler
    if (!isSimplerAfterOptimization(inlinedInstructions, inlinedInstructions)) {
        // Inlining didn't help, reject it
        attemptInfo.failed = true;
        return false;
    }

    // Replace the call with the optimized inlined instructions
    instructions.erase(instructions.begin() + callIndex);
    instructions.insert(instructions.begin() + callIndex,
                        inlinedInstructions.begin(), inlinedInstructions.end());

    attemptInfo.succeeded = true;
    return true;
}

} // namespace PExpr::ssa
