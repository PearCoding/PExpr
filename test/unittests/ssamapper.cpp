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

TEST_CASE("SSAMapper: mutable capture with assignment returns correct version", "[ssamapper][uplift]")
{
    Environment env;
    // This test specifically checks the bug where uplifted mutable capture
    // with assignment doesn't update variable version in SSAMapper
    auto ast = env.parse("let mut x = 5; fn increment() = { x = x + 1; x }; let k = increment(); x");
    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // The function should be uplifted to accept x as mutable parameter
    REQUIRE(dumped.find("fn _Z9increment_Pi") != std::string::npos);
    
    // Check that x is assigned version .1 initially
    REQUIRE(dumped.find("x.1:int = assign(5:int)") != std::string::npos);
    
    // Check that x gets updated to version .3 inside the closure
    // (x.2 is inside the function, x.3 is after assignment)
    REQUIRE(dumped.find("x.3:int = assign(") != std::string::npos);
    
    // The final return should be x.3 not x.1
    REQUIRE(dumped.find("return x.3:int") != std::string::npos);
    
    // Ensure x.1 is not returned (the bug)
    REQUIRE(dumped.find("return x.1:int") == std::string::npos);
}

TEST_CASE("SSAMapper: variable version tracking in nested scopes", "[ssamapper]")
{
    Environment env;
    // Test variable version tracking across nested scopes without uplifting
    auto ast = env.parse("{ let mut x = 1; let k = { x = x + 1; x }; x }");
    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // x.1 = 1, x.2 = x.1 + 1
    REQUIRE(dumped.find("x.1:int = assign(1:int)") != std::string::npos);
    REQUIRE(dumped.find("x.2:int = assign(") != std::string::npos);
    // Should return the updated value, not x.1
    REQUIRE(dumped.find("return x.1:int") == std::string::npos);
}

TEST_CASE("SSAMapper: variable version tracking with function inlining", "[ssamapper]")
{
    Environment env;
    // Test that variable versions are tracked correctly when functions are inlined
    auto ast = env.parse("let mut x = 1; fn f() = { x = x + 1; x }; let r = f(); x");
    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // x.1 = 1, x.2 = x.1 + 1 (inside function)
    REQUIRE(dumped.find("x.1:int = assign(1:int)") != std::string::npos);
    REQUIRE(dumped.find("x.2:int = assign(") != std::string::npos);
    // Should return the updated value, not x.1
    REQUIRE(dumped.find("return x.1:int") == std::string::npos);
}

TEST_CASE("SSAMapper: multiple assignments in nested scopes", "[ssamapper]")
{
    Environment env;
    // Test multiple assignments in different scopes
    auto ast = env.parse("let mut x = 1; let k = { x = x + 1; { x = x + 2; x } }; x");
    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // x.1 = 1, x.3 = x.2 + 2 (or similar)
    REQUIRE(dumped.find("x.1:int = assign(1:int)") != std::string::npos);
    // Should have x.3 (or higher) due to two increments
    // Should return the updated value, not x.1
    REQUIRE(dumped.find("return x.1:int") == std::string::npos);
}

TEST_CASE("SSAMapper: variable shadowing preserves version tracking", "[ssamapper]")
{
    Environment env;
    // Test that shadowed variables don't interfere with version tracking
    auto ast = env.parse("let mut x = 1; let k = { let x = 5; x }; x = x + 1; x");
    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Outer x: x.1 = 1, x.2 = x.1 + 1
    REQUIRE(dumped.find("x.1:int = assign(1:int)") != std::string::npos);
    REQUIRE(dumped.find("x.2:int = assign(") != std::string::npos);
    // Should return the updated value, not x.1
    REQUIRE(dumped.find("return x.1:int") == std::string::npos);
}
