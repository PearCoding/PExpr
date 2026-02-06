#include "SSATupleDissolvePass.h"
#include "SSCPDeadCodeOptimizer.h"
#include "SSCPSideEffectAnalyzer.h"
#include "ssa/SSAInstruction.h"

#include <algorithm>

namespace PExpr::opt {
using namespace ssa;

bool SSATupleDissolvePass::dissolve(SSAContext* context, SSAProgram& program)
{
    opt::SSCPSideEffectAnalyzer sideEffects;
    sideEffects.propagateSideEffects(program);

    bool changed = false;

    // Repeat dissolve and dead-code analysis until no further changes
    auto handleInstructions = [&](InstructionList& instructions) {
        while (true) {
            bool changedBlock = dissolveInstructions(context, instructions);

            changed |= changedBlock;
            if (!changedBlock)
                break;
        };
    };

    // Dissolve tuples in main body
    handleInstructions(program.Body);

    // Dissolve tuples in all functions
    for (auto& func : program.Functions)
        handleInstructions(func.Body);

    return changed;
}

bool SSATupleDissolvePass::dissolveInstructions(SSAContext* context, InstructionList& instructions)
{
    // Note: We keep the original instructions (except for some rare cases) and rely on dead-code analysis to remove them.
    // If calls or returns have no use for it, it will be cleaned up by dead-code analysis.

    InstructionList newInstructions;
    newInstructions.reserve(instructions.size() * 2); // Estimate

    bool changed = false;

    for (const auto& instrPtr : instructions) {
        // Handle SSAInstrAssign
        if (auto assign = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
            if (mTupleElements.contains(assign->Target.hash())) {
                // Already handled in a previous pass, keep at it is
                newInstructions.push_back(instrPtr);
                continue;
            }

            // Case 1: Tuple creation -> Create multiple assignments
            if (assign->Operator == SSAInstrAssign::OpKind::Tuple) {
                const auto& operands         = assign->Operands;
                const auto& target           = assign->Target;
                const auto& targetComponents = target.type().components();

                // Construct multiple assignments instead of the tuple instruction
                std::vector<SSAValue> resultElements;
                resultElements.reserve(targetComponents.size());
                for (size_t i = 0; i < targetComponents.size(); ++i) {
                    SSAValue elemTarget = SSAValue::Named(context->fresh("%"), targetComponents[i]);
                    auto newAssign      = std::make_shared<SSAInstrAssign>();
                    newAssign->Target   = elemTarget;
                    newAssign->Operator = SSAInstrAssign::OpKind::Assign;
                    newAssign->Operands = { operands.at(i) };
                    newInstructions.push_back(newAssign);
                    resultElements.push_back(elemTarget);
                }

                changed = true;

                mTupleElements[target.hash()] = std::move(resultElements);
            }

            // Case 2: Access on tuple - resolve to direct value
            if (assign->Operator == SSAInstrAssign::OpKind::Access && assign->Operands.size() == 2) {
                const auto& tupleVal = assign->Operands[0];
                const auto& indexVal = assign->Operands[1];

                if (tupleVal.type().isTuple() && indexVal.isConstant()) {
                    size_t index = static_cast<size_t>(indexVal.valueAs<Integer>());

                    auto it = mTupleElements.find(tupleVal.hash());
                    if (it != mTupleElements.end() && index < it->second.size()) {
                        const auto& element = it->second[index];

                        // Replace the access with an assignment from the element
                        auto newAssign      = std::make_shared<SSAInstrAssign>();
                        newAssign->Target   = assign->Target;
                        newAssign->Operator = SSAInstrAssign::OpKind::Assign;
                        newAssign->Operands = { element };
                        newInstructions.push_back(newAssign);

                        changed = true;

                        // Don't emit the access instruction itself
                        continue;
                    }
                } else {
                    PEXPR_ASSERT(false, "Invalid access instruction");
                }
            }

            // Case 3: Unary operation with tuple operand
            if (assign->Operator == SSAInstrAssign::OpKind::Unary
                && assign->Operands.size() == 1 && assign->Operands[0].type().isTuple()) {
                const auto& sourceVal = assign->Operands[0];
                auto it               = mTupleElements.find(sourceVal.hash());
                if (it != mTupleElements.end()) {
                    const auto& sourceElements   = it->second;
                    const auto& targetType       = assign->Target.type();
                    const auto& targetComponents = targetType.components();

                    std::vector<SSAValue> resultElements;
                    resultElements.reserve(sourceElements.size());

                    for (size_t i = 0; i < sourceElements.size(); ++i) {
                        SSAValue elemTarget = SSAValue::Named(context->fresh("%"), targetComponents[i]);
                        auto elemInstr      = std::make_shared<SSAInstrAssign>();
                        elemInstr->Target   = elemTarget;
                        elemInstr->Operator = SSAInstrAssign::OpKind::Unary;
                        elemInstr->UnaryOp  = assign->UnaryOp;
                        elemInstr->Operands = { sourceElements[i] };
                        newInstructions.push_back(elemInstr);
                        resultElements.push_back(elemTarget);
                    }

                    changed = true;

                    // Store the dissolved tuple
                    mTupleElements[assign->Target.hash()] = std::move(resultElements);
                }
            }

            // Case 4: Binary operation with tuple operands
            if (assign->Operator == SSAInstrAssign::OpKind::Binary && assign->Operands.size() == 2) {
                const auto& leftVal  = assign->Operands[0];
                const auto& rightVal = assign->Operands[1];
                const auto& retVal   = assign->Target;

                if (leftVal.type().isTuple() && rightVal.type().isTuple()) { //< Tuple = Tuple op Tuple
                    auto leftIt  = mTupleElements.find(leftVal.hash());
                    auto rightIt = mTupleElements.find(rightVal.hash());

                    if (leftIt != mTupleElements.end() && rightIt != mTupleElements.end()) {
                        const auto& leftElements     = leftIt->second;
                        const auto& rightElements    = rightIt->second;
                        const auto& targetType       = assign->Target.type();
                        const auto& targetComponents = targetType.components();

                        std::vector<SSAValue> resultElements;
                        resultElements.reserve(targetComponents.size());

                        for (size_t i = 0; i < targetComponents.size(); ++i) {
                            SSAValue elemTarget = SSAValue::Named(context->fresh("%"), targetComponents.at(i));
                            auto elemInstr      = std::make_shared<SSAInstrAssign>();
                            elemInstr->Target   = elemTarget;
                            elemInstr->Operator = SSAInstrAssign::OpKind::Binary;
                            elemInstr->BinaryOp = assign->BinaryOp;
                            elemInstr->Operands = { leftElements.at(i), rightElements.at(i) };
                            newInstructions.push_back(elemInstr);
                            resultElements.push_back(elemTarget);
                        }

                        changed = true;

                        // Store the dissolved tuple
                        mTupleElements[assign->Target.hash()] = std::move(resultElements);
                    }
                } else if (!leftVal.type().isTuple() && rightVal.type().isTuple()) { //< Tuple = Scalar op Tuple
                    auto rightIt = mTupleElements.find(rightVal.hash());

                    if (rightIt != mTupleElements.end()) {
                        const auto& rightElements    = rightIt->second;
                        const auto& targetType       = assign->Target.type();
                        const auto& targetComponents = targetType.components();

                        std::vector<SSAValue> resultElements;
                        resultElements.reserve(targetComponents.size());

                        for (size_t i = 0; i < targetComponents.size(); ++i) {
                            SSAValue elemTarget = SSAValue::Named(context->fresh("%"), targetComponents.at(i));
                            auto elemInstr      = std::make_shared<SSAInstrAssign>();
                            elemInstr->Target   = elemTarget;
                            elemInstr->Operator = SSAInstrAssign::OpKind::Binary;
                            elemInstr->BinaryOp = assign->BinaryOp;
                            elemInstr->Operands = { leftVal, rightElements.at(i) };
                            newInstructions.push_back(elemInstr);
                            resultElements.push_back(elemTarget);
                        }

                        changed = true;

                        // Store the dissolved tuple
                        mTupleElements[assign->Target.hash()] = std::move(resultElements);
                    }
                } else if (leftVal.type().isTuple() && !rightVal.type().isTuple()) { //< Tuple = Tuple op Scalar
                    auto leftIt = mTupleElements.find(leftVal.hash());

                    if (leftIt != mTupleElements.end()) {
                        const auto& leftElements     = leftIt->second;
                        const auto& targetType       = assign->Target.type();
                        const auto& targetComponents = targetType.components();

                        std::vector<SSAValue> resultElements;
                        resultElements.reserve(targetComponents.size());

                        for (size_t i = 0; i < targetComponents.size(); ++i) {
                            SSAValue elemTarget = SSAValue::Named(context->fresh("%"), targetComponents.at(i));
                            auto elemInstr      = std::make_shared<SSAInstrAssign>();
                            elemInstr->Target   = elemTarget;
                            elemInstr->Operator = SSAInstrAssign::OpKind::Binary;
                            elemInstr->BinaryOp = assign->BinaryOp;
                            elemInstr->Operands = { leftElements.at(i), rightVal };
                            newInstructions.push_back(elemInstr);
                            resultElements.push_back(elemTarget);
                        }

                        changed = true;

                        // Store the dissolved tuple
                        mTupleElements[assign->Target.hash()] = std::move(resultElements);
                    }
                } else if (!retVal.type().isTuple() && !leftVal.type().isTuple() && !rightVal.type().isTuple()) { //< Scalar = Scalar op Scalar
                    // Ignore
                } else {
                    PEXPR_ASSERT(false, "Ill-configured binary SSA instruction detected");
                }
            }

            // Case 6: Cast operation with tuple source
            if (assign->Operator == SSAInstrAssign::OpKind::Cast
                && assign->Operands.size() == 1 && assign->Operands[0].type().isTuple()) {
                const auto& sourceVal = assign->Operands[0];
                auto it               = mTupleElements.find(sourceVal.hash());
                if (it != mTupleElements.end()) {
                    const auto& sourceElements   = it->second;
                    const auto& targetType       = assign->Target.type();
                    const auto& targetComponents = targetType.components();

                    std::vector<SSAValue> castedElements;
                    castedElements.reserve(sourceElements.size());

                    for (size_t i = 0; i < sourceElements.size(); ++i) {
                        const auto& elemType = targetComponents[i];

                        if (!(sourceElements[i].type() == elemType)) {
                            // Need to cast this element
                            SSAValue castedElem = SSAValue::Named(context->fresh("%"), elemType);
                            auto castInstr      = std::make_shared<SSAInstrAssign>();
                            castInstr->Target   = castedElem;
                            castInstr->Operator = SSAInstrAssign::OpKind::Cast;
                            castInstr->Operands = { sourceElements[i] };
                            newInstructions.push_back(castInstr);
                            castedElements.push_back(castedElem);
                        } else {
                            castedElements.push_back(sourceElements[i]);
                        }
                    }

                    changed = true;

                    // Store the dissolved tuple
                    mTupleElements[assign->Target.hash()] = castedElements;
                }
            }

            // Case 7: Assign with tuple source
            if (assign->Operator == SSAInstrAssign::OpKind::Assign
                && assign->Operands.size() == 1 && assign->Operands[0].type().isTuple()) {
                const auto& sourceVal = assign->Operands[0];
                auto it               = mTupleElements.find(sourceVal.hash());
                if (it != mTupleElements.end()) {
                    const auto& sourceElements   = it->second;
                    const auto& targetType       = assign->Target.type();
                    const auto& targetComponents = targetType.components();

                    std::vector<SSAValue> resultElements;
                    resultElements.reserve(sourceElements.size());

                    for (size_t i = 0; i < sourceElements.size(); ++i) {
                        SSAValue elemTarget = SSAValue::Named(context->fresh("%"), targetComponents.at(i));
                        auto elemInstr      = std::make_shared<SSAInstrAssign>();
                        elemInstr->Target   = elemTarget;
                        elemInstr->Operator = SSAInstrAssign::OpKind::Unary;
                        elemInstr->UnaryOp  = assign->UnaryOp;
                        elemInstr->Operands = { sourceElements[i] };
                        newInstructions.push_back(elemInstr);
                        resultElements.push_back(elemTarget);
                    }

                    changed = true;

                    // Store the dissolved tuple
                    mTupleElements[assign->Target.hash()] = resultElements;
                }
            }
        }

        // Handle SSAInstrPhi with tuple types
        if (auto phi = dynamic_cast<SSAInstrPhi*>(instrPtr.get())) {
            if (!mTupleElements.contains(phi->Target.hash()) && phi->Target.type().isTuple()) {
                const auto& targetType   = phi->Target.type();
                const size_t numElements = targetType.components().size();

                // Create phi nodes for each element
                std::vector<SSAValue> targetElements;
                targetElements.reserve(numElements);

                for (size_t elemIdx = 0; elemIdx < numElements; ++elemIdx) {
                    const auto& elemType = targetType.components()[elemIdx];
                    SSAValue elemTarget  = SSAValue::Named(context->fresh("%"), elemType);

                    auto elemPhi        = std::make_shared<SSAInstrPhi>();
                    elemPhi->Target     = elemTarget;
                    elemPhi->Conditions = phi->Conditions;

                    // Extract element from each branch
                    elemPhi->Branches.reserve(phi->Branches.size());
                    for (const auto& branchVal : phi->Branches) {
                        if (branchVal.type().isTuple()) {
                            auto it = mTupleElements.find(branchVal.hash());
                            if (it != mTupleElements.end() && elemIdx < it->second.size()) {
                                elemPhi->Branches.push_back(it->second[elemIdx]);
                            } else {
                                // Shouldn't happen with proper dissolution
                                elemPhi->Branches.push_back(branchVal);
                            }
                        } else {
                            // Shouldn't happen - all branches should be tuples
                            elemPhi->Branches.push_back(branchVal);
                        }
                    }

                    newInstructions.push_back(elemPhi);
                    targetElements.push_back(elemTarget);
                }

                changed = true;

                // Store the dissolved tuple
                mTupleElements[phi->Target.hash()] = targetElements;
            }
        }

        // Handle SSAInstrCall with tuple arguments or return values
        if (dynamic_cast<SSAInstrCall*>(instrPtr.get())) {
            // For calls, we have to keep the tuples. This can not be solved at this stage
        }

        // Handle SSAInstrReturn with tuple value
        if (dynamic_cast<SSAInstrReturn*>(instrPtr.get())) {
            // For returns, we have to keep the tuples. This can not be solved at this stage
        }

        // Keep instruction as-is
        newInstructions.push_back(instrPtr);
    }

    instructions = std::move(newInstructions);
    return changed;
}

} // namespace PExpr::opt
