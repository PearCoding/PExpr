#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;

TEST_CASE("DeclaredTypes: implicit int->num declaration injects SSA cast", "[declared_types]")
{
    std::stringstream stream("let a:num = 1; a");
    Environment env;
    auto ast = env.parse(stream);
    REQUIRE(ast != nullptr); // parsing & typechecking must succeed

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Expect a cast instruction inserted by lowering the injected CastExpression
    REQUIRE(dumped.find("cast(") != std::string::npos);
}

TEST_CASE("DeclaredTypes: explicit num->int declaration using 'as' is accepted and lowers to SSA cast", "[declared_types]")
{
    std::stringstream stream("let a:int = 1.0 as int; a");
    Environment env;
    auto ast = env.parse(stream);
    REQUIRE(ast != nullptr); // explicit cast must be accepted by typechecker

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    REQUIRE(dumped.find("cast(") != std::string::npos);
}

TEST_CASE("DeclaredTypes: implicit num->int declaration is rejected (requires explicit cast)", "[declared_types]")
{
    std::stringstream stream("let a:int = 1.0; a");
    Environment env;
    env.reporter().setQuiet(true); // Keep it silent as an error will be triggered
    auto ast = env.parse(stream);
    REQUIRE(ast == nullptr); // typechecker should reject implicit narrowing
}
