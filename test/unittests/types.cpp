#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "Environment.h"
#include "ast/Closure.h"
#include "ast/Expression.h"
#include "ssa/SSAValue.h"
#include "type/Type.h"

using namespace PExpr;
using namespace PExpr::type;

TEST_CASE("Type: basic construction and equality", "[type]")
{
    SECTION("Elementary types")
    {
        Type boolType(TypeKind::Boolean);
        Type intType(TypeKind::Integer);
        Type numType(TypeKind::Number);
        Type strType(TypeKind::String);

        REQUIRE(boolType.kind() == TypeKind::Boolean);
        REQUIRE(intType.kind() == TypeKind::Integer);
        REQUIRE(numType.kind() == TypeKind::Number);
        REQUIRE(strType.kind() == TypeKind::String);

        REQUIRE(boolType.isElementary());
        REQUIRE(!boolType.isTuple());
        REQUIRE(boolType.size() == 1);
    }

    SECTION("Tuple construction")
    {
        Type tuple({ Type(TypeKind::Boolean), Type(TypeKind::Integer) });
        REQUIRE(tuple.kind() == TypeKind::Tuple);
        REQUIRE(tuple.isTuple());
        REQUIRE(!tuple.isElementary());
        REQUIRE(tuple.size() == 2);
        REQUIRE(tuple.components().size() == 2);
        REQUIRE(tuple.components()[0].kind() == TypeKind::Boolean);
        REQUIRE(tuple.components()[1].kind() == TypeKind::Integer);
    }

    SECTION("Vector construction")
    {
        Type vec3 = Type::AsVector(3);
        REQUIRE(vec3.isTuple());
        REQUIRE(vec3.isVector());
        REQUIRE(vec3.size() == 3);
        for (size_t i = 0; i < 3; ++i)
            REQUIRE(vec3.components()[i].kind() == TypeKind::Number);

        Type vec4 = Type::AsVector(4);
        REQUIRE(vec4.isVector());
        REQUIRE(vec4.size() == 4);
    }

    SECTION("Equality comparison")
    {
        Type bool1(TypeKind::Boolean);
        Type bool2(TypeKind::Boolean);
        Type int1(TypeKind::Integer);

        REQUIRE(bool1 == bool2);
        REQUIRE(bool1 != int1);

        Type tuple1({ Type(TypeKind::Boolean), Type(TypeKind::Integer) });
        Type tuple2({ Type(TypeKind::Boolean), Type(TypeKind::Integer) });
        Type tuple3({ Type(TypeKind::Integer), Type(TypeKind::Boolean) });

        REQUIRE(tuple1 == tuple2);
        REQUIRE(tuple1 != tuple3);
        REQUIRE(tuple1 != bool1);
    }

    SECTION("Hash function")
    {
        Type boolType(TypeKind::Boolean);
        Type intType(TypeKind::Integer);
        Type tuple1({ Type(TypeKind::Boolean), Type(TypeKind::Integer) });
        Type tuple2({ Type(TypeKind::Boolean), Type(TypeKind::Integer) });
        Type tuple3({ Type(TypeKind::Integer), Type(TypeKind::Boolean) });

        REQUIRE(boolType.hash() != intType.hash());
        REQUIRE(tuple1.hash() == tuple2.hash());
        REQUIRE(tuple1.hash() != tuple3.hash());
    }
}

