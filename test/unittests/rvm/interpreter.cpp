#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "rvm/RVMInstruction.h"
#include "rvm/RVMInterpreter.h"
#include "rvm/RVMStructs.h"
#include "rvm/RVMValue.h"
#include "type/Type.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;

// ============================================================================
// Basic Arithmetic Operations
// ============================================================================

TEST_CASE("RVMInterpreter: ADD operation with integers", "[rvm][interpreter][arithmetic]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::ADD,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(10)),
        RVMValue::Constant(Integer(20))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 30);
}

TEST_CASE("RVMInterpreter: ADD operation with floats", "[rvm][interpreter][arithmetic]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::ADD,
        RVMValue::Register(0, Type(TypeKind::Number)),
        RVMValue::Constant(Number(3.5)),
        RVMValue::Constant(Number(2.5))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Number));

    REQUIRE(std::holds_alternative<Number>(result));
    REQUIRE(std::get<Number>(result) == Catch::Approx(6.0));
}

TEST_CASE("RVMInterpreter: ADD operation with mixed types", "[rvm][interpreter][arithmetic]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::ADD,
        RVMValue::Register(0, Type(TypeKind::Number)),
        RVMValue::Constant(Integer(5)),
        RVMValue::Constant(Number(3.5))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Number));

    REQUIRE(std::holds_alternative<Number>(result));
    REQUIRE(std::get<Number>(result) == Catch::Approx(8.5));
}

TEST_CASE("RVMInterpreter: SUB operation", "[rvm][interpreter][arithmetic]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::SUB,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(20)),
        RVMValue::Constant(Integer(7))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 13);
}

TEST_CASE("RVMInterpreter: MUL operation", "[rvm][interpreter][arithmetic]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::MUL,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(6)),
        RVMValue::Constant(Integer(7))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 42);
}

TEST_CASE("RVMInterpreter: DIV operation", "[rvm][interpreter][arithmetic]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::DIV,
        RVMValue::Register(0, Type(TypeKind::Number)),
        RVMValue::Constant(Number(10.0)),
        RVMValue::Constant(Number(4.0))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Number));

    REQUIRE(std::holds_alternative<Number>(result));
    REQUIRE(std::get<Number>(result) == Catch::Approx(2.5));
}

TEST_CASE("RVMInterpreter: DIV by zero returns infinity", "[rvm][interpreter][arithmetic]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::DIV,
        RVMValue::Register(0, Type(TypeKind::Number)),
        RVMValue::Constant(Number(10.0)),
        RVMValue::Constant(Number(0.0))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Number));

    REQUIRE(std::holds_alternative<Number>(result));
    REQUIRE(std::isinf(std::get<Number>(result)));
}

TEST_CASE("RVMInterpreter: MOD operation", "[rvm][interpreter][arithmetic]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::MOD,
        RVMValue::Register(0, Type(TypeKind::Number)),
        RVMValue::Constant(Number(17.0)),
        RVMValue::Constant(Number(5.0))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Number));

    REQUIRE(std::holds_alternative<Number>(result));
    REQUIRE(std::get<Number>(result) == Catch::Approx(2.0));
}

TEST_CASE("RVMInterpreter: POW operation", "[rvm][interpreter][arithmetic]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::POW,
        RVMValue::Register(0, Type(TypeKind::Number)),
        RVMValue::Constant(Number(2.0)),
        RVMValue::Constant(Number(3.0))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Number));

    REQUIRE(std::holds_alternative<Number>(result));
    REQUIRE(std::get<Number>(result) == Catch::Approx(8.0));
}

TEST_CASE("RVMInterpreter: multiple sequential arithmetic operations", "[rvm][interpreter][arithmetic]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));

    // r0 = 5 + 3 = 8
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::ADD,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(5)),
        RVMValue::Constant(Integer(3))));

    // r1 = 2 * 4 = 8
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::MUL,
        RVMValue::Register(1, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(2)),
        RVMValue::Constant(Integer(4))));

    // r0 = r0 + r1 = 16
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::ADD,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(1, Type(TypeKind::Integer))));

    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 16);
}

// ============================================================================
// Comparison Operations
// ============================================================================

TEST_CASE("RVMInterpreter: CMP_EQ with integers", "[rvm][interpreter][comparison]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::CMP_EQ,
        RVMValue::Register(0, Type(TypeKind::Boolean)),
        RVMValue::Constant(Integer(42)),
        RVMValue::Constant(Integer(42))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Boolean));

    REQUIRE(std::holds_alternative<bool>(result));
    REQUIRE(std::get<bool>(result) == true);
}

TEST_CASE("RVMInterpreter: CMP_NE with integers", "[rvm][interpreter][comparison]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::CMP_NE,
        RVMValue::Register(0, Type(TypeKind::Boolean)),
        RVMValue::Constant(Integer(10)),
        RVMValue::Constant(Integer(20))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Boolean));

    REQUIRE(std::holds_alternative<bool>(result));
    REQUIRE(std::get<bool>(result) == true);
}

