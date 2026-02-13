#pragma once

#include "PExpr.h"
#include "type/Type.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace PExpr::rvm {
/// RVM register identifier
using RegId = uint32_t;

/// RVM instruction opcode
enum class Opcode : uint8_t {
    MOV, // Move between registers

    // Arithmetic operations
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    POW,

    // Bit operations (not really supported in PExpr, but maybe in the future)
    AND,
    OR,
    XOR,
    SHL,
    SHR,

    // Comparisons
    CMP_EQ,
    CMP_NE,
    CMP_LT,
    CMP_LE,
    CMP_GT,
    CMP_GE,

    // Conversions
    I2F, // Integer to Float
    F2I, // Float to Integer

    // Control flow
    BR,            // Branch conditional
    BRZ,           // Branch if zero
    BRNZ,          // Branch if not zero
    JMP,           // Unconditional jump
    CALL_EXTERNAL, // Function call to an external function (host)
    CALL_INTERNAL, // Function call to an internal function (PExpr)
    RET,           // Return

    // Register frame operations
    PUSH_FRAME, // Push register frame onto stack
    POP_FRAME,  // Pop register frame from stack

    // String literal
    LOAD_STRING, // Load string literal
};

} // namespace PExpr::rvm