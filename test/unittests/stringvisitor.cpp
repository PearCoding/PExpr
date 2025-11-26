#include <catch2/catch_test_macros.hpp>

#include "PExpr.h"

using namespace PExpr;

TEST_CASE("String visitor should produce a correct parsable version", "[stringvisitor]")
{
    Environment env;
    auto ast1 = env.parse("abc(231*22.231*2.42e-3).xyz", true);
    REQUIRE(ast1 != nullptr);

    const std::string src1 = StringVisitor::visit(ast1);

    auto ast2 = env.parse(src1, true);
    REQUIRE(ast2 != nullptr);

    const std::string src2 = StringVisitor::visit(ast2);
    REQUIRE(src1 == src2);
}