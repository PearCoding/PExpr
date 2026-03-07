#include "SSCPFunctionInliner.h"
#include "SSAOptimizer.h"
#include "ssa/SSAContext.h"

#include <queue>
#include <ranges>

namespace PExpr::opt {
using namespace ssa;

void SSCPFunctionInliner::analyzeCallGraph(const SSAProgram& program)
{
    mCallCounts.clear();
    mRecursiveFunctions.clear();

    std::queue<std::string> mentionedFunctions;

    // Count all calls to functions
    auto countCallsInBody = [&](const InstructionList& body) {
        std::ranges::for_each(body, [&](const auto& instrPtr) {
            if (auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
                mentionedFunctions.push(call->FunctionName);
                ++mCallCounts[call->FunctionName];
            }
        });
    };

    // Count in main body
    countCallsInBody(program.Body);

    // Go over all mentioned functions
    // -> This works correctly with recursive functions
    std::unordered_set<std::string> handledFunctions;
    while (!mentionedFunctions.empty()) {
        std::string funcName = mentionedFunctions.front();
        mentionedFunctions.pop();

        // Check if we already checked the function?
        if (handledFunctions.contains(funcName))
            continue;
        handledFunctions.insert(funcName);

        // Count in the given function body
        for (const auto& func : program.Functions) {
            if (func.Name == funcName) {
                countCallsInBody(func.Body);
                break;
            }
        }
    }

    // Detect recursive functions
    detectRecursiveFunctions(program);
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
                    // Skip force inlining for recursive functions to avoid infinite recursion
                    if (mOptions.ForceInlineFunctions && !mRecursiveFunctions.contains(func.Name)) {
                        if (tryBasicInlining(ctx, call, func, body, i)) {
                            changed = true;
                            break;
                        }
                    } else if (mOptions.InlineFunctions) {
                        // Try optimized inlining (with constant folding and simplification check)
                        if (tryOptimizedInlining(ctx, call, func, body, i)) {
                            changed = true;
                            break;
                        }
                        // Fall back to basic inlining for single-call functions
                        else if (mCallCounts[func.Name] == 1 && tryBasicInlining(ctx, call, func, body, i)) {
                            changed = true;
                            break;
                        }
                    }
                    
                    // Always try intrinsic inlining regardless of options
                    if (tryInlineIntrinsic(call, func, body, i)) {
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
        valueMap[func.Parameters[i].name()] = call->Arguments[i];

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
        SSAOptimizer::Run(mOptions, outInlinedBody);

    PEXPR_ASSERT(dynamic_cast<const SSAInstrReturn*>(outInlinedBody.back().get()) != nullptr, "Expected the last entry to be a return statement");

    // Replace the return statement and assign the return value of it to the target
    auto assign           = std::make_shared<SSAInstrAssign>();
    assign->Target        = call->Target;
    assign->Operator      = SSAInstrAssign::OpKind::Assign;
    assign->Operands      = { dynamic_cast<const SSAInstrReturn*>(outInlinedBody.back().get())->Value };
    outInlinedBody.back() = std::move(assign);
}

bool SSCPFunctionInliner::tryBasicInlining(SSAContext* ctx, SSAInstrCall* call, const SSAFunction& func, InstructionList& instructions, size_t callIndex)
{
    if (func.External)
        return false;

    InstructionList inlinedInstructions;

    // Use common helper to clone and map function body (no optimization)
    cloneAndMapFunctionBody(ctx, func, call, inlinedInstructions, false);

    // Replace the call with the inlined instructions
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

bool SSCPFunctionInliner::isSimplerAfterOptimization(const InstructionList& originalBody, const InstructionList& inlinedBody) const
{
    const size_t originalEffective = originalBody.size();
    const size_t inlinedEffective  = inlinedBody.size();

    // Significant size reduction
    return inlinedEffective <= 2 || inlinedEffective < originalEffective / 2;
}

bool SSCPFunctionInliner::tryOptimizedInlining(SSAContext* ctx, SSAInstrCall* call, const SSAFunction& func, InstructionList& instructions, size_t callIndex)
{
    if (func.External)
        return false;

    // Check if all arguments are constants (required for optimization to be effective)
    for (const auto& arg : call->Arguments) {
        if (!arg.isConstant())
            return false;
    }

    InstructionList inlinedInstructions;

    // Clone and optimize the function body with argument substitution
    cloneAndMapFunctionBody(ctx, func, call, inlinedInstructions, true);

    // Check if inlining was beneficial
    if (inlinedInstructions.empty() || !isSimplerAfterOptimization(func.Body, inlinedInstructions))
        return false;

    // For recursive functions: only inline if the result has NO recursive calls
    if (mRecursiveFunctions.contains(func.Name)) {
        if (containsCallToRecursiveFunction(inlinedInstructions))
            return false; // Recursion not eliminated, reject inlining
    }

    // Replace the call with the optimized inlined instructions
    const auto prevCallIt = instructions.erase(instructions.begin() + callIndex);
    instructions.insert(prevCallIt, inlinedInstructions.begin(), inlinedInstructions.end());

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

bool SSCPFunctionInliner::containsCallToRecursiveFunction(const InstructionList& body) const
{
    for (const auto& instrPtr : body) {
        if (!instrPtr)
            continue;
        if (auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
            if (mRecursiveFunctions.contains(call->FunctionName))
                return true;
        }
    }
    return false;
}

void SSCPFunctionInliner::detectRecursiveFunctions(const ssa::SSAProgram& program)
{
    // Build adjacency list: function name -> set of called functions
    std::unordered_map<std::string, std::unordered_set<std::string>> callGraph;
    std::unordered_map<std::string, const SSAFunction*> functionMap;

    // Map function names to their bodies
    for (const auto& func : program.Functions)
        functionMap[func.Name] = &func;

    // Build call graph
    auto collectCalls = [&](const InstructionList& body, const std::string& caller) {
        for (const auto& instrPtr : body) {
            if (auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
                // Only track calls to functions defined in this program (not external)
                if (functionMap.contains(call->FunctionName))
                    callGraph[caller].insert(call->FunctionName);
            }
        }
    };

    // Collect calls from main body
    callGraph["<main>"] = {};
    collectCalls(program.Body, "<main>");

    // Collect calls from each function
    for (const auto& func : program.Functions) {
        callGraph[func.Name] = {};
        collectCalls(func.Body, func.Name);
    }

    // Detect cycles using DFS with color marking and stack
    std::unordered_map<std::string, int> color; // 0 = unvisited, 1 = visiting, 2 = visited
    std::vector<std::string> stack;
    std::unordered_map<std::string, size_t> stackIndex;

    std::function<void(const std::string&)> dfs = [&](const std::string& node) {
        color[node]      = 1; // visiting
        stackIndex[node] = stack.size();
        stack.push_back(node);

        for (const auto& neighbor : callGraph[node]) {
            if (color[neighbor] == 0) {
                dfs(neighbor);
            } else if (color[neighbor] == 1) {
                // Found a cycle: neighbor is currently being visited
                // Mark all nodes in the cycle from the neighbor's position to the end of stack
                size_t startIdx = stackIndex[neighbor];
                for (size_t i = startIdx; i < stack.size(); ++i)
                    mRecursiveFunctions.insert(stack[i]);

                // Also mark the neighbor itself
                mRecursiveFunctions.insert(neighbor);
            }
        }

        color[node] = 2; // visited
        stack.pop_back();
        stackIndex.erase(node);
    };

    // Run DFS on all nodes
    for (const auto& [node, _] : callGraph) {
        if (color[node] == 0)
            dfs(node);
    }
}
} // namespace PExpr::opt
