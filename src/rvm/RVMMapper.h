#pragma once

#include "RVMStructs.h"
#include "ssa/SSAStructs.h"

#include <memory>
#include <unordered_map>

namespace PExpr::rvm {

/// Maps SSA IR to RVM IR
class RVMMapper {
public:
    /// Create a mapper
    RVMMapper();

    /// Map an entire SSA program to RVM program
    RVMProgram mapProgram(const ssa::SSAProgram& ssaProgram);

    /// Map SSA type to elementary types (dissolve tuples)
    static std::vector<type::Type> dissolveTupleType(const type::Type& type);

private:
    /// Map SSA type to elementary types (dissolve tuples)
    std::vector<RVMValue> mapTupleValues(const ssa::SSAValue& value);

    /// Map SSA instructions to RVM instructions
    std::vector<std::shared_ptr<RVMInstr>> mapInstructions(const std::vector<std::shared_ptr<ssa::SSAInstr>>& ssaInstrs, const ssa::SSAProgram& ssaProgram);

    /// Map SSA value to RVM value
    RVMValue mapValue(const ssa::SSAValue& ssaValue);

    // Helper functions for specific instruction types
    std::vector<std::shared_ptr<RVMInstr>> mapAssign(const ssa::SSAInstrAssign& instr);

    std::vector<std::shared_ptr<RVMInstr>> mapCall(const ssa::SSAInstrCall& instr, const ssa::SSAProgram& ssaProgram);

    std::vector<std::shared_ptr<RVMInstr>> mapReturn(const ssa::SSAInstrReturn& instr);

    std::shared_ptr<RVMInstr> mapBranch(const ssa::SSAInstrBranch& instr);

    std::shared_ptr<RVMInstr> mapGoto(const ssa::SSAInstrGoto& instr);

    RVMValue accessTuple(const ssa::SSAValue& value, Integer idx);

    // Member variables
    RVMContext mContext;
    std::unordered_map<std::string, RVMValue> mSSAtoRVMMap;
    std::unordered_map<std::string, RVMValue> mStringMap;
    std::unordered_map<ssa::SSAValue, std::vector<RVMValue>> mTupleMap;
    std::unordered_map<ssa::SSAValue, std::pair<ssa::SSAValue, Integer>> mAccessMap;
};

} // namespace PExpr::rvm