#include <catch2/catch_test_macros.hpp>

#include "PExpr.h"
#include "StringVisitor.h"

using namespace PExpr;

TEST_CASE("UpliftPass: simple capture and call update", "[uplift]")
{
    Environment env;
    const std::string input = "let x = 1; fn f() = { x + 1 }; f()";

    auto ast = env.parse(input);
    REQUIRE(ast != nullptr);

    const std::string out = StringVisitor::visit(ast);

    // The captured variable 'x' should be uplifted into function parameter
    REQUIRE(out.find("fn f(x:int") != std::string::npos);
    // Calls to f() should have been updated to pass the captured variable
    REQUIRE(out.find("f(x)") != std::string::npos);
}

TEST_CASE("UpliftPass: nested function capture and call update", "[uplift]")
{
    Environment env;
    // inner captures x from outer scope; inner's declaration should gain a parameter and the call updated
    const std::string input = "let x = 2; fn outer() = { fn inner() = { x + 1 }; inner() }; outer()";

    auto ast = env.parse(input);
    REQUIRE(ast != nullptr);

    const std::string out = StringVisitor::visit(ast);

    REQUIRE(out.find("fn inner(x:int") != std::string::npos);
    REQUIRE(out.find("inner(x)") != std::string::npos);
}
