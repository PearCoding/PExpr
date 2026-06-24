#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Environment.h"
#include "rvm/RVMInterpreter.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMOptimizer.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;

// Compile a single-expression program at the given optimization level, run it,
// and return the result. getNum()/getBool() are external impure functions whose
// results are fixed, keeping the operand non-constant so the math-identity rules
// (rather than constant folding) decide what happens to the unary operator.
static ValueVariant evalUnary(const char* source, const opt::OptimizerOptions& opts,
                              const type::Type& returnType, Number numInput, bool boolInput)
{
    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);
    auto ssaProgram = env.map(closure);
    env.optimize(ssaProgram, opts);

    RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(ssaProgram);
    RVMOptimizer::optimize(opts, rvmProgram);

    RVMInterpreter interpreter;
    interpreter.registerExternalFunction("_Z6getNum_P",
                                         [numInput](const std::vector<ValueVariant>&) -> ValueVariant { return numInput; });
    interpreter.registerExternalFunction("_Z7getBool_P",
                                         [boolInput](const std::vector<ValueVariant>&) -> ValueVariant { return boolInput; });

    return interpreter.execute(rvmProgram, returnType);
}

// Regression for the inverted guard in matchUnaryIdentity, which replaced every
// non-Pos unary op with a plain copy of its operand: -a became a and !a became a
// at -O2 and above (where ApplyMathIdentities is enabled).
TEST_CASE("Identity optimizer keeps negation at math-identity levels", "[sscp][identity][unary]")
{
    const char* src = R"(
        @[extern] fn getNum() -> num;
        -getNum()
    )";

    for (auto opts : { opt::OptimizerOptions::Medium(), opt::OptimizerOptions::High() }) {
        auto result = evalUnary(src, opts, Type(TypeKind::Number), 5.0, false);
        REQUIRE(std::holds_alternative<Number>(result));
        REQUIRE(std::get<Number>(result) == Catch::Approx(-5.0));
    }
}

TEST_CASE("Identity optimizer keeps logical not at math-identity levels", "[sscp][identity][unary]")
{
    const char* src = R"(
        @[extern] fn getBool() -> bool;
        !getBool()
    )";

    for (auto opts : { opt::OptimizerOptions::Medium(), opt::OptimizerOptions::High() }) {
        auto resultTrue = evalUnary(src, opts, Type(TypeKind::Boolean), 0.0, true);
        REQUIRE(std::holds_alternative<bool>(resultTrue));
        REQUIRE(std::get<bool>(resultTrue) == false);

        auto resultFalse = evalUnary(src, opts, Type(TypeKind::Boolean), 0.0, false);
        REQUIRE(std::holds_alternative<bool>(resultFalse));
        REQUIRE(std::get<bool>(resultFalse) == true);
    }
}

// Double negation must still collapse: --a = a and !!a = a.
TEST_CASE("Identity optimizer still collapses double negation", "[sscp][identity][unary]")
{
    auto resultNum = evalUnary(R"(
        @[extern] fn getNum() -> num;
        -(-getNum())
    )",
                               opt::OptimizerOptions::High(), Type(TypeKind::Number), 5.0, false);
    REQUIRE(std::holds_alternative<Number>(resultNum));
    REQUIRE(std::get<Number>(resultNum) == Catch::Approx(5.0));

    auto resultBool = evalUnary(R"(
        @[extern] fn getBool() -> bool;
        !(!getBool())
    )",
                                opt::OptimizerOptions::High(), Type(TypeKind::Boolean), 0.0, true);
    REQUIRE(std::holds_alternative<bool>(resultBool));
    REQUIRE(std::get<bool>(resultBool) == true);
}
