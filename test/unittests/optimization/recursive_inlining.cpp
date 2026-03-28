#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "Environment.h"
#include "opt/SSAOptimizer.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;

TEST_CASE("SSAOptimizer: recursive function detection with force inlining", "[sscp][inlining][recursion]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Recursive factorial function
        fn fact(n:int) -> int = if n < 2 { 1 } else { n * fact(n - 1) };
        fact(5)
    )");

    auto prog = env.map(ast);

    // Check for function definition before optimization
    auto before = SSASerializer::serialize(prog);
    REQUIRE(before.find("fn _Z4fact") != std::string::npos);   // Function should exist
    REQUIRE(before.find("call[_Z4fact") != std::string::npos); // Should have recursive call

    // Run optimization with force inlining - this should NOT hang due to infinite recursion
    auto opts                 = opt::OptimizerOptions::None();
    opts.ForceInlineFunctions = true;
    opts.RemoveDeadCode       = true;

    opt::SSAOptimizer::Run(opts, prog);

    auto after = SSASerializer::serialize(prog);

    // After optimization with recursion detection, the function should still exist
    // (since recursive functions shouldn't be force-inlined)
    REQUIRE(after.find("fn _Z4fact") != std::string::npos);
    // The call might still be there or might have been partially inlined up to a limit
    // but we shouldn't have infinite inlining
}

TEST_CASE("SSAOptimizer: mutual recursion detection with force inlining", "[sscp][inlining][mutual-recursion]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Mutual recursion: even calls odd, odd calls even
        fn even(n:int) -> bool = if n == 0 { true } else { odd(n - 1) };
        fn odd(n:int) -> bool = if n == 0 { false } else { even(n - 1) };
        even(10)
    )");

    auto prog = env.map(ast);

    // Check for function definitions before optimization
    auto before = SSASerializer::serialize(prog);
    REQUIRE(before.find("fn _Z4even") != std::string::npos);
    REQUIRE(before.find("fn _Z3odd") != std::string::npos);
    REQUIRE(before.find("call[_Z3odd") != std::string::npos);
    REQUIRE(before.find("call[_Z4even") != std::string::npos);

    // Run optimization with force inlining
    auto opts                 = opt::OptimizerOptions::None();
    opts.ForceInlineFunctions = true;
    opts.RemoveDeadCode       = true;

    // This should not hang due to mutual recursion detection
    opt::SSAOptimizer::Run(opts, prog);

    auto after = SSASerializer::serialize(prog);

    // After optimization with recursion detection, both functions should still exist
    REQUIRE(after.find("fn _Z4even") != std::string::npos);
    REQUIRE(after.find("fn _Z3odd") != std::string::npos);
}

TEST_CASE("SSAOptimizer: force inlining non-recursive function", "[sscp][inlining][force][non-recursive]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Non-recursive function that should be force-inlined
        fn addOne(x:int) -> int = x + 1;
        fn addTwo(x:int) -> int = addOne(addOne(x));
        
        @[extern] fn getInput() -> int;
        let a = getInput();
        let b = addTwo(a);
        b
    )");

    auto prog = env.map(ast);

    // Check for function definitions before optimization
    auto before = SSASerializer::serialize(prog);
    REQUIRE(before.find("fn _Z6addOne") != std::string::npos);
    REQUIRE(before.find("fn _Z6addTwo") != std::string::npos);
    REQUIRE(before.find("call[_Z6addTwo") != std::string::npos);

    // Run optimization with force inlining
    auto opts                 = opt::OptimizerOptions::None();
    opts.ForceInlineFunctions = true;
    opts.RemoveDeadCode       = true;

    opt::SSAOptimizer::Run(opts, prog);

    auto after = SSASerializer::serialize(prog);

    // Non-recursive functions should be force-inlined and removed
    REQUIRE(after.find("fn _Z6addOne") == std::string::npos);
    REQUIRE(after.find("fn _Z6addTwo") == std::string::npos);
    REQUIRE(after.find("call[_Z6addTwo") == std::string::npos);

    // Should have direct arithmetic operations instead
    REQUIRE(after.find("add(") != std::string::npos);
}

TEST_CASE("SSAOptimizer: indirect recursion detection", "[sscp][inlining][indirect-recursion]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Indirect recursion through multiple functions
        fn a(n:int) -> int = if n <= 0 { 0 } else { b(n - 1) + 1 };
        fn b(n:int) -> int = if n <= 0 { 0 } else { c(n - 1) + 2 };
        fn c(n:int) -> int = if n <= 0 { 0 } else { a(n - 1) + 3 };
        
        a(5)
    )");

    auto prog = env.map(ast);

    // Check for function definitions before optimization
    auto before = SSASerializer::serialize(prog);
    REQUIRE(before.find("fn _Z1a") != std::string::npos);
    REQUIRE(before.find("fn _Z1b") != std::string::npos);
    REQUIRE(before.find("fn _Z1c") != std::string::npos);

    // Run optimization with force inlining
    auto opts                 = opt::OptimizerOptions::None();
    opts.ForceInlineFunctions = true;
    opts.RemoveDeadCode       = true;

    // This should not hang due to indirect recursion detection (cycle a->b->c->a)
    opt::SSAOptimizer::Run(opts, prog);

    auto after = SSASerializer::serialize(prog);

    // All functions in the cycle should still exist (not force-inlined)
    REQUIRE(after.find("fn _Z1a") != std::string::npos);
    REQUIRE(after.find("fn _Z1b") != std::string::npos);
    REQUIRE(after.find("fn _Z1c") != std::string::npos);
}