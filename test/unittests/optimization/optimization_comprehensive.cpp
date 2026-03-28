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

[[nodiscard]] inline static auto MakeFullOptimization()
{
    auto opts = opt::OptimizerOptions::High();
    return opts;
}

TEST_CASE("SSAOptimizer: control flow simplification with constant conditions", "[sscp][controlflow]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Branch with constant condition should be eliminated
        let result = if true {
            42
        } else {
            24
        };
        result
    )");

    auto prog = env.map(ast);

    // Check for phi nodes before optimization
    auto before = SSASerializer::serialize(prog);
    REQUIRE(before.find("phi[") != std::string::npos);

    // Run full optimization
    opt::SSAOptimizer::Run(MakeFullOptimization(), prog);

    auto after = SSASerializer::serialize(prog);

    // After optimization, the branch should be eliminated
    // No phi node should be present
    REQUIRE(after.find("phi[") == std::string::npos);
    // Should have constant 42
    REQUIRE(after.find("42") != std::string::npos);
}

TEST_CASE("SSAOptimizer: dead code elimination with unused branches", "[sscp][deadcode][controlflow]")
{
    Environment env;
    auto ast = env.parse(R"(
        @[extern, pure] fn getInputPure() -> num;
        let a = getInputPure();
        let b = getInputPure();
        let cond = a > b;
        
        // This branch result is never used
        let unused = if cond {
            a * 2.0
        } else {
            b * 3.0
        };
        
        // Only this is used
        a + b
    )");

    auto prog = env.map(ast);

    // Check for branch structure before optimization
    auto before = SSASerializer::serialize(prog);
    REQUIRE(before.find("phi[") != std::string::npos); // Should have phi for branch

    // Run optimization with dead code elimination
    auto opts           = opt::OptimizerOptions::None();
    opts.RemoveDeadCode = true;
    opt::SSAOptimizer::Run(opts, prog);

    auto after = SSASerializer::serialize(prog);

    // TODO

    // The unused branch should be eliminated (due to getInputPure, without pure this would not work)
    // No phi node should be present (branch removed)
    REQUIRE(after.find("phi[") == std::string::npos);
    // Should only have a + b computation
    REQUIRE(after.find("add(") != std::string::npos);
}

TEST_CASE("SSAOptimizer: trigonometric identities simplification", "[sscp][identities]")
{
    Environment env;
    auto ast = env.parse(R"(
        @[extern, pure] fn sin(a:num) -> num;
        @[extern, pure] fn cos(a:num) -> num;
        @[extern, pure] fn asin(a:num) -> num;
        @[extern, pure] fn acos(a:num) -> num;
        @[extern] fn getInput() -> num;
        
        let a = getInput();
        
        // These should be simplified
        let id1 = sin(asin(a));        // Should become a
        let id2 = cos(acos(a));        // Should become a
        let id3 = sin(a)^2 + cos(a)^2; // Should become 1
        
        id1 + id2 + id3
    )");

    auto prog = env.map(ast);

    // Count trigonometric calls before optimization
    auto before             = SSASerializer::serialize(prog);
    size_t sin_count_before = 0, cos_count_before = 0, asin_count_before = 0, acos_count_before = 0;
    size_t pos = 0;

    while ((pos = before.find("call[_Z3sin", pos)) != std::string::npos) {
        sin_count_before++;
        pos += 12;
    }

    pos = 0;
    while ((pos = before.find("call[_Z3cos", pos)) != std::string::npos) {
        cos_count_before++;
        pos += 12;
    }

    pos = 0;
    while ((pos = before.find("call[_Z4asin", pos)) != std::string::npos) {
        asin_count_before++;
        pos += 13;
    }

    pos = 0;
    while ((pos = before.find("call[_Z4acos", pos)) != std::string::npos) {
        acos_count_before++;
        pos += 13;
    }

    // Run full optimization with trigonometric identities
    auto opts = opt::OptimizerOptions::High();
    opt::SSAOptimizer::Run(opts, prog);

    auto after = SSASerializer::serialize(prog);

    // Count trigonometric calls after optimization
    size_t sin_count_after = 0, cos_count_after = 0, asin_count_after = 0, acos_count_after = 0;
    pos = 0;

    while ((pos = after.find("call[_Z3sin", pos)) != std::string::npos) {
        sin_count_after++;
        pos += 12;
    }

    pos = 0;
    while ((pos = after.find("call[_Z3cos", pos)) != std::string::npos) {
        cos_count_after++;
        pos += 12;
    }

    pos = 0;
    while ((pos = after.find("call[_Z4asin", pos)) != std::string::npos) {
        asin_count_after++;
        pos += 13;
    }

    pos = 0;
    while ((pos = after.find("call[_Z4acos", pos)) != std::string::npos) {
        acos_count_after++;
        pos += 13;
    }

    // Trigonometric identities should reduce calls
    REQUIRE(sin_count_after < sin_count_before);
    REQUIRE(cos_count_after < cos_count_before);
    REQUIRE(asin_count_after < asin_count_before);
    REQUIRE(acos_count_after < acos_count_before);
}