TEST_CASE("RVMInterpreter: CMP_LT", "[rvm][interpreter][comparison]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::CMP_LT,
        RVMValue::Register(0, Type(TypeKind::Boolean)),
        RVMValue::Constant(Integer(5)),
        RVMValue::Constant(Integer(10))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Boolean));

    REQUIRE(std::holds_alternative<bool>(result));
    REQUIRE(std::get<bool>(result) == true);
}

TEST_CASE("RVMInterpreter: CMP_LE", "[rvm][interpreter][comparison]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::CMP_LE,
        RVMValue::Register(0, Type(TypeKind::Boolean)),
        RVMValue::Constant(Integer(10)),
        RVMValue::Constant(Integer(10))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Boolean));

    REQUIRE(std::holds_alternative<bool>(result));
    REQUIRE(std::get<bool>(result) == true);
}

TEST_CASE("RVMInterpreter: CMP_GT", "[rvm][interpreter][comparison]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::CMP_GT,
        RVMValue::Register(0, Type(TypeKind::Boolean)),
        RVMValue::Constant(Number(10.5)),
        RVMValue::Constant(Number(5.5))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Boolean));

    REQUIRE(std::holds_alternative<bool>(result));
    REQUIRE(std::get<bool>(result) == true);
}

TEST_CASE("RVMInterpreter: CMP_GE", "[rvm][interpreter][comparison]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::CMP_GE,
        RVMValue::Register(0, Type(TypeKind::Boolean)),
        RVMValue::Constant(Integer(10)),
        RVMValue::Constant(Integer(5))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Boolean));

    REQUIRE(std::holds_alternative<bool>(result));
    REQUIRE(std::get<bool>(result) == true);
}

TEST_CASE("RVMInterpreter: CMP_EQ with strings", "[rvm][interpreter][comparison]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));

    // Load string literals into string table
    prog.push_back(std::make_shared<RVMInstrStringLiteral>(
        RVMValue::StringRef(0),
        "hello"));
    prog.push_back(std::make_shared<RVMInstrStringLiteral>(
        RVMValue::StringRef(1),
        "hello"));

    // Compare strings using string references
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::CMP_EQ,
        RVMValue::Register(0, Type(TypeKind::Boolean)),
        RVMValue::StringRef(0),
        RVMValue::StringRef(1)));

    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Boolean));

    REQUIRE(std::holds_alternative<bool>(result));
    REQUIRE(std::get<bool>(result) == true);
}

TEST_CASE("RVMInterpreter: CMP_NE with strings", "[rvm][interpreter][comparison]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));

    // Load string literals into string table
    prog.push_back(std::make_shared<RVMInstrStringLiteral>(
        RVMValue::StringRef(0),
        "hello"));
    prog.push_back(std::make_shared<RVMInstrStringLiteral>(
        RVMValue::StringRef(1),
        "world"));

    // Compare strings using string references
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::CMP_NE,
        RVMValue::Register(0, Type(TypeKind::Boolean)),
        RVMValue::StringRef(0),
        RVMValue::StringRef(1)));

    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Boolean));

    REQUIRE(std::holds_alternative<bool>(result));
    REQUIRE(std::get<bool>(result) == true);
}

// ============================================================================
// Type Conversions
// ============================================================================

TEST_CASE("RVMInterpreter: I2F conversion", "[rvm][interpreter][conversion]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::I2F,
        RVMValue::Register(0, Type(TypeKind::Number)),
        RVMValue::Constant(Integer(42))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Number));

    REQUIRE(std::holds_alternative<Number>(result));
    REQUIRE(std::get<Number>(result) == Catch::Approx(42.0));
}

TEST_CASE("RVMInterpreter: F2I conversion", "[rvm][interpreter][conversion]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::F2I,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Number(3.7))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 3);
}

TEST_CASE("RVMInterpreter: round-trip conversion I2F then F2I", "[rvm][interpreter][conversion]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));

    // I2F: r0 = 100.0
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::I2F,
        RVMValue::Register(0, Type(TypeKind::Number)),
        RVMValue::Constant(Integer(100))));

    // F2I: r0 = 100 (store in r0, not r1)
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::F2I,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(0, Type(TypeKind::Number))));

    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 100);
}

// ============================================================================
// Control Flow
// ============================================================================

TEST_CASE("RVMInterpreter: JZ with false condition", "[rvm][interpreter][controlflow]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(10))));
    prog.push_back(std::make_shared<RVMInstrBranch>(
        Opcode::JZ,
        RVMValue::Constant(false),
        "skip"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(20))));
    prog.push_back(std::make_shared<RVMInstrLabel>("skip"));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 10);
}

