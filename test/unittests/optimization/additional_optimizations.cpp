#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "opt/SSAOptimizer.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;

[[nodiscard]] inline static auto MakeAllOptimizations()
{
    return opt::OptimizerOptions::High();
}

TEST_CASE("SSAOptimizer: constant propagation through branches", "[sscp][constantprop]")
{
    Environment env;
    auto ast = env.parse(R"(
        @[extern] fn getInput() -> int;
        let a = getInput();
        let b = getInput();
        let cond = a == b;
        
        // Constant propagation should eliminate this branch if a and b are known
        let result = if cond {
            a + b
        } else {
            a - b
        };
        
        result
    )");

    auto prog = env.map(ast);

    // Run optimization
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);

    auto dumped = SSASerializer::serialize(prog);

    // Just check that optimization runs without crashing
    REQUIRE(!dumped.empty());
}

TEST_CASE("SSAOptimizer: loop invariant code motion", "[sscp][licm]")
{
    Environment env;
    auto ast = env.parse(R"(
        @[extern] fn getValue() -> num;
        
        // Simulate loop with recursion
        fn process(n:int, acc:num) -> num = {
            let invariant = getValue() * 2.0;  // Should be moved outside
            if n <= 0 {
                acc
            } else {
                process(n - 1, acc + invariant)
            }
        };
        
        process(5, 0.0)
    )");

    auto prog = env.map(ast);

    // Run optimization
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);

    auto dumped = SSASerializer::serialize(prog);

    // Just check that optimization runs without crashing
    REQUIRE(!dumped.empty());
}

TEST_CASE("SSAOptimizer: strength reduction", "[sscp][strength]")
{
    Environment env;
    auto ast = env.parse(R"(
        @[extern] fn getInput() -> int;
        let a = getInput();
        
        // Multiplication by powers of 2 could be strength reduced to shifts
        let mul2 = a * 2;
        let mul4 = a * 4;
        let mul8 = a * 8;
        
        // Division by powers of 2
        let div2 = a / 2;
        let div4 = a / 4;
        
        mul2 + mul4 + mul8 + div2 + div4
    )");

    auto prog = env.map(ast);

    // Run optimization
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);

    auto dumped = SSASerializer::serialize(prog);

    // Just check that optimization runs without crashing
    REQUIRE(!dumped.empty());
}

TEST_CASE("SSAOptimizer: algebraic simplifications", "[sscp][algebraic]")
{
    Environment env;
    auto ast = env.parse(R"(
        @[extern] fn getInput() -> num;
        let a = getInput();
        let b = getInput();
        
        // Various algebraic simplifications
        let expr1 = (a + b) - a;   // Could become b
        let expr2 = a * b / a;     // Could become b (if a != 0)
        let expr3 = a * 0.0;       // Should become 0.0
        let expr4 = a + a;         // Could become 2*a
        let expr5 = a * a - a * a; // Should become 0.0
        
        expr1 + expr2 + expr3 + expr4 + expr5
    )");

    auto prog = env.map(ast);

    // Run optimization
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);

    auto dumped = SSASerializer::serialize(prog);

    // Just check that optimization runs without crashing
    REQUIRE(!dumped.empty());
}

TEST_CASE("SSAOptimizer: conditional constant propagation", "[sscp][condconst]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Nested conditionals with constants
        let x = if true {
            let y = if false { 1 } else { 2 };
            y * 2
        } else {
            0
        };
        
        x
    )");

    auto prog = env.map(ast);

    // Run optimization
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);

    auto dumped = SSASerializer::serialize(prog);

    // Check that we get some constant (ideally 4)
    REQUIRE(!dumped.empty());
}

TEST_CASE("SSAOptimizer: function specialization", "[sscp][specialization]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Function that could be specialized for constant arguments
        fn compute(x:int, y:int) -> int = x * y + x - y;
        
        let result1 = compute(5, 3);
        let result2 = compute(10, 2);
        
        result1 + result2
    )");

    auto prog = env.map(ast);

    // Run optimization
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);

    auto dumped = SSASerializer::serialize(prog);

    // Just check that optimization runs without crashing
    REQUIRE(!dumped.empty());
}

TEST_CASE("SSAOptimizer: tail recursion optimization", "[sscp][tailrec]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Tail recursive function
        fn factorial(n:int, acc:int) -> int = 
            if n <= 1 { acc } else { factorial(n - 1, acc * n) };
        
        factorial(5, 1)
    )");

    auto prog = env.map(ast);

    // Run optimization
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);

    auto dumped = SSASerializer::serialize(prog);

    // TODO: Not yet implemented
    // Just check that optimization runs without crashing
    REQUIRE(!dumped.empty());
}

// --- Logical identity tests ---