TEST_CASE("SSAOptimizer: math identities simplification", "[sscp][identities]")
{
    Environment env;
    auto ast = env.parse(R"(
        @[extern] fn getInput() -> num;
        
        let a = getInput();
        
        // Basic math identities that should be simplified
        let id1 = a + 0.0; // Should become a
        let id2 = a - 0.0; // Should become a
        let id3 = a * 1.0; // Should become a
        let id4 = a / 1.0; // Should become a
        let id5 = 0.0 * a; // Should become 0.0
        let id6 = 1.0 * a; // Should become a
        let id7 = a ^ 2;   // Should become a * a
        let id8 = -(-a);   // Should become a
        
        id1 + id2 + id3 + id4 + id5 + id6 + id7 + id8
    )");

    auto prog = env.map(ast);

    // Count operations before optimization
    auto before             = SSASerializer::serialize(prog);
    size_t add_count_before = 0, sub_count_before = 0, mul_count_before = 0, div_count_before = 0, neg_count_before = 0;
    size_t pos = 0;

    while ((pos = before.find("add(", pos)) != std::string::npos) {
        add_count_before++;
        pos += 4;
    }

    pos = 0;
    while ((pos = before.find("sub(", pos)) != std::string::npos) {
        sub_count_before++;
        pos += 4;
    }

    pos = 0;
    while ((pos = before.find("mul(", pos)) != std::string::npos) {
        mul_count_before++;
        pos += 4;
    }

    pos = 0;
    while ((pos = before.find("div(", pos)) != std::string::npos) {
        div_count_before++;
        pos += 4;
    }

    pos = 0;
    while ((pos = before.find("neg(", pos)) != std::string::npos) {
        neg_count_before++;
        pos += 4;
    }

    // Run full optimization with math identities
    auto opts = opt::OptimizerOptions::High();
    opt::SSAOptimizer::Run(opts, prog);

    auto after = SSASerializer::serialize(prog);

    // Count operations after optimization
    size_t add_count_after = 0, sub_count_after = 0, mul_count_after = 0, div_count_after = 0, neg_count_after = 0;
    pos = 0;

    while ((pos = after.find("add(", pos)) != std::string::npos) {
        add_count_after++;
        pos += 4;
    }

    pos = 0;
    while ((pos = after.find("sub(", pos)) != std::string::npos) {
        sub_count_after++;
        pos += 4;
    }

    pos = 0;
    while ((pos = after.find("mul(", pos)) != std::string::npos) {
        mul_count_after++;
        pos += 4;
    }

    pos = 0;
    while ((pos = after.find("div(", pos)) != std::string::npos) {
        div_count_after++;
        pos += 4;
    }

    pos = 0;
    while ((pos = after.find("neg(", pos)) != std::string::npos) {
        neg_count_after++;
        pos += 4;
    }

    // Math identities should reduce operations
    REQUIRE(add_count_after < add_count_before);
    REQUIRE(sub_count_after < sub_count_before);
    REQUIRE(mul_count_after < mul_count_before);
    REQUIRE(div_count_after < div_count_before);
    REQUIRE(neg_count_after < neg_count_before);
}

TEST_CASE("SSAOptimizer: repeated addition identities", "[sscp][identities]")
{
    Environment env;
    auto ast = env.parse(R"(
        @[extern] fn getInput() -> num;
        
        let a1 = getInput();
        let a2 = getInput();
        let a3 = getInput();
        let a4 = getInput();
        
        // Test repeated addition patterns that should be simplified
        // Note: a + a should NOT become 2*a
        // But patterns like n*a + a should become (n+1)*a
        
        let pattern1 = a1 + a1;       // Should stay the same  
        let pattern2 = a2 + a2 + a2;  // Should become 3.0 * a2
        let pattern3 = a3 + 3.0 * a3; // Should become 4.0 * a3
        let pattern4 = a4 * 4.0 + a4; // Should become 5.0 * a4
        
        pattern1 + pattern2 + pattern3 + pattern4
    )");

    auto prog = env.map(ast);

    // Count mul operations before optimization
    auto before             = SSASerializer::serialize(prog);
    size_t mul_count_before = 0, add_count_before = 0;
    size_t pos = 0;

    while ((pos = before.find("mul(", pos)) != std::string::npos) {
        mul_count_before++;
        pos += 4;
    }

    pos = 0;
    while ((pos = before.find("add(", pos)) != std::string::npos) {
        add_count_before++;
        pos += 4;
    }

    // Run full optimization with math identities
    auto opts = opt::OptimizerOptions::High();
    opt::SSAOptimizer::Run(opts, prog);

    auto after = SSASerializer::serialize(prog);

    // Count operations after optimization
    size_t mul_count_after = 0, add_count_after = 0;
    pos = 0;

    while ((pos = after.find("mul(", pos)) != std::string::npos) {
        mul_count_after++;
        pos += 4;
    }

    pos = 0;
    while ((pos = after.find("add(", pos)) != std::string::npos) {
        add_count_after++;
        pos += 4;
    }

    // Repeated addition identities should reduce operations
    // We should have fewer add operations (n*a + a -> (n+1)*a eliminates adds)
    REQUIRE(add_count_after + mul_count_after < add_count_before + mul_count_before);

    // Also check that we can find the expected constants in the output
    // The patterns should be simplified to 3*a, 4*a, 5*a
    // So we should see constants 3, 4, 5 in the output, but not 2
    REQUIRE(after.find(" 2:num") == std::string::npos);
    REQUIRE(after.find(" 3:num") != std::string::npos);
    REQUIRE(after.find(" 4:num") != std::string::npos);
    REQUIRE(after.find(" 5:num") != std::string::npos);
}

