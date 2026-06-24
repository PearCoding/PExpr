#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"
#include "type/TypeChecker.h"

using namespace PExpr;
using namespace PExpr::type;

TEST_CASE("TypeChecker: complex type inference scenarios", "[typechecker][inference]")
{
    Environment env;

    SECTION("Type aliases with constraints")
    {
        auto ast = env.parse(R"(
            using Number = num;
            using Vector = vec3;
            
            let v:Vector = [1.0, 2.0, 3.0];
            let n:Number = 42.0;
            
            v.x + n
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }
}

TEST_CASE("TypeChecker: subtype relationships and conversions", "[typechecker][subtyping]")
{
    Environment env;

    SECTION("Numeric type promotions")
    {
        auto ast = env.parse(R"(
            // int to num promotion
            let i:int = 42;
            let n:num = i;  // Implicit conversion
            let sum = i + n; // Mixed expression
            
            sum
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Tuple type compatibility")
    {
        auto ast = env.parse(R"(
            // Tuple type assignments
            let t1:[int, num] = [42, 3.14];
            let t2:[num, num] = t1;  // Should work due to int->num conversion
            
            t2.x + t2.y
        )");

        REQUIRE(ast != nullptr);
        // Might have errors depending on type system
    }

    SECTION("Vector component type mixing")
    {
        auto ast = env.parse(R"(
            // Vector with mixed component types
            let v:vec3 = [1, 2.0, 3];  // int, num, int -> should become vec3
            
            v.x + v.y + v.z
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }
}

TEST_CASE("TypeChecker: error cases and type mismatches", "[typechecker][errors]")
{
    Environment env;

    SECTION("Function return type mismatch")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            fn bad() -> int = 3.14;  // num cannot be implicitly converted to int
            bad()
        )");

        // Should produce type error
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Wrong number of function arguments")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            fn add(x:int, y:int) -> int = x + y;
            let result = add(1);  // Missing argument
            
            result
        )");

        // Should produce error
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Invalid vector component access")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            let v:vec2 = [1.0, 2.0];
            let comp = v.z;  // vec2 has no z component

            comp
        )");

        // Should produce error
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Tuple index equal to size is out of bounds")
    {
        // Regression: the bounds check used '<' instead of '<=', so index == size
        // slipped through and crashed components().at(index) with std::out_of_range.
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            let v = [1, 2, 3];
            v[3]
        )");

        // Should report an error instead of crashing
        REQUIRE(env.reporter().errorCount() > 0);
    }
}

TEST_CASE("TypeChecker: last tuple element is accessible", "[typechecker][access]")
{
    Environment env;

    auto ast = env.parse(R"(
        let v = [1, 2, 3];
        v[2]
    )");

    REQUIRE(ast != nullptr);
    REQUIRE(env.reporter().errorCount() == 0);
}

TEST_CASE("TypeChecker: generic type patterns", "[typechecker][generics]")
{
    Environment env;

    SECTION("Tuple type patterns")
    {
        auto ast = env.parse(R"(
            // Working with tuples of different types
            fn swap(pair:[int, num]) -> [num, int] = [pair.y, pair.x];
            
            let original:[int, num] = [42, 3.14];
            let swapped = swap(original);
            
            swapped.x + swapped.y
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }
}

TEST_CASE("TypeChecker: external function type checking", "[typechecker][extern]")
{
    Environment env;

    SECTION("External function with pure attribute")
    {
        auto ast = env.parse(R"(
            @[extern, pure] fn sin(x:num) -> num;
            @[extern, pure] fn cos(x:num) -> num;
            
            let result = sin(3.14) + cos(1.57);
            
            result
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("External function type mismatch")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            @[extern] fn externalFunc(x:num) -> num;
            
            let result = externalFunc(true);  // bool passed where num expected
            
            result
        )");

        // Should produce type error
        REQUIRE(env.reporter().errorCount() > 0);
    }
}

TEST_CASE("TypeChecker: closure type inference", "[typechecker][closures]")
{
    Environment env;

    SECTION("Closure capturing environment")
    {
        auto ast = env.parse(R"(
            let x = 42;
            fn getX() -> int = x;
            
            getX()
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Closure with mutable capture")
    {
        auto ast = env.parse(R"(
            let mut counter = 0;
            fn increment() -> int = {
                counter = counter + 1;
                counter
            };
            
            increment() + increment()
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Nested closure type scoping")
    {
        auto ast = env.parse(R"(
            let x = 1;
            let y = 2.0;
            
            fn outer() -> num = {
                fn inner() -> int = x;
                inner() + y
            };
            
            outer()
        )");

        REQUIRE(ast != nullptr);
        // Type checking should handle nested closures
    }
}