TEST_CASE("Type: isConvertible and isExplicitConvertible", "[type]")
{
    SECTION("Elementary conversions")
    {
        Type intType(TypeKind::Integer);
        Type numType(TypeKind::Number);
        Type boolType(TypeKind::Boolean);

        REQUIRE(isConvertible(intType, numType));         // int -> num implicit
        REQUIRE(!isConvertible(numType, intType));        // num -> int not implicit
        REQUIRE(isExplicitConvertible(numType, intType)); // num -> int explicit

        REQUIRE(isConvertible(boolType, boolType)); // same type
        REQUIRE(!isConvertible(boolType, intType)); // bool -> int not allowed
        REQUIRE(!isExplicitConvertible(boolType, intType));
    }

    SECTION("Tuple conversions")
    {
        Type tupleIntNum({ Type(TypeKind::Integer), Type(TypeKind::Number) });
        Type tupleNumNum({ Type(TypeKind::Number), Type(TypeKind::Number) });
        Type tupleNumInt({ Type(TypeKind::Number), Type(TypeKind::Integer) });

        // Component-wise implicit conversion
        REQUIRE(isConvertible(tupleIntNum, tupleNumNum)); // [int, num] -> [num, num]

        // Not convertible (wrong direction)
        REQUIRE(!isConvertible(tupleNumNum, tupleIntNum));

        // Explicit conversion allowed
        REQUIRE(isExplicitConvertible(tupleNumNum, tupleIntNum));
    }

    SECTION("Vector conversions")
    {
        Type vec2 = Type::AsVector(2);
        Type vec3 = Type::AsVector(3);
        Type tuple2({ Type(TypeKind::Number), Type(TypeKind::Number) });

        REQUIRE(vec2.isVector());
        REQUIRE(vec3.isVector());
        REQUIRE(isConvertible(vec2, vec2));   // same vector size
        REQUIRE(!isConvertible(vec2, vec3));  // different size
        REQUIRE(isConvertible(vec2, tuple2)); // vec2 is convertible to [num, num]
        REQUIRE(isConvertible(tuple2, vec2)); // [num, num] is convertible to vec2
    }
}

TEST_CASE("Type: string representation", "[type]")
{
    SECTION("Elementary types")
    {
        REQUIRE(Type(TypeKind::Boolean).toString() == "bool");
        REQUIRE(Type(TypeKind::Integer).toString() == "int");
        REQUIRE(Type(TypeKind::Number).toString() == "num");
        REQUIRE(Type(TypeKind::String).toString() == "str");
    }

    SECTION("Tuple types")
    {
        Type tuple1({ Type(TypeKind::Boolean), Type(TypeKind::Integer) });
        REQUIRE(tuple1.toString() == "[bool, int]");

        Type tuple2({ Type(TypeKind::Number), Type(TypeKind::String), Type(TypeKind::Boolean) });
        REQUIRE(tuple2.toString() == "[num, str, bool]");

        Type nested({ Type(TypeKind::Boolean), Type({ Type(TypeKind::Integer), Type(TypeKind::Number) }) });
        REQUIRE(nested.toString() == "[bool, [int, num]]");
    }

    SECTION("Vector types")
    {
        REQUIRE(Type::AsVector(2).toString() == "vec2");
        REQUIRE(Type::AsVector(3).toString() == "vec3");
        REQUIRE(Type::AsVector(4).toString() == "vec4");
    }
}