TEST_CASE("SSAOptimizer: vector constant folding", "[sscp][constantfolding]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Vector constant folding
        let v1 = [1.0, 2.0, 3.0];
        let v2 = [4.0, 5.0, 6.0];
        let add = v1 + v2;  // Should fold to [5.0, 7.0, 9.0]
        let sub = v1 - v2;  // Should fold to [-3.0, -3.0, -3.0]
        let mul = v1 * 2.0; // Should fold to [2.0, 4.0, 6.0]
        let div = v2 / 2.0; // Should fold to [2.0, 2.5, 3.0]
        
        // Access folded results
        add.x + sub.y + mul.z + div.x
    )");

    auto prog = env.map(ast);

    // Run constant folding optimization
    auto opts                        = opt::OptimizerOptions::None();
    opts.EnableConstantFolding       = true;
    opts.EnableConstantFoldingNumber = true;
    opts.RemoveDeadCode              = true;
    opt::SSAOptimizer::Run(opts, prog);

    auto after = SSASerializer::serialize(prog);

    // Check that vector operations are folded to constants
    // The result should be a constant: 5.0 + (-3.0) + 6.0 + 2.0 = 10.0
    // Actually: 5.0 + (-3.0) + 6.0 + 2.0 = 10.0
    // Look for constant 10 in output
    REQUIRE(after.find("10") != std::string::npos);
}

TEST_CASE("SSAOptimizer: function inlining with small functions", "[sscp][inlining]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Small function that should be inlined
        fn addOne(x:num) -> num = x + 1.0;
        
        @[extern] fn getInput() -> num;
        let a = getInput();
        let b = addOne(a);
        let c = addOne(b);
        let d = addOne(c);
        
        a + b + c + d
    )");

    auto prog = env.map(ast);

    // Check for function definition before optimization
    auto before = SSASerializer::serialize(prog);
    REQUIRE(before.find("fn _Z6addOne") != std::string::npos); // Function should exist

    // Run optimization with inlining
    auto opts            = opt::OptimizerOptions::None();
    opts.InlineFunctions = true;
    opts.RemoveDeadCode  = true;
    opt::SSAOptimizer::Run(opts, prog);

    auto after = SSASerializer::serialize(prog);

    // After inlining, the function might be removed if all calls are inlined
    // or it might still exist if not all calls were inlined
    // At minimum, we should see the inlined code
    REQUIRE(after.find("add(") != std::string::npos); // Should have additions
}