TEST_CASE("RVMInterpreter: JZ with true condition", "[rvm][interpreter][controlflow]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(10))));
    prog.push_back(std::make_shared<RVMInstrBranch>(
        Opcode::JZ,
        RVMValue::Constant(true),
        "skip"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(20))));
    prog.push_back(std::make_shared<RVMInstrLabel>("skip"));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 20);
}

TEST_CASE("RVMInterpreter: JZ with integer zero", "[rvm][interpreter][controlflow]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(10))));
    prog.push_back(std::make_shared<RVMInstrBranch>(
        Opcode::JZ,
        RVMValue::Constant(Integer(0)),
        "skip"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(20))));
    prog.push_back(std::make_shared<RVMInstrLabel>("skip"));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 10);
}

TEST_CASE("RVMInterpreter: JNZ with true condition", "[rvm][interpreter][controlflow]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(10))));
    prog.push_back(std::make_shared<RVMInstrBranch>(
        Opcode::JNZ,
        RVMValue::Constant(true),
        "skip"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(20))));
    prog.push_back(std::make_shared<RVMInstrLabel>("skip"));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 10);
}

TEST_CASE("RVMInterpreter: JMP unconditional jump", "[rvm][interpreter][controlflow]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstrJump>("target"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(10))));
    prog.push_back(std::make_shared<RVMInstrLabel>("target"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(20))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 20);
}

TEST_CASE("RVMInterpreter: branch with computed condition", "[rvm][interpreter][controlflow]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));

    // Compute condition: 5 < 10 = true
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::CMP_LT,
        RVMValue::Register(0, Type(TypeKind::Boolean)),
        RVMValue::Constant(Integer(5)),
        RVMValue::Constant(Integer(10))));

    // Branch based on computed condition
    prog.push_back(std::make_shared<RVMInstrBranch>(
        Opcode::JZ,
        RVMValue::Register(0, Type(TypeKind::Boolean)),
        "else"));

    // Then branch - store in r0 (not r1)
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(100))));
    prog.push_back(std::make_shared<RVMInstrJump>("end"));

    // Else branch - store in r0 (not r1)
    prog.push_back(std::make_shared<RVMInstrLabel>("else"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(200))));

    prog.push_back(std::make_shared<RVMInstrLabel>("end"));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 100);
}

// ============================================================================
// Function Calls - Internal
// ============================================================================

TEST_CASE("RVMInterpreter: internal function call", "[rvm][interpreter][function]")
{
    RVMProgram prog;

    // Jump to entry to skip function definitions
    prog.push_back(std::make_shared<RVMInstrJump>("entry"));

    // Function definition
    prog.push_back(std::make_shared<RVMInstrLabel>("add_func"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::ADD,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(1, Type(TypeKind::Integer))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    // Main entry
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));

    // Set up parameters in registers
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(10))));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(1, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(20))));

    // Call function
    prog.push_back(std::make_shared<RVMInstrCall>(false, 2, 1, "add_func"));

    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 30);
}

TEST_CASE("RVMInterpreter: nested internal function calls", "[rvm][interpreter][function]")
{
    RVMProgram prog;

    // Jump to entry to skip function definitions
    prog.push_back(std::make_shared<RVMInstrJump>("entry"));

    // Function: square
    prog.push_back(std::make_shared<RVMInstrLabel>("square"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::MUL,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(0, Type(TypeKind::Integer))));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    // Function: add_and_square
    prog.push_back(std::make_shared<RVMInstrLabel>("add_and_square"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::ADD,
        RVMValue::Register(2, Type(TypeKind::Integer)),
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(1, Type(TypeKind::Integer))));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(2, Type(TypeKind::Integer))));
    prog.push_back(std::make_shared<RVMInstrCall>(false, 1, 1, "square"));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    // Main entry
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(3))));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(1, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(4))));
    prog.push_back(std::make_shared<RVMInstrCall>(false, 2, 1, "add_and_square"));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 49); // (3 + 4)^2 = 49
}

// ============================================================================
// Function Calls - External
// ============================================================================

TEST_CASE("RVMInterpreter: external function call", "[rvm][interpreter][function]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));

    // Set up parameter
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(10))));

    // Call external function
    prog.push_back(std::make_shared<RVMInstrCall>(true, 1, 1, "double_value"));

    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;

    // Register external function
    interpreter.registerExternalFunction("double_value", [](const std::vector<ValueVariant>& args) {
        if (std::holds_alternative<Integer>(args[0])) {
            return ValueVariant(Integer(std::get<Integer>(args[0]) * 2));
        }
        return ValueVariant(Integer(0));
    });

    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 20);
}

