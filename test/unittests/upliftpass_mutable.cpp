#include <catch2/catch_test_macros.hpp>

#include "Environment.h"
#include "utils/StringVisitor.h"

using namespace PExpr;

TEST_CASE("UpliftPass: mutable capture without assignment", "[uplift][mutable]")
{
    Environment env;
    // Mutable variable captured but not assigned - should be captured by value
    auto ast = env.parse("let mut x = 5; fn f() = { x + 1 }; f()");
    REQUIRE(ast != nullptr);

    const std::string out = utils::StringVisitor::visit(ast);

    // The captured variable 'x' should be uplifted into function parameter
    REQUIRE(out.find("fn f(x:int") != std::string::npos);
    // Calls to f() should have been updated to pass the captured variable
    REQUIRE(out.find("f(x)") != std::string::npos);
}

TEST_CASE("UpliftPass: mutable capture with assignment", "[uplift][mutable]")
{
    Environment env;
    // Mutable variable captured and assigned - should return tuple with updated value
    auto ast = env.parse("let mut x = 5; fn f() = { x = x + 1; x }; f()");
    REQUIRE(ast != nullptr);

    const std::string out = utils::StringVisitor::visit(ast);

    // The function should have x as parameter and return a tuple (int, int)
    REQUIRE(out.find("fn f(mut x:int") != std::string::npos);
    // Should return a tuple
    REQUIRE(out.find("[x, x]") != std::string::npos);
}

TEST_CASE("UpliftPass: multiple mutable captures", "[uplift][mutable]")
{
    Environment env;
    // Multiple mutable variables captured and assigned
    auto ast = env.parse("let mut a = 1; let mut b = 2; fn swap() = { let temp = a; a = b; b = temp; 0 }; swap()");
    REQUIRE(ast != nullptr);

    const std::string out = utils::StringVisitor::visit(ast);

    // The function should have a and b as parameters
    REQUIRE(out.find("fn swap(mut a:int, mut b:int") != std::string::npos);
    // Should return a tuple with three elements (0, a, b)
    REQUIRE(out.find("[0, a, b]") != std::string::npos);
}

TEST_CASE("UpliftPass: mixed immutable and mutable captures", "[uplift][mutable]")
{
    Environment env;
    // Both immutable and mutable captures
    auto ast = env.parse("let mut x = 5; let y = 10; fn mixed() = { x = x + y; x }; mixed()");
    REQUIRE(ast != nullptr);

    const std::string out = utils::StringVisitor::visit(ast);

    // The function should have both x and y as parameters
    REQUIRE(out.find("fn mixed(mut x:int, y:int") != std::string::npos);
    // Should return a tuple with two elements (x, x) since only x is mutable
    REQUIRE(out.find("[x, x]") != std::string::npos);
}

TEST_CASE("UpliftPass: nested closures with mutable captures", "[uplift][mutable]")
{
    Environment env;
    // Nested closures with mutable capture
    auto ast = env.parse("let mut x = 5; fn outer() = { fn inner() = { x = x + 1; x }; inner() }; outer()");
    REQUIRE(ast != nullptr);

    const std::string out = utils::StringVisitor::visit(ast);

    // Inner function should have x as parameter and return tuple
    REQUIRE(out.find("fn inner(mut x:int") != std::string::npos);
    // Outer function should also have x as parameter
    REQUIRE(out.find("fn outer(mut x:int") != std::string::npos);
}

TEST_CASE("UpliftPass: destructuring assignment with mutable capture", "[uplift][mutable]")
{
    Environment env;
    // Destructuring assignment with mutable capture
    auto ast = env.parse("let mut a = 1; let mut b = 2; fn update() = { *[a, b] = [b, a]; 0 }; update()");
    REQUIRE(ast != nullptr);

    const std::string out = utils::StringVisitor::visit(ast);

    // Function should have a and b as parameters
    REQUIRE(out.find("fn update(mut a:int, mut b:int") != std::string::npos);
    // Should return a tuple with three elements
    REQUIRE(out.find("[0, a, b]") != std::string::npos);
}