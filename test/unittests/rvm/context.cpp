#include <catch2/catch_test_macros.hpp>

#include "rvm/RVMContext.h"

using namespace PExpr;
using namespace PExpr::rvm;

TEST_CASE("RVMContext: register management", "[rvm][context]")
{
    RVMContext context;

    SECTION("Register allocation")
    {
        RegId reg1 = context.allocateRegister();
        RegId reg2 = context.allocateRegister();
        RegId reg3 = context.allocateRegister();

        REQUIRE(reg1 == 0u);
        REQUIRE(reg2 == 1u);
        REQUIRE(reg3 == 2u);
    }
}