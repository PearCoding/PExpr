#include "RVMConstantOptimizer.h"
#include "RVMBasicBlockAnalyzer.h"
#include "RVMInstruction.h"
#include "RVMValue.h"
#include "utils/Reporter.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace PExpr::rvm {

bool RVMConstantOptimizer::optimize(const opt::OptimizerOptions& options, RVMProgram& program)
{
    // Early exit if constant propagation is not enabled
    if (!options.OptimizeConstantPropagation)
        return false;

    if (program.empty())
        return false;

    // Run constant propagation pass
    return runConstantPropagationPass(program);
}

bool RVMConstantOptimizer::runConstantPropagationPass(RVMProgram& program)
{
    // Split program into basic blocks
    auto blocks = RVMBasicBlockAnalyzer::splitIntoBlocks(program);
    if (blocks.empty())
        return false;

    bool changed = false;

    // Process each basic block independently
    for (auto& block : blocks) {
        // Map: RegId -> constant RVMValue within this block
        std::unordered_map<RegId, RVMValue> constMap;

        // Process instructions in the block
        for (size_t instrIdx = 0; instrIdx < block.size(); ++instrIdx) {
            auto& instr     = block[instrIdx];
            const Opcode op = instr->opcode();

            // Check if it's a MOV instruction with constant source
            if (auto* movInstr = dynamic_cast<RVMInstr2Op*>(instr.get())) {
                if (op == Opcode::MOV) {
                    const RVMValue& src = movInstr->source();
                    const RVMValue& dst = movInstr->destination();

                    // If source is a constant and destination is a register,
                    // record the constant mapping
                    if (src.isConstant() && dst.isRegister()) {
                        RegId dstReg     = dst.regId();
                        constMap[dstReg] = src;
                        // Continue to next instruction - we'll let redundant move elimination
                        // remove this MOV if the register is never used again
                        continue;
                    }
                } else if (op == Opcode::I2F || op == Opcode::F2I) {
                    const RVMValue& src = movInstr->source();
                    const RVMValue& dst = movInstr->destination();

                    // If source is a constant and destination is a register,
                    // record the constant mapping
                    if (src.isConstant() && dst.isRegister()) {
                        if (op == Opcode::I2F && src.type().kind() == type::TypeKind::Integer) {
                            RegId dstReg     = dst.regId();
                            constMap[dstReg] = RVMValue::Constant(static_cast<Number>(std::get<Integer>(src.constantValue())));
                            // Continue to next instruction - we'll let dead-code elimination
                            // remove this I2F if the register is never used again
                            continue;
                        } else if (op == Opcode::F2I && src.type().kind() == type::TypeKind::Number) {
                            RegId dstReg     = dst.regId();
                            constMap[dstReg] = RVMValue::Constant(static_cast<Integer>(std::get<Number>(src.constantValue())));
                            // Continue to next instruction - we'll let dead-code elimination
                            // remove this F2I if the register is never used again
                            continue;
                        }
                    }
                }
            }

            // For all instructions, except call and return, replace register uses with constants where possible
            if (op != Opcode::CALL_EXTERNAL && op != Opcode::CALL_INTERNAL && op != Opcode::RET) {
                instr->forEachSource([&](RVMValue& value) {
                    if (value.isRegister()) {
                        RegId reg = value.regId();
                        if (auto it = constMap.find(reg); it != constMap.end()) {
                            // Replace register with constant value
                            value   = it->second;
                            changed = true;
                        }
                    }
                });
            }

            // If this instruction writes to a register, invalidate any constant mapping for it
            instr->forEachDestination([&](const RVMValue& value) {
                if (value.isRegister())
                    constMap.erase(value.regId());
            });
        }
    }

    // Reconstruct program from blocks if changes were made
    if (changed) {
        program.clear();
        for (const auto& block : blocks)
            program.insert(program.end(), block.begin(), block.end());
    }

    return changed;
}

} // namespace PExpr::rvm