TEST_CASE("SSAValue: tuple constants and hashing", "[ssavalue]")
{
    using namespace PExpr::ssa;

    SECTION("Tuple constant creation")
    {
        std::vector<Number> vec{ 1.0, 2.0, 3.0 };
        SSAValue vecConst = SSAValue::Constant(vec);

        REQUIRE(vecConst.isConstant());
        REQUIRE(vecConst.type().isVector());
        REQUIRE(vecConst.type().size() == 3);

        const auto& tuple = std::get<Tuple>(vecConst.rawValue());
        REQUIRE(tuple->elements.size() == 3);
        REQUIRE(std::get<Number>(tuple->elements[0]) == 1.0);
        REQUIRE(std::get<Number>(tuple->elements[1]) == 2.0);
        REQUIRE(std::get<Number>(tuple->elements[2]) == 3.0);
    }

    SECTION("Tuple value equality")
    {
        std::vector<Number> vec1{ 1.0, 2.0, 3.0 };
        std::vector<Number> vec2{ 1.0, 2.0, 3.0 };
        std::vector<Number> vec3{ 4.0, 5.0, 6.0 };

        SSAValue val1 = SSAValue::Constant(vec1);
        SSAValue val2 = SSAValue::Constant(vec2);
        SSAValue val3 = SSAValue::Constant(vec3);

        REQUIRE(val1 == val2);
        REQUIRE(val1 != val3);
    }

    SECTION("Tuple value hashing")
    {
        std::vector<Number> vec1{ 1.0, 2.0, 3.0 };
        std::vector<Number> vec2{ 1.0, 2.0, 3.0 };
        std::vector<Number> vec3{ 4.0, 5.0, 6.0 };

        SSAValue val1 = SSAValue::Constant(vec1);
        SSAValue val2 = SSAValue::Constant(vec2);
        SSAValue val3 = SSAValue::Constant(vec3);

        REQUIRE(val1.hash() == val2.hash());
        REQUIRE(val1.hash() != val3.hash());

        // Test with nested tuples (through ValueVariant construction)
        Tuple innerTuple = Tuple(new TupleVariant());
        innerTuple->elements.push_back(1.0);
        innerTuple->elements.push_back(2.0);

        Tuple outerTuple = Tuple(new TupleVariant());
        outerTuple->elements.push_back(true);
        outerTuple->elements.push_back(innerTuple);

        Type outerType({ Type(TypeKind::Boolean), Type({ Type(TypeKind::Number), Type(TypeKind::Number) }) });
        SSAValue nestedVal(true, outerType, outerTuple);

        REQUIRE(nestedVal.isConstant());
        REQUIRE(nestedVal.type().isTuple());
        REQUIRE(nestedVal.type().components()[0].kind() == TypeKind::Boolean);
        REQUIRE(nestedVal.type().components()[1].isTuple());
    }
}

TEST_CASE("Type system integration: parsing and type checking", "[integration]")
{
    Environment env;

    SECTION("Simple tuple type annotation")
    {
        auto ast = env.parse("let t: [int, num] = [1, 2.5]; t");
        REQUIRE(ast != nullptr);
        auto t = ast->expression()->returnType();
        REQUIRE(t.isTuple());
        REQUIRE(t.components().size() == 2);
        REQUIRE(t.components()[0].kind() == TypeKind::Integer);
        REQUIRE(t.components()[1].kind() == TypeKind::Number);
    }

    SECTION("Vector type annotation")
    {
        auto ast = env.parse("let v: vec3 = [1.0, 2.0, 3.0]; v");
        REQUIRE(ast != nullptr);
        auto t = ast->expression()->returnType();
        REQUIRE(t.isVector());
        REQUIRE(t.size() == 3);
    }

    SECTION("Tuple access")
    {
        auto ast = env.parse("let t = [true, 42, 3.14]; t[1]");
        REQUIRE(ast != nullptr);
        auto t = ast->expression()->returnType();
        REQUIRE(t.kind() == TypeKind::Integer);
    }

    SECTION("Nested tuple")
    {
        auto ast = env.parse("let t: [bool, [int, num]] = [true, [42, 3.14]]; t[1][0]");
        REQUIRE(ast != nullptr);
        auto t = ast->expression()->returnType();
        REQUIRE(t.kind() == TypeKind::Integer);
    }

    SECTION("Function with tuple parameter")
    {
        auto ast = env.parse(R"(
            fn process(p: [bool, int]) -> num = {
                if p[0] {
                    p[1] as num
                } else {
                    0.0
                }
            };
            process([true, 5])
        )");
        REQUIRE(ast != nullptr);
        auto t = ast->expression()->returnType();
        REQUIRE(t.kind() == TypeKind::Number);
    }

    SECTION("Implicit tuple conversion")
    {
        auto ast = env.parse("let t: [num, num] = [1, 2]; t");
        REQUIRE(ast != nullptr);
        auto t = ast->expression()->returnType();
        REQUIRE(t.isTuple());
        REQUIRE(t.components()[0].kind() == TypeKind::Number);
        REQUIRE(t.components()[1].kind() == TypeKind::Number);
    }
}