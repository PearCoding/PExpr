#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;

TEST_CASE("Casts: implicit int->num in call injects SSA cast", "[casts]")
{
    Environment env;
    auto ast = env.parse("fn f(a:num) = a; f(1)");
    REQUIRE(ast != nullptr); // parsing & typechecking must succeed

    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Expect a cast instruction inserted by lowering the injected CastExpression
    REQUIRE(dumped.find("cast(") != std::string::npos);
}

TEST_CASE("Casts: explicit num->int allowed with 'as' and lowers to SSA cast", "[casts]")
{
    Environment env;
    auto ast = env.parse("let x = 1.0 as int; x");
    REQUIRE(ast != nullptr); // explicit cast must be accepted by typechecker

    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Expect a cast instruction produced by the explicit CastExpression
    REQUIRE(dumped.find("cast(") != std::string::npos);
}

TEST_CASE("Casts: implicit num->int in call is rejected (requires explicit cast)", "[casts]")
{
    Environment env;
    env.reporter().setQuiet(true);

    // parse should fail because implicit num->int is not allowed
    auto ast = env.parse("fn f(a:int) = a; f(1.0)");
    REQUIRE(ast == nullptr);
}

TEST_CASE("Casts: implicit int->num assignment injects SSA cast", "[casts]")
{
    // declare mutable variable of type num with a matching literal, then assign int
    Environment env;
    env.reporter().setQuiet(true);
    auto ast = env.parse("let mut a:num = 0.0; a = 1; a");
    REQUIRE(ast != nullptr); // typechecking should succeed and inject implicit cast on assignment

    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Expect a cast instruction for the assignment
    REQUIRE(dumped.find("cast(") != std::string::npos);
}

TEST_CASE("Casts: implicit num->int assignment is rejected (requires explicit cast)", "[casts]")
{
    // assigning a num to a variable declared as int should require explicit cast
    Environment env;
    env.reporter().setQuiet(true);

    auto ast = env.parse("let mut a:int = 0; a = 1.0; a");
    REQUIRE(ast == nullptr); // typechecker should reject implicit num->int assignment
}