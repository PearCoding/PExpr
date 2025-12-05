#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"

using namespace PExpr;
using namespace PExpr::ssa;
using namespace PExpr::internal;

TEST_CASE("Casts: implicit int->num in call injects SSA cast", "[casts]")
{
    std::stringstream stream("fn f(a:num) = a; f(1)");
    Environment env;
    auto ast = env.parse(stream);
    REQUIRE(ast != nullptr); // parsing & typechecking must succeed

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = prog.dump();

    // Expect a cast instruction inserted by lowering the injected CastExpression
    REQUIRE(dumped.find("cast(") != std::string::npos);
}

TEST_CASE("Casts: explicit num->int allowed with 'as' and lowers to SSA cast", "[casts]")
{
    std::stringstream stream("let x = 1.0 as int; x");
    Environment env;
    auto ast = env.parse(stream);
    REQUIRE(ast != nullptr); // explicit cast must be accepted by typechecker

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = prog.dump();

    // Expect a cast instruction produced by the explicit CastExpression
    REQUIRE(dumped.find("cast(") != std::string::npos);
}

TEST_CASE("Casts: implicit num->int in call is rejected (requires explicit cast)", "[casts]")
{
    std::stringstream stream("fn f(a:int) = a; f(1.0)");
    Environment env;
    // parse should fail because implicit num->int is not allowed
    auto ast = env.parse(stream);
    REQUIRE(ast == nullptr);
}

TEST_CASE("Casts: implicit int->num assignment injects SSA cast", "[casts]")
{
    // declare mutable variable of type num with a matching literal, then assign int
    std::stringstream stream("let mut a:num = 0.0; a = 1; a");
    Environment env;
    auto ast = env.parse(stream);
    REQUIRE(ast != nullptr); // typechecking should succeed and inject implicit cast on assignment

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = prog.dump();

    // Expect a cast instruction for the assignment
    REQUIRE(dumped.find("cast(") != std::string::npos);
}

TEST_CASE("Casts: implicit num->int assignment is rejected (requires explicit cast)", "[casts]")
{
    // assigning a num to a variable declared as int should require explicit cast
    std::stringstream stream("let mut a:int = 0; a = 1.0; a");
    Environment env;
    auto ast = env.parse(stream);
    REQUIRE(ast == nullptr); // typechecker should reject implicit num->int assignment
}
