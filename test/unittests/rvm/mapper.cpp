#include <catch2/catch_test_macros.hpp>
#include <vector>

#include "rvm/RVMMapper.h"
#include "type/Type.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;

TEST_CASE("RVMMapper: tuple type dissolution", "[rvm][mapper]")
{
    SECTION("Simple tuple dissolution")
    {
        std::vector<type::Type> components = {
            Type(TypeKind::Integer),
            Type(TypeKind::Number),
            Type(TypeKind::Boolean)
        };
        Type tupleType(components);

        auto dissolved = RVMMapper::dissolveTupleType(tupleType);

        REQUIRE(dissolved.size() == 3);
        REQUIRE(dissolved[0].kind() == TypeKind::Integer);
        REQUIRE(dissolved[1].kind() == TypeKind::Number);
        REQUIRE(dissolved[2].kind() == TypeKind::Boolean);
    }

    SECTION("Nested tuple dissolution")
    {
        std::vector<type::Type> inner = {
            Type(TypeKind::Number),
            Type(TypeKind::Number)
        };
        Type innerTuple(inner);

        std::vector<type::Type> outer = {
            Type(TypeKind::Integer),
            innerTuple,
            Type(TypeKind::Boolean)
        };
        Type outerTuple(outer);

        auto dissolved = RVMMapper::dissolveTupleType(outerTuple);

        REQUIRE(dissolved.size() == 4);
        REQUIRE(dissolved[0].kind() == TypeKind::Integer);
        REQUIRE(dissolved[1].kind() == TypeKind::Number);
        REQUIRE(dissolved[2].kind() == TypeKind::Number);
        REQUIRE(dissolved[3].kind() == TypeKind::Boolean);
    }

    SECTION("Elementary type dissolution")
    {
        Type intType(TypeKind::Integer);
        auto dissolved = RVMMapper::dissolveTupleType(intType);

        REQUIRE(dissolved.size() == 1);
        REQUIRE(dissolved[0].kind() == TypeKind::Integer);
    }
}

