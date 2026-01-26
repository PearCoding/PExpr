#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "internal/TypeChecker.h"

using namespace PExpr;
using namespace PExpr::internal;

TEST_CASE("TypeChecker: integer arithmetic", "[typechecker]")
{
    std::stringstream stream("1+2");
    Environment env;
    auto ast = env.parse(stream);
    auto t   = ast->expression()->returnType();

    REQUIRE(t.kind() == TypeKind::Integer);
}

TEST_CASE("TypeChecker: number arithmetic", "[typechecker]")
{
    std::stringstream stream("1.5+2.25");
    Environment env;
    auto ast = env.parse(stream);
    auto t   = ast->expression()->returnType();

    REQUIRE(t.kind() == TypeKind::Number);
}

TEST_CASE("TypeChecker: mixed int and number yields number", "[typechecker]")
{
    std::stringstream stream("1+2.0");
    Environment env;
    auto ast = env.parse(stream);
    auto t   = ast->expression()->returnType();

    REQUIRE(t.kind() == TypeKind::Number);
}

TEST_CASE("TypeChecker: variable declaration registers variable and used in expression", "[typechecker]")
{
    std::stringstream stream("let mut x = 1; x = x+4; x+2");
    Environment env;
    auto ast = env.parse(stream);
    auto t   = ast->expression()->returnType();

    REQUIRE(t.kind() == TypeKind::Integer);
}

TEST_CASE("TypeChecker: string literal", "[typechecker]")
{
    std::stringstream stream("\"hello\"");
    Environment env;
    auto ast = env.parse(stream);
    auto t   = ast->expression()->returnType();

    REQUIRE(t.kind() == TypeKind::String);
}