TEST_CASE("SSAOptimizer: a || false = a", "[sscp][identity][logical]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getBool() -> bool;
        let a = getBool();
        a || false
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);
    auto dumped = SSASerializer::serialize(prog);
    // The or operation should be eliminated
    REQUIRE(dumped.find("or(") == std::string::npos);
}

TEST_CASE("SSAOptimizer: false || a = a", "[sscp][identity][logical]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getBool() -> bool;
        let a = getBool();
        false || a
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);
    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(dumped.find("or(") == std::string::npos);
}

TEST_CASE("SSAOptimizer: a || true = true", "[sscp][identity][logical]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getBool() -> bool;
        let a = getBool();
        a || true
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);
    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(dumped.find("or(") == std::string::npos);
    REQUIRE(dumped.find("true") != std::string::npos);
}

TEST_CASE("SSAOptimizer: a && true = a", "[sscp][identity][logical]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getBool() -> bool;
        let a = getBool();
        a && true
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);
    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(dumped.find("and(") == std::string::npos);
}

TEST_CASE("SSAOptimizer: a && false = false", "[sscp][identity][logical]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getBool() -> bool;
        let a = getBool();
        a && false
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);
    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(dumped.find("and(") == std::string::npos);
    REQUIRE(dumped.find("false") != std::string::npos);
}

TEST_CASE("SSAOptimizer: num self-identity NOT applied at O2 (IEEE-754 safe)", "[sscp][identity][logical]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getInput() -> num;
        let a = getInput();
        a - a
    )");
    auto prog = env.map(ast);
    // O2 has ApplyMathIdentities but NOT ApplyUnsafeMathIdentities
    opt::SSAOptimizer::Run(opt::OptimizerOptions::Medium(), prog);
    auto dumped = SSASerializer::serialize(prog);
    // The subtraction should still be present because num self-identities are unsafe
    REQUIRE(dumped.find("sub(") != std::string::npos);
}

TEST_CASE("SSAOptimizer: num self-identity applied at O3 (fast-math)", "[sscp][identity][logical]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getInput() -> num;
        let a = getInput();
        a - a
    )");
    auto prog = env.map(ast);
    // O3 has ApplyUnsafeMathIdentities
    opt::SSAOptimizer::Run(opt::OptimizerOptions::High(), prog);
    auto dumped = SSASerializer::serialize(prog);
    // The subtraction should be eliminated
    REQUIRE(dumped.find("sub(") == std::string::npos);
}

// --- Self-operand identity tests ---

TEST_CASE("SSAOptimizer: a - a = 0", "[sscp][identity][self]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getInput() -> int;
        let a = getInput();
        a - a
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);
    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(dumped.find("sub(") == std::string::npos);
    REQUIRE(dumped.find("0") != std::string::npos);
}

TEST_CASE("SSAOptimizer: a / a = 1", "[sscp][identity][self]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getInput() -> int;
        let a = getInput();
        a / a
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);
    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(dumped.find("div(") == std::string::npos);
}

TEST_CASE("SSAOptimizer: a == a = true", "[sscp][identity][self]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getInput() -> int;
        let a = getInput();
        a == a
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);
    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(dumped.find("eq(") == std::string::npos);
    REQUIRE(dumped.find("true") != std::string::npos);
}

TEST_CASE("SSAOptimizer: a != a = false", "[sscp][identity][self]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getInput() -> int;
        let a = getInput();
        a != a
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);
    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(dumped.find("neq(") == std::string::npos);
    REQUIRE(dumped.find("false") != std::string::npos);
}

TEST_CASE("SSAOptimizer: a && a = a (idempotent)", "[sscp][identity][self]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getBool() -> bool;
        let a = getBool();
        a && a
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);
    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(dumped.find("and(") == std::string::npos);
}

TEST_CASE("SSAOptimizer: a || a = a (idempotent)", "[sscp][identity][self]")
{
    Environment env;
    auto ast  = env.parse(R"(
        @[extern] fn getBool() -> bool;
        let a = getBool();
        a || a
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);
    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(dumped.find("or(") == std::string::npos);
}

TEST_CASE("SSAOptimizer: common expression elimination across functions", "[sscp][interprocedural]")
{
    Environment env;
    auto ast = env.parse(R"(
        @[extern] fn getInput() -> num;
        
        fn helper1(x:num) -> num = x * x + 2.0 * x + 1.0;
        fn helper2(y:num) -> num = y * y + 2.0 * y + 1.0;
        
        let a = getInput();
        let b = getInput();
        
        let result1 = helper1(a);
        let result2 = helper2(b);
        let result3 = helper1(b);
        
        result1 + result2 + result3
    )");

    auto prog = env.map(ast);

    // Run optimization
    opt::SSAOptimizer::Run(MakeAllOptimizations(), prog);

    auto dumped = SSASerializer::serialize(prog);

    // Just check that optimization runs without crashing
    REQUIRE(!dumped.empty());
}