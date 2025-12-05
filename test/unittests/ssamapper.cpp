#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"

using namespace PExpr;
using namespace PExpr::ssa;
using namespace PExpr::internal;

TEST_CASE("SSAMapper: simple variable and expression", "[ssamapper]")
{
    std::stringstream stream("let mut x = 1; x+2");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = prog.dump();

    // Expect an assignment for x, and a return
    REQUIRE(dumped.find("assign(") != std::string::npos);
    REQUIRE(dumped.find("x.") != std::string::npos);
    REQUIRE(dumped.find("return ") != std::string::npos);
}

TEST_CASE("SSAMapper: function declaration and call", "[ssamapper]")
{
    std::stringstream stream("fn f(a:int) = a; f(1)");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = prog.dump();

    // Expect a function named '_Z1f*' (mangled) and a call to f in main body
    REQUIRE(dumped.find("fn _Z1f") != std::string::npos);
    REQUIRE(dumped.find("call _Z1f") != std::string::npos);
}

TEST_CASE("SSAMapper: branch produces phi", "[ssamapper]")
{
    std::stringstream stream("if true { 1 } else { 2 }");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = prog.dump();

    // Expect a phi node for merged branch results
    REQUIRE(dumped.find("phi(") != std::string::npos);
}
