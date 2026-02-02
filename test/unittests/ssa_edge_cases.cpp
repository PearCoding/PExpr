#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;

TEST_CASE("SSAMapper: complex nested closures with mutable captures", "[ssamapper][closures]")
{
    Environment env;
    auto ast = env.parse(R"(
        let mut x = 1;
        let mut y = 2;
        
        // Inner closure captures x, outer closure captures y
        fn outer() -> int = {
            fn inner() -> int = {
                x = x + 1;
                x
            };
            y = y + 1;
            inner() + y
        };
        
        let result = outer();
        x + y + result
    )");

    REQUIRE(ast != nullptr);

    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Should have uplifted functions with mutable parameters
    REQUIRE(dumped.find("fn") != std::string::npos);
    // Should have proper version tracking for x and y
    REQUIRE(dumped.find("x_L2C17.") != std::string::npos);
    REQUIRE(dumped.find("y_L3C17.") != std::string::npos);
}

TEST_CASE("SSAMapper: mutual recursion with type checking", "[ssamapper][recursion]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Mutual recursion: even calls odd, odd calls even
        fn even(n:int) -> bool = if n == 0 { true } else { odd(n - 1) };
        fn odd(n:int) -> bool = if n == 0 { false } else { even(n - 1) };
        
        even(5)
    )");

    REQUIRE(ast != nullptr);

    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Should have both even and odd functions
    REQUIRE(dumped.find("_Z4even") != std::string::npos);
    REQUIRE(dumped.find("_Z3odd") != std::string::npos);
    // Should have recursive calls
    REQUIRE(dumped.find("call") != std::string::npos);
}

TEST_CASE("SSAMapper: vector operations with SSA mapping", "[ssamapper][vectors]")
{
    Environment env;
    auto ast = env.parse(R"(
        let v1 = [1.0, 2.0, 3.0];
        let v2 = [4.0, 5.0, 6.0];
        
        // Various vector operations
        let add = v1 + v2;
        let sub = v1 - v2;
        let mul = v1 * 2.0;
        let div = v2 / 2.0;
        
        // Component access
        let x = add.x;
        let y = sub.y;
        let z = mul.z;
        
        // Swizzling
        let swizzle = v1.zyx;
        
        x + y + z + swizzle.x
    )");

    REQUIRE(ast != nullptr);

    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Should have vector operations
    REQUIRE(dumped.find("tuple[") != std::string::npos);
}

TEST_CASE("SSAMapper: destructuring with pattern matching", "[ssamapper][destructuring]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Complex destructuring patterns
        let tuple = [1, 2.0, [3, 4]];
        let *[a, b, [c, d]] = tuple;
        
        // Nested destructuring in function parameters
        fn process(pair:[int, num]) -> num = {
            let *[x, y] = pair;
            x + y
        };
        
        // Destructuring assignment with swap
        let mut m = 1;
        let mut n = 2;
        *[n, m] = [m, n];
        
        a + b + c + d + process([5, 6.0]) + m + n
    )");

    REQUIRE(ast != nullptr);

    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Should handle destructuring correctly
    REQUIRE(dumped.find("assign") != std::string::npos);
}

TEST_CASE("SSAMapper: loops and iteration patterns", "[ssamapper][controlflow]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Simulate loop with recursion
        fn sum(n:int, acc:int) -> int = if n <= 0 { acc } else { sum(n - 1, acc + n) };
        
        // While-like pattern with mutable variable
        let mut i = 10;
        let mut total = 0;
        fn iterate() -> int = {
            if i > 0 {
                total = total + i;
                i = i - 1;
                iterate()
            } else {
                0
            }
        };
        let k = iterate();
        
        sum(5, 0) + total
    )");

    REQUIRE(ast != nullptr);

    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Should have recursive calls for loop simulation
    REQUIRE(dumped.find("call") != std::string::npos);
    // Should have phi nodes for mutable variables in loops
    REQUIRE(dumped.find("phi[") != std::string::npos);
}

TEST_CASE("SSAMapper: type casting and conversions", "[ssamapper][casts]")
{
    Environment env;
    auto ast = env.parse(R"(
        // Explicit casts
        let int_to_num = 42 as num;
        let num_to_int = 3.14 as int;
        
        // Implicit conversions in expressions
        let mixed = 1 + 2.0;  // int + num -> num
        
        // Vector component type conversions
        let vec_mixed = [1, 2.0, 3];  // vec3 with mixed types
        
        // Function with type conversion
        fn toInt(x:num) -> int = x as int;
        
        int_to_num + num_to_int + mixed + vec_mixed.x + toInt(7.5)
    )");

    REQUIRE(ast != nullptr);

    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Should have cast operations
    REQUIRE(dumped.find("cast") != std::string::npos);
}

TEST_CASE("SSAMapper: error recovery and invalid patterns", "[ssamapper][errors]")
{
    Environment env;

    SECTION("Multiple assignments to same immutable variable")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            let x = 1;
            x = 2;  // Should fail - x is immutable
            x
        )");

        // This should produce an error
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Type mismatch in assignment")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            let mut x:num = 1.0;
            x = true;  // Should fail - type mismatch
            x
        )");

        // This should produce an error
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Undefined variable")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            undefined + 1  // Should fail - undefined variable
        )");

        // This should produce an error
        REQUIRE(env.reporter().errorCount() > 0);
    }
}

TEST_CASE("SSAMapper: complex expression trees", "[ssamapper][expressions]")
{
    Environment env;
    auto ast = env.parse(R"(
        [[extern, pure]] fn sin(a:num) -> num;
        [[extern, pure]] fn cos(a:num) -> num;
        [[extern, pure]] fn exp(a:num) -> num;
        
        // Complex mathematical expression
        let a = 1.5;
        let b = 2.5;
        let c = 3.5;
        
        let result = sin(a * b) * cos(b / c) + exp(a - b) * (a^2 + b^2 - c^2) / (a * b * c);
        
        // Nested conditional expression
        let conditional = if result > 0 {
            sin(result) * cos(result)
        } elif result < 0 {
            exp(-result) * sin(-result)
        } else {
            1.0
        };
        
        result * conditional
    )");

    REQUIRE(ast != nullptr);

    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Should handle complex expression trees
    REQUIRE(dumped.find("sin") != std::string::npos);
    REQUIRE(dumped.find("cos") != std::string::npos);
    REQUIRE(dumped.find("exp") != std::string::npos);
    REQUIRE(dumped.find("pow") != std::string::npos); // Power operator
}

TEST_CASE("SSAMapper: branch with mutables", "[ssamapper][controlflow]")
{

    Environment env;
    auto ast = env.parse(R"(
        [[extern, pure]] fn getInput() -> num;

        let mut x:num = 0;
        let branch_phi = if getInput() < 0.5 {
            x = 12;
            let x = 10;
            x
        } else {
            x = 42;
            20
        };
        
        branch_phi + x
    )");

    REQUIRE(ast != nullptr);

    auto prog   = env.map(ast);
    auto dumped = SSASerializer::serialize(prog);

    // Should have two phi nodes
    size_t pos      = 0;
    size_t phiCount = 0;
    while ((pos = dumped.find("phi[", pos)) != std::string::npos) {
        phiCount++;
        pos += 4;
    }

    REQUIRE(phiCount == 2);
}
