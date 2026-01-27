#include <catch2/catch_test_macros.hpp>

#include "Environment.h"
#include "utils/StringVisitor.h"

using namespace PExpr;

TEST_CASE("UpliftPass: simple capture and call update", "[uplift]")
{
    Environment env;
    auto ast = env.parse("let x = 1; fn f() = { x + 1 }; f()");
    REQUIRE(ast != nullptr);

    const std::string out = utils::StringVisitor::visit(ast);

    // The captured variable 'x' should be uplifted into function parameter
    REQUIRE(out.find("fn f(x:int") != std::string::npos);
    // Calls to f() should have been updated to pass the captured variable
    REQUIRE(out.find("f(x)") != std::string::npos);
}

TEST_CASE("UpliftPass: nested function capture and call update", "[uplift]")
{
    Environment env;
    // inner captures x from outer scope; inner's declaration should gain a parameter and the call updated
    auto ast = env.parse("let x = 2; fn outer() = { fn inner() = { x + 1 }; inner() }; outer()");
    REQUIRE(ast != nullptr);

    const std::string out = utils::StringVisitor::visit(ast);

    REQUIRE(out.find("fn inner(x:int") != std::string::npos);
    REQUIRE(out.find("inner(x)") != std::string::npos);
}
