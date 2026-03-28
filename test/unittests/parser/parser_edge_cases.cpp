#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "parser/Parser.h"
#include "utils/StringVisitor.h"

using namespace PExpr;
using namespace PExpr::parser;

TEST_CASE("Parser: complex expression parsing", "[parser][expressions]")
{
    Environment env;

    SECTION("Nested operator precedence")
    {
        auto ast = env.parse(R"(
            // Test operator precedence
            let result = 1 + 2 * 3 - 4 / 2 + 5 * (6 - 3);
            
            result
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Complex vector expressions")
    {
        auto ast = env.parse(R"(
            // Vector operations with mixed expressions
            let v = [1.0, 2.0] + [3.0, 4.0] * 2.0 - [1.0, 1.0];
            let swizzle = v.yx;
            
            swizzle.x + swizzle.y
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Function calls in expressions")
    {
        auto ast = env.parse(R"(
            @[extern, pure] fn abs(x:num) -> num;
            
            let result = abs(3.14) * 2.0 + abs(-2.5);
            
            result
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }
}

TEST_CASE("Parser: statement sequencing and blocks", "[parser][statements]")
{
    Environment env;

    SECTION("Multiple statements in block")
    {
        auto ast = env.parse(R"(
            {
                let a = 1;
                let b = 2;
                let c = a + b;
                c * 2
            }
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Nested blocks with variables")
    {
        auto ast = env.parse(R"(
            let outer = 1;
            {
                let inner = 2;
                {
                    let innermost = 3;
                    outer + inner + innermost
                }
            }
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Empty block")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            {
                // Empty block should still parse, but is ill-formed
            }
        )");

        // Will fail
        // Just check it doesn't crash
    }
}

TEST_CASE("Parser: conditional expressions", "[parser][conditionals]")
{
    Environment env;

    SECTION("Nested conditionals")
    {
        auto ast = env.parse(R"(
            let x = if true {
                if false { 1 } else { 2 }
            } else {
                3
            };
            
            x
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Elif chain")
    {
        auto ast = env.parse(R"(
            let grade = 85;
            let letter = if grade >= 90 {
                "A"
            } elif grade >= 80 {
                "B"
            } elif grade >= 70 {
                "C"
            } elif grade >= 60 {
                "D"
            } else {
                "F"
            };
            
            letter
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Conditional without else")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            let x = 5;
            if x > 0 {
                x * 2
            };
            
            x
        )");

        // Correct in the newest version. Will produce warnings though
        REQUIRE(env.reporter().errorCount() == 0);
    }
}

TEST_CASE("Parser: error recovery and syntax errors", "[parser][errors]")
{
    Environment env;

    SECTION("Missing semicolon")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            let x = 1
            let y = 2
            x + y
        )");

        // Should produce parser error
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Mismatched parentheses")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            let x = (1 + 2 * 3;
            x
        )");

        // Should produce parser error
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Invalid token in expression")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            let x = 1 @ 2;  // @ is not a valid operator
            x
        )");

        // Should produce parser error
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Unclosed block")
    {
        env.reporter().setQuiet(true);
        auto ast = env.parse(R"(
            {
                let x = 1;
                let y = 2;
                x + y
            // Missing closing brace
        )");

        // Should produce parser error
        REQUIRE(env.reporter().errorCount() > 0);
    }
}

TEST_CASE("Parser: type annotations and declarations", "[parser][types]")
{
    Environment env;

    SECTION("Complex type annotations")
    {
        auto ast = env.parse(R"(
            let v:vec3 = [1.0, 2.0, 3.0];
            let t:[int, num, bool] = [42, 3.14, true];
            
            v.x + t.x
        )");

        // TODO
        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Function type signatures")
    {
        auto ast = env.parse(R"(
            fn add(x:int, y:int) -> int = x + y;
            fn process(v:vec3) -> num = v.x + v.y + v.z;
            
            add(1, 2) + process([1.0, 2.0, 3.0])
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Type alias declarations")
    {
        auto ast = env.parse(R"(
            using Point = vec3;
            using Color = [num, num, num];
            
            let p:Point = [1.0, 2.0, 3.0];
            let c:Color = [0.5, 0.5, 0.5];
            
            p.x + c.x
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }
}

TEST_CASE("Parser: edge cases and boundary conditions", "[parser][edge]")
{
    Environment env;

    SECTION("Very large integer literal")
    {
        auto ast = env.parse(R"(
            let big = 12345678901234567890;
            
            big
        )");

        REQUIRE(ast != nullptr);
        // Parser should handle large numbers
    }

    SECTION("Scientific notation numbers")
    {
        auto ast = env.parse(R"(
            let sci1 = 1.23e4;
            let sci2 = 5.67e-3;
            let sci3 = 9.0e+10;
            
            sci1 + sci2 + sci3
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Whitespace variations")
    {
        auto ast = env.parse(R"(
            let   x   =   1   +   2   ;
            let y=3*4;
            let z = ( 5 + 6 ) * 7 ;
            
            x + y + z
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Comments in code")
    {
        auto ast = env.parse(R"(
            // This is a comment
            let x = 1;  // Inline comment
            /*
               Multi-line comment
               spanning several lines
            */
            let y = 2;
            
            x + y  // Result
        )");

        REQUIRE(ast != nullptr);
        REQUIRE(env.reporter().errorCount() == 0);
    }
}