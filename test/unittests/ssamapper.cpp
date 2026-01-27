#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;

TEST_CASE("SSAMapper: simple variable and expression", "[ssamapper]")
{
    Environment env;
    auto ast    = env.parse("let mut x = 1; x+2");
    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Expect an assignment for x, and a return
    REQUIRE(dumped.find("assign(") != std::string::npos);
    REQUIRE(dumped.find("x.") != std::string::npos);
    REQUIRE(dumped.find("return ") != std::string::npos);
}

TEST_CASE("SSAMapper: function declaration and call", "[ssamapper]")
{
    Environment env;
    auto ast    = env.parse("fn f(a:int) = a; f(1)");
    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Expect a function named '_Z1f*' (mangled) and a call to f in main body
    REQUIRE(dumped.find("fn _Z1f") != std::string::npos);
    REQUIRE(dumped.find("call[_Z1f") != std::string::npos);
}

TEST_CASE("SSAMapper: branch produces phi", "[ssamapper]")
{
    Environment env;
    auto ast    = env.parse("if true { 1 } else { 2 }");
    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Expect a phi node for merged branch results
    REQUIRE(dumped.find("phi[") != std::string::npos);
}

TEST_CASE("SSAMapper: recursion function mapping", "[ssamapper]")
{
    Environment env;
    auto ast  = env.parse("fn fact(n:int) -> int = if n < 2 { 1 } else { n * fact(n - 1) }; fact(5)");
    auto prog = env.map(ast);

    bool foundFunc = false;
    for (const auto& f : prog.Functions) {
        if (f.Name.find("fact") != std::string::npos) {
            foundFunc = true;
            break;
        }
    }
    REQUIRE(foundFunc);

    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(dumped.find("call") != std::string::npos);
}
