#include "RVMMoveSimplifier.h"
#include "RVMBasicBlockAnalyzer.h"

namespace PExpr::rvm {

bool RVMMoveSimplifier::simplify(RVMProgram& program)
{
    bool changed = false;

    // Split program into basic blocks using the analyzer
    auto blocks = RVMBasicBlockAnalyzer::splitIntoBlocks(program);

    // Process each block
    for (auto& block : blocks)
        changed |= simplifyBlock(block);

    // Reconstruct program from blocks
    if (changed) {
        program.clear();
        for (auto& block : blocks)
            program.insert(program.end(), block.begin(), block.end());
    }

    return changed;
}

bool RVMMoveSimplifier::simplifyBlock(std::vector<std::shared_ptr<RVMInstr>>& block)
{
    bool changed = false;

    auto removeIdentities = [&]() {
        block.erase(std::remove_if(block.begin(), block.end(),
                                   [&](const std::shared_ptr<RVMInstr>& instr) {
                                       if (isMovInstruction(instr.get())) {
                                           auto* movInstr = static_cast<RVMInstr2Op*>(instr.get());
                                           if (isIdentityMov(movInstr)) {
                                               changed = true;
                                               return true;
                                           }
                                       }
                                       return false;
                                   }),
                    block.end());
    };

    // Remove all identity moves first
    removeIdentities();

    // Build register renaming map for this block
    std::unordered_map<RegId, RegId> renameMap; // from original register to ultimate source
    std::unordered_set<RegId> pinnedRegs;       // registers that cannot be renamed

    buildRenameMap(block, renameMap, pinnedRegs);

    // Apply renaming to SOURCE operands in all instructions
    for (auto& instr : block) {
        if (isMovInstruction(instr.get())) {
            auto* movInstr = static_cast<RVMInstr2Op*>(instr.get());

            // For MOV instructions, only rename the source operand (not the destination)
            auto srcs = movInstr->srcs();
            PEXPR_ASSERT(srcs.size() == 1, "'mov' instruction must have a single source");
            if (srcs.at(0).isRegister()) {
                RegId srcReg = srcs[0].regId();
                auto it      = renameMap.find(srcReg);
                if (it != renameMap.end()) {
                    // Create a new MOV instruction with renamed source
                    auto newMov = std::make_shared<RVMInstr2Op>(
                        Opcode::MOV,
                        movInstr->dst().value(),
                        RVMValue::Register(it->second, srcs[0].type()));
                    instr   = newMov;
                    changed = true;
                }
            }
        } else {
            // For non-MOV instructions, rename all register operands
            instr->forEachValue([&](RVMValue& value) {
                if (value.isRegister()) {
                    RegId reg = value.regId();
                    auto it   = renameMap.find(reg);
                    if (it != renameMap.end()) {
                        value   = RVMValue::Register(it->second, value.type());
                        changed = true;
                    }
                }
            });
        }
    }

    // Now remove any identity MOVs that were created by renaming
    removeIdentities();

    return changed;
}

bool RVMMoveSimplifier::isMovInstruction(const RVMInstr* instr)
{
    if (auto* instr2op = dynamic_cast<const RVMInstr2Op*>(instr))
        return instr2op->opcode() == Opcode::MOV;
    return false;
}

bool RVMMoveSimplifier::isIdentityMov(const RVMInstr2Op* movInstr)
{
    if (movInstr->opcode() != Opcode::MOV)
        return false;

    auto dstOpt = movInstr->dst();
    auto srcs   = movInstr->srcs();

    PEXPR_ASSERT(dstOpt.has_value(), "'mov' instruction must have a target destination");
    PEXPR_ASSERT(srcs.size() == 1, "'mov' instruction must have a single source");

    const RVMValue& dst = dstOpt.value();
    const RVMValue& src = srcs[0];

    // Check if both are registers with same ID and same type
    if (dst.isRegister() && src.isRegister())
        return dst.regId() == src.regId() && dst.type() == src.type();

    return false;
}

void RVMMoveSimplifier::collectPinnedRegisters(
    const std::vector<std::shared_ptr<RVMInstr>>& block,
    std::unordered_set<RegId>& pinnedRegs)
{
    pinnedRegs.clear();

    for (const auto& instr : block) {
        // Collect all registers used in non-MOV
        if (!isMovInstruction(instr.get())) {
            instr->forEachValue([&](const RVMValue& value) {
                if (value.isRegister())
                    pinnedRegs.insert(value.regId());
            });
        }
    }
}

void RVMMoveSimplifier::buildRenameMap(
    const std::vector<std::shared_ptr<RVMInstr>>& block,
    std::unordered_map<RegId, RegId>& renameMap,
    std::unordered_set<RegId>& pinnedRegs)
{
    renameMap.clear();
    collectPinnedRegisters(block, pinnedRegs);

    // Track chains of MOV instructions
    for (const auto& instr : block) {
        if (isMovInstruction(instr.get())) {
            auto* movInstr = static_cast<const RVMInstr2Op*>(instr.get());

            // Skip identity MOVs
            if (isIdentityMov(movInstr))
                continue;

            auto dstOpt = movInstr->dst();
            auto srcs   = movInstr->srcs();

            PEXPR_ASSERT(dstOpt.has_value(), "'mov' instruction must have a target destination");
            PEXPR_ASSERT(srcs.size() == 1, "'mov' instruction must have a single source");

            const RVMValue& dst = dstOpt.value();
            const RVMValue& src = srcs[0];

            if (!dst.isRegister() || !src.isRegister())
                continue;

            RegId dstReg = dst.regId();
            RegId srcReg = src.regId();

            // Follow rename chain to find ultimate source
            RegId ultimateSrc = srcReg;
            auto it           = renameMap.find(srcReg);
            while (it != renameMap.end()) {
                ultimateSrc = it->second;
                it          = renameMap.find(ultimateSrc);
            }

            // Check if destination is used in non-MOV instruction
            // We should be conservative and not rename registers that are used elsewhere
            if (pinnedRegs.find(dstReg) != pinnedRegs.end())
                continue;

            // Check for cycles
            RegId current = dstReg;
            bool cycle    = false;
            while (renameMap.find(current) != renameMap.end()) {
                if (renameMap[current] == ultimateSrc) {
                    cycle = true;
                    break;
                }
                current = renameMap[current];
            }

            if (!cycle && dstReg != ultimateSrc)
                renameMap[dstReg] = ultimateSrc;
        }
    }
}

RVMValue RVMMoveSimplifier::renameValue(const RVMValue& value, const std::unordered_map<RegId, RegId>& renameMap)
{
    if (!value.isRegister())
        return value;

    RegId reg = value.regId();
    auto it   = renameMap.find(reg);
    if (it != renameMap.end())
        return RVMValue::Register(it->second, value.type());

    return value;
}

} // namespace PExpr::rvm