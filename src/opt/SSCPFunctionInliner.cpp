#include "SSCPFunctionInliner.h"
#include "Optimizer.h"
#include "ssa/SSAContext.h"

#include <ranges>

namespace PExpr::opt {
using namespace ssa;

void SSCPFunctionInliner::analyzeCallGraph(const SSAProgram& program)
{
    mCallCounts.clear();

    // Count all calls to functions
    auto countCallsInBody = [&](const InstructionList& body) {
        std::ranges::for_each(body, [this](const auto& instrPtr) {
            if (auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get()))
                ++mCallCounts[call->FunctionName];
        });
    };

    // Count in main body
    countCallsInBody(program.Body);

    // Count in function bodies
    for (const auto& func : program.Functions)
        countCallsInBody(func.Body);
}

bool SSCPFunctionInliner::attempFunctionInlining(SSAContext* ctx, SSAProgram& program, SSAFunction& func)
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
                    if (attemptAdvancedInlining(ctx, call, func, body, i)) { //< Try advanced inlining first
                        changed = true;
                        break;
                    } else if (mCallCounts[func.Name] == 1 && inlineFunctionCall(ctx, call, func, body, i)) { //< Fall back to basic inlining for single-call functions
                        changed = true;
                        break;
                    } else if (tryInlineIntrinsic(call, func, body, i)) {
                        changed = true;
                        break;
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

void SSCPFunctionInliner::cloneAndMapFunctionBody(SSAContext* ctx, const SSAFunction& func, const SSAInstrCall* call,
                                                  InstructionList& outInlinedBody, bool runOptimization)
{
    PEXPR_ASSERT(func.Parameters.size() == call->Arguments.size(), "Call parameters must match function parameters");

    // Create mappings
    std::unordered_map<std::string, SSAValue> valueMap;    // Old variable names -> new values
    std::unordered_map<std::string, std::string> labelMap; // Old label names -> new label names

    // Map parameters to arguments
    for (size_t i = 0; i < func.Parameters.size(); ++i)
        valueMap[func.Parameters[i]] = call->Arguments[i];

    // Lambda to map and rename a value, generating fresh names for non-parameter variables
    auto mapAndRenameValue = [&](SSAValue& val) {
        if (val.isConstant())
            return; // Constants don't need renaming

        // Check if already in map
        if (const auto it = valueMap.find(val.name()); it != valueMap.end()) {
            val = it->second;
        } else {
            // This is a new variable from the function body - generate a fresh name to avoid clashes
            SSAValue renamed     = SSAValue::Named(ctx->fresh(val.baseName()), val.type());
            valueMap[val.name()] = renamed;
            val                  = renamed;
        }
    };

    outInlinedBody.clear();
    outInlinedBody.reserve(func.Body.size());

    // First pass: build label mapping
    for (const auto& instrPtr : func.Body) {
        if (!instrPtr)
            continue;

        // Generate fresh label name and store mapping
        if (auto label = dynamic_cast<const SSAInstrLabel*>(instrPtr.get()))
            labelMap[label->Name] = ctx->fresh("lbl");
    }

    // Second pass: clone instructions
    for (const auto& instrPtr : func.Body) {
        if (!instrPtr)
            continue;

        // Clone instruction
        std::shared_ptr<SSAInstr> cloned;

        if (auto asg = dynamic_cast<const SSAInstrAssign*>(instrPtr.get())) {
            cloned = std::make_shared<SSAInstrAssign>(*asg);
        } else if (auto callInstr = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
            cloned = std::make_shared<SSAInstrCall>(*callInstr);
        } else if (auto br = dynamic_cast<const SSAInstrBranch*>(instrPtr.get())) {
            auto newBr = std::make_shared<SSAInstrBranch>(*br);
            // Remap target label
            if (const auto labelIt = labelMap.find(br->TargetLabel); labelIt != labelMap.end())
                newBr->TargetLabel = labelIt->second;
            cloned = std::move(newBr);
        } else if (auto phi = dynamic_cast<const SSAInstrPhi*>(instrPtr.get())) {
            cloned = std::make_shared<SSAInstrPhi>(*phi);
        } else if (auto label = dynamic_cast<const SSAInstrLabel*>(instrPtr.get())) {
            auto newLabel  = std::make_shared<SSAInstrLabel>();
            newLabel->Name = labelMap.at(label->Name); //< It must exist or the previous pass did fail horribly
            cloned         = std::move(newLabel);
        } else if (auto gotoInstr = dynamic_cast<const SSAInstrGoto*>(instrPtr.get())) {
            auto newGoto = std::make_shared<SSAInstrGoto>(*gotoInstr);
            // Remap target label
            if (const auto labelIt = labelMap.find(gotoInstr->TargetLabel); labelIt != labelMap.end())
                newGoto->TargetLabel = labelIt->second;
            cloned = std::move(newGoto);
        } else if (auto ret = dynamic_cast<const SSAInstrReturn*>(instrPtr.get())) {
            cloned = std::make_shared<SSAInstrReturn>(*ret);
        } else {
            PEXPR_ASSERT(false, "Unhandled SSAInstr type in cloneAndMapFunctionBody");
            continue;
        }

        // Use forEachValue to map and rename all variables
        cloned->forEachValue(mapAndRenameValue);

        outInlinedBody.push_back(cloned);
    }

    // Apply all the optimization possible on instructions
    if (runOptimization)
        Optimizer::Run(mOptions, outInlinedBody);

    PEXPR_ASSERT(dynamic_cast<const SSAInstrReturn*>(outInlinedBody.back().get()) != nullptr, "Expected the last entry to be a return statement");

    // Replace the return statement and assign the return value of it to the target
    auto assign           = std::make_shared<SSAInstrAssign>();
    assign->Target        = call->Target;
    assign->Operator      = SSAInstrAssign::OpKind::Assign;
    assign->Operands      = { dynamic_cast<const SSAInstrReturn*>(outInlinedBody.back().get())->Value };
    outInlinedBody.back() = std::move(assign);
}

bool SSCPFunctionInliner::inlineFunctionCall(SSAContext* ctx, SSAInstrCall* call, SSAFunction& func, InstructionList& instructions, size_t callIndex)
{
    if (func.External)
        return false;

    InstructionList inlinedInstructions;

    // Use common helper to clone and map function body
    cloneAndMapFunctionBody(ctx, func, call, inlinedInstructions, false);

    // Replace the call with the optimized inlined instructions
    instructions.erase(instructions.begin() + callIndex);
    instructions.insert(instructions.begin() + callIndex, inlinedInstructions.begin(), inlinedInstructions.end());

    return true;
}

bool SSCPFunctionInliner::removeUnusedFunctions(SSAProgram& program)
{
    bool changed = false;
    // Remove functions that are never called
    auto it = program.Functions.begin();
    while (it != program.Functions.end()) {
        auto callCountIt = mCallCounts.find(it->Name);
        if (callCountIt == mCallCounts.end() || callCountIt->second == 0) {
            changed = true;
            it      = program.Functions.erase(it);
        } else {
            ++it;
        }
    }

    return changed;
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
        if (!arg.isConstant())
            return false;
    }

    return true;
}