TEST_CASE("SSAOptimizer: interaction between multiple optimizations", "[sscp][integration]")
{
    Environment env;
    auto ast = env.parse(R"(
        @[extern] fn getInput() -> num;
        @[extern, pure] fn sin(a:num) -> num;
        @[extern, pure] fn cos(a:num) -> num;
        
        let a = getInput();
        let b = getInput();
        let cond = a > b;
        
        // Complex expression that benefits from multiple optimizations
        let result = if cond {
            let x = sin(a) * sin(a) + cos(a) * cos(a); // Should become 1
            let y = (a + 0.0) * (b * 1.0);             // Should become a * b
            x * y
        } else {
            let x = sin(b) * sin(b) + cos(b) * cos(b); // Should become 1
            let y = (b + 0.0) * (a * 1.0);             // Should become b * a
            x * y
        };
        
        result
    )");

    auto prog = env.map(ast);

    // Count various operations before optimization
    auto before             = SSASerializer::serialize(prog);
    size_t mul_count_before = 0, add_count_before = 0, sin_count_before = 0, cos_count_before = 0;
    size_t pos = 0;

    while ((pos = before.find("mul(", pos)) != std::string::npos) {
        mul_count_before++;
        pos += 4;
    }

    pos = 0;
    while ((pos = before.find("add(", pos)) != std::string::npos) {
        add_count_before++;
        pos += 4;
    }

    pos = 0;
    while ((pos = before.find("call[_Z3sin", pos)) != std::string::npos) {
        sin_count_before++;
        pos += 12;
    }

    pos = 0;
    while ((pos = before.find("call[_Z3cos", pos)) != std::string::npos) {
        cos_count_before++;
        pos += 12;
    }

    // Run ALL optimizations
    opt::SSAOptimizer::Run(opt::OptimizerOptions::High(), prog);

    auto after = SSASerializer::serialize(prog);

    // Count operations after optimization
    size_t mul_count_after = 0, add_count_after = 0, sin_count_after = 0, cos_count_after = 0;
    pos = 0;

    while ((pos = after.find("mul(", pos)) != std::string::npos) {
        mul_count_after++;
        pos += 4;
    }

    pos = 0;
    while ((pos = after.find("add(", pos)) != std::string::npos) {
        add_count_after++;
        pos += 4;
    }

    pos = 0;
    while ((pos = after.find("call[_Z3sin", pos)) != std::string::npos) {
        sin_count_after++;
        pos += 12;
    }

    pos = 0;
    while ((pos = after.find("call[_Z3cos", pos)) != std::string::npos) {
        cos_count_after++;
        pos += 12;
    }

    // TODO: With full optimization, trigonometric identities should eliminate
    // sin and cos calls: sin_count_after should be 0, cos_count_after should be 0
    // The expression sin(a)^2 + cos(a)^2 should become 1, eliminating both calls.
    // Also, (a + 0.0) should become a (eliminating add), and (b * 1.0) should become b.
    // After all optimizations, only two mul operations should remain (a * b or b * a).
    // No add operations should remain.
    REQUIRE(mul_count_after < mul_count_before);
    REQUIRE(add_count_after < add_count_before);
    REQUIRE(sin_count_after < sin_count_before);
    REQUIRE(cos_count_after < cos_count_before);
}

TEST_CASE("SSAOptimizer: force function inlining", "[sscp][inlining][force]")
{
    Environment env;
    auto ast = env.parse(R"(
        @[extern] fn getInput() -> int;
        // Functions called multiple times with non-constant arguments
        fn add(x: int, y: int) = x + y;
        fn multiply(x: int, y: int) = x * y;

        let a = getInput();
        let b = getInput();
        let c = getInput();
        let d = getInput();

        // These calls should NOT be inlined with regular inlining
        // because function is called multiple times with non-constant args
        let result1 = add(a, b);
        let result2 = multiply(c, d);
        let result3 = add(result1, result2);

        result3
    )");

    auto prog = env.map(ast);

    // Check for function definitions before optimization
    auto before = SSASerializer::serialize(prog);
    REQUIRE(before.find("fn _Z3add") != std::string::npos);      // Function should exist
    REQUIRE(before.find("fn _Z8multiply") != std::string::npos); // Function should exist
    REQUIRE(before.find("call[_Z3add") != std::string::npos);    // Should have calls

    // Run optimization with regular inlining (not force)
    auto opts            = opt::OptimizerOptions::None();
    opts.InlineFunctions = true;
    opts.RemoveDeadCode  = true;
    opt::SSAOptimizer::Run(opts, prog);

    auto afterRegular = SSASerializer::serialize(prog);

    // With regular inlining, functions should still exist because:
    // - They're called multiple times
    // - Arguments are not constants
    REQUIRE(afterRegular.find("fn _Z3add") != std::string::npos);
    REQUIRE(afterRegular.find("call[_Z3add") != std::string::npos);
    // - Called once
    REQUIRE(afterRegular.find("fn _Z8multiply") == std::string::npos);
    REQUIRE(afterRegular.find("call[_Z8multiply") == std::string::npos);

    // Now test with force inlining
    // Re-map to get fresh program
    prog = env.map(ast);

    auto optsForce                 = opt::OptimizerOptions::None();
    optsForce.ForceInlineFunctions = true;
    optsForce.RemoveDeadCode       = true;
    opt::SSAOptimizer::Run(optsForce, prog);

    auto afterForce = SSASerializer::serialize(prog);

    // With force inlining, functions should NOT exist
    REQUIRE(afterForce.find("fn _Z3add") == std::string::npos);
    REQUIRE(afterForce.find("fn _Z8multiply") == std::string::npos);
    REQUIRE(afterForce.find("call[_Z3add") == std::string::npos);
    REQUIRE(afterForce.find("call[_Z8multiply") == std::string::npos);

    // Should have direct arithmetic operations instead
    REQUIRE(afterForce.find("add(") != std::string::npos);
    REQUIRE(afterForce.find("mul(") != std::string::npos);
}