TEST_CASE("RVMInterpreter: external function with multiple parameters", "[rvm][interpreter][function]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));

    // Set up parameters
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(5))));
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(1, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(3))));

    // Call external function
    prog.push_back(std::make_shared<RVMInstrCall>(true, 2, 1, "multiply"));

    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;

    // Register external function
    interpreter.registerExternalFunction("multiply", [](const std::vector<ValueVariant>& args) {
        Integer a = std::holds_alternative<Integer>(args[0]) ? std::get<Integer>(args[0]) : 0;
        Integer b = std::holds_alternative<Integer>(args[1]) ? std::get<Integer>(args[1]) : 0;
        return ValueVariant(Integer(a * b));
    });

    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 15);
}

// ============================================================================
// String Operations
// ============================================================================

TEST_CASE("RVMInterpreter: LOAD_STRING instruction", "[rvm][interpreter][string]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstrStringLiteral>(RVMValue::StringRef(0), "Hello, World!"));
    prog.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, RVMValue::Register(0, Type(TypeKind::String)), RVMValue::StringRef(0)));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::String));

    REQUIRE(std::holds_alternative<std::string>(result));
    REQUIRE(std::get<std::string>(result) == "Hello, World!");
}

TEST_CASE("RVMInterpreter: multiple string literals", "[rvm][interpreter][string]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));

    prog.push_back(std::make_shared<RVMInstrStringLiteral>(RVMValue::StringRef(0), "first"));
    prog.push_back(std::make_shared<RVMInstrStringLiteral>(RVMValue::StringRef(1), "second"));
    prog.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, RVMValue::Register(0, Type(TypeKind::String)), RVMValue::StringRef(0)));

    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::String));

    REQUIRE(std::holds_alternative<std::string>(result));
    REQUIRE(std::get<std::string>(result) == "first");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("RVMInterpreter: empty program with void return", "[rvm][interpreter][edgecase]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstrReturn>(0));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Void));

    // Void returns an empty variant (monostate)
    REQUIRE(result.index() == 0);  // monostate is at index 0
}

TEST_CASE("RVMInterpreter: default value for uninitialized register", "[rvm][interpreter][edgecase]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    // Don't set any registers, just return
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 0);
}

TEST_CASE("RVMInterpreter: MOV instruction", "[rvm][interpreter][edgecase]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));

    // r0 = 42
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(42))));

    // r1 = r0
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(1, Type(TypeKind::Integer)),
        RVMValue::Register(0, Type(TypeKind::Integer))));

    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 42);
}

TEST_CASE("RVMInterpreter: complex expression chain", "[rvm][interpreter][edgecase]")
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));

    // r0 = 10
    prog.push_back(std::make_shared<RVMInstr2Op>(
        Opcode::MOV,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(10))));

    // r0 = r0 + 5 = 15
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::ADD,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(5))));

    // r0 = r0 * 2 = 30
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::MUL,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(2))));

    // r0 = r0 - 10 = 20
    prog.push_back(std::make_shared<RVMInstr3Op>(
        Opcode::SUB,
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Register(0, Type(TypeKind::Integer)),
        RVMValue::Constant(Integer(10))));

    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    auto result = interpreter.execute(prog, Type(TypeKind::Integer));

    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 20);
}

TEST_CASE("RVMInterpreter: parseValue utility", "[rvm][interpreter][utility]")
{
    SECTION("Parse boolean true")
    {
        auto result = RVMInterpreter::parseValue("true");
        REQUIRE(std::holds_alternative<bool>(result));
        REQUIRE(std::get<bool>(result) == true);
    }

    SECTION("Parse boolean false")
    {
        auto result = RVMInterpreter::parseValue("false");
        REQUIRE(std::holds_alternative<bool>(result));
        REQUIRE(std::get<bool>(result) == false);
    }

    SECTION("Parse integer")
    {
        auto result = RVMInterpreter::parseValue("42");
        REQUIRE(std::holds_alternative<Integer>(result));
        REQUIRE(std::get<Integer>(result) == 42);
    }

    SECTION("Parse negative integer")
    {
        auto result = RVMInterpreter::parseValue("-10");
        REQUIRE(std::holds_alternative<Integer>(result));
        REQUIRE(std::get<Integer>(result) == -10);
    }

    SECTION("Parse number with decimal")
    {
        auto result = RVMInterpreter::parseValue("3.14");
        REQUIRE(std::holds_alternative<Number>(result));
        REQUIRE(std::get<Number>(result) == Catch::Approx(3.14));
    }

    SECTION("Parse number with f suffix")
    {
        auto result = RVMInterpreter::parseValue("2.5f");
        REQUIRE(std::holds_alternative<Number>(result));
        REQUIRE(std::get<Number>(result) == Catch::Approx(2.5));
    }

    SECTION("Parse string")
    {
        auto result = RVMInterpreter::parseValue("hello");
        REQUIRE(std::holds_alternative<std::string>(result));
        REQUIRE(std::get<std::string>(result) == "hello");
    }
}