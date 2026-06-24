#include <catch2/catch_test_macros.hpp>

#include "Environment.h"
#include "rvm/RVMInterpreter.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMOptimizer.h"
#include "ssa/SSAMapper.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;

// Direct interpreter test: build a single logical instruction and run it.
static ValueVariant runLogical(Opcode op, const RVMValue& a, const RVMValue& b)
{
    RVMProgram prog;
    prog.push_back(std::make_shared<RVMInstrLabel>("entry"));
    prog.push_back(std::make_shared<RVMInstr3Op>(
        op,
        RVMValue::Register(0, Type(TypeKind::Boolean)),
        a,
        b));
    prog.push_back(std::make_shared<RVMInstrReturn>(1));

    RVMInterpreter interpreter;
    return interpreter.execute(prog, Type(TypeKind::Boolean));
}

static bool asBool(const ValueVariant& v)
{
    if (std::holds_alternative<bool>(v))
        return std::get<bool>(v);
    if (std::holds_alternative<Integer>(v))
        return std::get<Integer>(v) != 0;
    return false;
}

// End-to-end: compile a boolean expression and run it. The expression may use an
// external impure getBool() whose result is fixed to `input`, which keeps the
// operands non-constant so the optimizer cannot fold the logical operator away.
// Uses the Low optimization level so the logical operators reach the interpreter
// unchanged (the math-identity rules are exercised separately).
static bool evalBool(const char* source, bool input)
{
    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);
    auto ssaProgram = env.map(closure);
    env.optimize(ssaProgram, opt::OptimizerOptions::Low());

    RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(ssaProgram);
    RVMOptimizer::optimize(opt::OptimizerOptions::Low(), rvmProgram);

    RVMInterpreter interpreter;
    interpreter.registerExternalFunction("_Z7getBool_P",
                                         [input](const std::vector<ValueVariant>&) -> ValueVariant {
                                             return input;
                                         });

    return asBool(interpreter.execute(rvmProgram, Type(TypeKind::Boolean)));
}

// Regression for the bug where applyBinaryOp had no AND/OR/XOR cases and fell
// through to `return Integer(0)`, so every &&, || and ! on runtime values
// evaluated to 0/false. See RVMInterpreter::applyBinaryOp.
TEST_CASE("RVMInterpreter: AND opcode", "[rvm][interpreter][logical]")
{
    const auto T = RVMValue::Constant(true);
    const auto F = RVMValue::Constant(false);
    REQUIRE(asBool(runLogical(Opcode::AND, T, T)) == true);
    REQUIRE(asBool(runLogical(Opcode::AND, T, F)) == false);
    REQUIRE(asBool(runLogical(Opcode::AND, F, T)) == false);
    REQUIRE(asBool(runLogical(Opcode::AND, F, F)) == false);
}

TEST_CASE("RVMInterpreter: OR opcode", "[rvm][interpreter][logical]")
{
    const auto T = RVMValue::Constant(true);
    const auto F = RVMValue::Constant(false);
    REQUIRE(asBool(runLogical(Opcode::OR, T, T)) == true);
    REQUIRE(asBool(runLogical(Opcode::OR, T, F)) == true);
    REQUIRE(asBool(runLogical(Opcode::OR, F, T)) == true);
    REQUIRE(asBool(runLogical(Opcode::OR, F, F)) == false);
}

TEST_CASE("RVMInterpreter: XOR opcode implements logical not", "[rvm][interpreter][logical]")
{
    // The mapper lowers unary ! to XOR(operand, 1).
    const auto one = RVMValue::Constant(Integer(1));
    REQUIRE(asBool(runLogical(Opcode::XOR, RVMValue::Constant(true), one)) == false);
    REQUIRE(asBool(runLogical(Opcode::XOR, RVMValue::Constant(false), one)) == true);
}

TEST_CASE("RVM logical operators evaluate correctly end-to-end", "[rvm][interpreter][logical]")
{
    SECTION("logical and")
    {
        const char* src = R"(
            @[extern] fn getBool() -> bool;
            getBool() && getBool()
        )";
        REQUIRE(evalBool(src, true) == true);
        REQUIRE(evalBool(src, false) == false);
    }

    SECTION("logical or")
    {
        const char* src = R"(
            @[extern] fn getBool() -> bool;
            getBool() || getBool()
        )";
        REQUIRE(evalBool(src, true) == true);
        REQUIRE(evalBool(src, false) == false);
    }

    SECTION("logical not")
    {
        const char* src = R"(
            @[extern] fn getBool() -> bool;
            !getBool()
        )";
        REQUIRE(evalBool(src, true) == false);
        REQUIRE(evalBool(src, false) == true);
    }
}
