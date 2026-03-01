#pragma once

#include "RVMStructs.h"
#include "type/Type.h"
#include "RVMValue.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace PExpr::rvm {

/// RVMInterpreter executes a RVMProgram
class RVMInterpreter {
public:
    struct RegisterValue {
        ValueVariant value;
        type::Type type;
    };

    RVMInterpreter() = default;

    /// Register an external function that can be called from RVM
    void registerExternalFunction(const std::string& mangledName,
                                  std::function<ValueVariant(const std::vector<ValueVariant>&)> func);

    /// Execute the RVM program and return the result
    ValueVariant execute(const RVMProgram& program, const type::Type& returnType);

    /// Utility to parse a string value (used for getValue validation function)
    static ValueVariant parseValue(const std::string& str);

private:
    std::unordered_map<RegId, RegisterValue> registers;
    std::unordered_map<uint32_t, std::string> stringTable;
    
    struct CallFrame {
        std::unordered_map<RegId, RegisterValue> Registers;
        size_t ReturnPC;
    };
    std::vector<CallFrame> callStack;
    std::unordered_map<std::string, std::function<ValueVariant(const std::vector<ValueVariant>&)>> externalFunctions;

    void pushCallFrame(size_t pc);
    size_t popCallFrame(size_t retCount);

    ValueVariant evaluateValue(const RVMValue& value);
    void setRegister(const RVMValue& dst, const ValueVariant& value);
    
    ValueVariant getDefaultValue(const type::Type& type);
    bool isZero(const ValueVariant& val);
    
    ValueVariant applyUnaryOp(Opcode op, const ValueVariant& src);
    ValueVariant applyBinaryOp(Opcode op, const ValueVariant& src1, const ValueVariant& src2);
    
    ValueVariant reconstructTuple(const type::Type& type, size_t& regIndex);
};

} // namespace PExpr::rvm