bool SSCPFunctionInliner::isSimplerAfterOptimization(const InstructionList& originalBody, const InstructionList& inlinedBody)
{
    const size_t originalEffective = originalBody.size();
    const size_t inlinedEffective  = inlinedBody.size();

    // Significant size reduction
    return inlinedEffective <= 2 || inlinedEffective < originalEffective / 2;
}

bool SSCPFunctionInliner::attemptAdvancedInlining(SSAContext* ctx, SSAInstrCall* call, SSAFunction& func, InstructionList& instructions, size_t callIndex)
{
    if (!shouldInlineFunctionCall(call, func))
        return false;

    auto& attemptInfo = mInlineAttempts[func.Name];
    attemptInfo.attempts++;

    InstructionList inlinedInstructions;

    // Use common helper to clone and map function body (keep return instruction for optimization)
    cloneAndMapFunctionBody(ctx, func, call, inlinedInstructions, true);

    // Check if optimization resulted in something simpler
    if (inlinedInstructions.empty() || !isSimplerAfterOptimization(func.Body, inlinedInstructions)) {
        // Inlining didn't help, reject it
        attemptInfo.failed = true;
        return false;
    }

    // Replace the call with the optimized inlined instructions
    const auto prevCallIt = instructions.erase(instructions.begin() + callIndex);
    instructions.insert(prevCallIt, inlinedInstructions.begin(), inlinedInstructions.end());

    attemptInfo.succeeded = true;
    return true;
}

bool SSCPFunctionInliner::tryInlineIntrinsic(SSAInstrCall* call, const SSAFunction& func, InstructionList& instructions, size_t callIndex)
{
    // Only external functions can be intrinsics
    if (!func.External)
        return false;

    // All must be constant
    for (const auto& val : call->Arguments) {
        if (!val.isConstant())
            return false;
    }

    const auto bound = mIntrinsics.equal_range(call->FunctionName);
    for (auto it = bound.first; it != bound.second; ++it) {
        if (!it->second.Definition.isExtern())
            continue;

        // Do we even have enough parameters?
        const auto& params = it->second.Definition.parameters();
        if (params.size() != call->Arguments.size())
            continue;

        std::vector<ValueVariant> args;
        args.reserve(params.size());

        // Check if the parameter types match
        bool isEqual = true;
        for (size_t i = 0; i < params.size(); ++i) {
            if (params[i]->type() != call->Arguments[i].type()) {
                isEqual = false;
                break;
            }

            args.push_back(call->Arguments[i].rawValue());
        }
        if (!isEqual)
            continue;

        // We found it!
        const auto constant = it->second.Callback(args);
        if (!constant.has_value())
            continue;

        // Check if the expected return type and the type inside the variant match
        const auto expectedType = it->second.Definition.returnType();
        const auto valueType    = type::Type::FromVariant(*constant);
        if (expectedType != valueType) // Maybe implicit compatible?
            continue;

        // Replace the return statement and assign the return value of it to the target
        auto value = SSAValue(true, it->second.Definition.returnType(), constant.value());

        auto assign             = std::make_shared<SSAInstrAssign>();
        assign->Target          = call->Target;
        assign->Operator        = SSAInstrAssign::OpKind::Assign;
        assign->Operands        = { value };
        instructions[callIndex] = std::move(assign);

        return true;
    }

    return false;
}
} // namespace PExpr::opt
