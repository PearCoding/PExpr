#pragma once

#include "RVMStructs.h"
#include "ssa/SSAStructs.h"

#include <memory>
#include <unordered_map>

namespace PExpr::rvm {

/// Maps SSA IR to RVM IR
class RVMMapper {
public:
    /// Create a mapper with string table
    explicit RVMMapper(std::shared_ptr<RVMStringTable> stringTable);

    /// Map an entire SSA program to RVM program
    RVMProgram mapProgram(const ssa::SSAProgram& ssaProgram);

    /// Map a single SSA function to RVM function
    RVMFunction mapFunction(const ssa::SSAFunction& ssaFunc,
                            const ssa::SSAProgram& ssaProgram);

    /// Map SSA instructions to RVM instructions
    std::vector<std::shared_ptr<RVMInstr>> mapInstructions(
        const std::vector<std::shared_ptr<ssa::SSAInstr>>& ssaInstrs,
        RVMContext& context,
        const ssa::SSAProgram& ssaProgram);

    /// Map SSA value to RVM value
    RVMValue mapValue(const ssa::SSAValue& ssaValue,
                      RVMContext& context);

    /// Map SSA type to elementary types (dissolve tuples)
    static std::vector<type::Type> dissolveTupleType(const type::Type& type);

private:
    // Helper functions for specific instruction types
    std::vector<std::shared_ptr<RVMInstr>> mapAssign(const ssa::SSAInstrAssign& instr,
                                                     RVMContext& context);

    std::vector<std::shared_ptr<RVMInstr>> mapCall(const ssa::SSAInstrCall& instr,
                                                   RVMContext& context,
                                                   const ssa::SSAProgram& ssaProgram);

    std::vector<std::shared_ptr<RVMInstr>> mapReturn(const ssa::SSAInstrReturn& instr,
                                                     RVMContext& context);

    std::shared_ptr<RVMInstr> mapBranch(const ssa::SSAInstrBranch& instr,
                                        RVMContext& context);

    std::shared_ptr<RVMInstr> mapGoto(const ssa::SSAInstrGoto& instr);

    // Internal helper for mapping values
    RVMValue mapValueInternal(const ssa::SSAValue& ssaValue,
                              RVMContext& context);

    // Member variables
    std::shared_ptr<RVMStringTable> mStringTable;
    std::unordered_map<std::string, RVMValue> mSSAtoRVMMap;
};

} // namespace PExpr::rvm