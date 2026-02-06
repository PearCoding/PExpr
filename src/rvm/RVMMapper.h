#pragma once

#include "RVMStructs.h"
#include "ssa/SSAStructs.h"

namespace PExpr::rvm {

/// Maps SSA IR to RVM IR
class RVMMapper {
public:
    /// Map an entire SSA program to RVM program
    static RVMProgram mapProgram(const ssa::SSAProgram& ssaProgram);

    /// Map a single SSA function to RVM function
    static RVMFunction mapFunction(const ssa::SSAFunction& ssaFunc,
                                   std::shared_ptr<RVMStringTable> stringTable,
                                   const ssa::SSAProgram& ssaProgram);

    /// Map SSA instructions to RVM instructions
    static std::vector<std::shared_ptr<RVMInstr>> mapInstructions(
        const std::vector<std::shared_ptr<ssa::SSAInstr>>& ssaInstrs,
        std::shared_ptr<RVMStringTable> stringTable,
        RVMContext& context,
        const ssa::SSAProgram& ssaProgram);

    /// Map SSA value to RVM value
    static RVMValue mapValue(const ssa::SSAValue& ssaValue,
                             std::shared_ptr<RVMStringTable> stringTable,
                             RVMContext& context);

    /// Map SSA type to elementary types (dissolve tuples)
    static std::vector<type::Type> dissolveTupleType(const type::Type& type);

private:
    // Helper functions for specific instruction types
    static std::vector<std::shared_ptr<RVMInstr>> mapAssign(
        const ssa::SSAInstrAssign& instr,
        std::shared_ptr<RVMStringTable> stringTable,
        RVMContext& context);

    static std::vector<std::shared_ptr<RVMInstr>> mapCall(
        const ssa::SSAInstrCall& instr,
        std::shared_ptr<RVMStringTable> stringTable,
        RVMContext& context,
        const ssa::SSAProgram& ssaProgram);

    static std::vector<std::shared_ptr<RVMInstr>> mapReturn(
        const ssa::SSAInstrReturn& instr,
        std::shared_ptr<RVMStringTable> stringTable,
        RVMContext& context);

    static std::shared_ptr<RVMInstr> mapBranch(
        const ssa::SSAInstrBranch& instr,
        std::shared_ptr<RVMStringTable> stringTable,
        RVMContext& context);

    static std::shared_ptr<RVMInstr> mapGoto(
        const ssa::SSAInstrGoto& instr);
};

} // namespace PExpr::rvm