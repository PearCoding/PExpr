#include <catch2/catch_test_macros.hpp>
#include <string>

#include "rvm/RVMValue.h"
#include "type/Type.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;

TEST_CASE("RVMValue: basic creation and properties", "[rvm][value]")
{
    SECTION("Constant values")
    {
        RVMValue constInt = RVMValue::Constant(Integer(42));
        REQUIRE(constInt.isConstant());
        REQUIRE(constInt.type() == Type(TypeKind::Integer));

        RVMValue constFloat = RVMValue::Constant(Number(3.14));
        REQUIRE(constFloat.isConstant());
        REQUIRE(constFloat.type() == Type(TypeKind::Number));

        RVMValue constBool = RVMValue::Constant(true);
        REQUIRE(constBool.isConstant());
        REQUIRE(constBool.type() == Type(TypeKind::Boolean));
    }

    SECTION("Register values")
    {
        RVMValue regValue = RVMValue::Register(5, Type(TypeKind::Integer));
        REQUIRE(regValue.isRegister());
        REQUIRE(regValue.type() == Type(TypeKind::Integer));
        REQUIRE(regValue.regId() == 5u);
    }

    SECTION("String reference values")
    {
        RVMValue strValue = RVMValue::StringRef(1);
        REQUIRE(strValue.isStringRef());
        REQUIRE(strValue.type() == Type(TypeKind::String));
        REQUIRE(strValue.stringId() == 1);
    }
}

TEST_CASE("RVMValue: equality and hashing", "[rvm][value]")
{
    SECTION("Constant equality")
    {
        RVMValue constInt1 = RVMValue::Constant(Integer(42));
        RVMValue constInt2 = RVMValue::Constant(Integer(42));
        RVMValue constInt3 = RVMValue::Constant(Integer(100));

        REQUIRE(constInt1 == constInt2);
        REQUIRE(!(constInt1 == constInt3));
    }

    SECTION("Register equality")
    {
        RVMValue reg1 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue reg2 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue reg3 = RVMValue::Register(2, Type(TypeKind::Integer));

        REQUIRE(reg1 == reg2);
        REQUIRE(!(reg1 == reg3));
    }

    SECTION("Hash consistency")
    {
        RVMValue constInt1 = RVMValue::Constant(Integer(42));
        RVMValue constInt2 = RVMValue::Constant(Integer(42));
        RVMValue constInt3 = RVMValue::Constant(Integer(100));

        REQUIRE(constInt1.hash() == constInt2.hash());
        REQUIRE(constInt1.hash() != constInt3.hash());
    }
}