#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "type/TypeChecker.h"

using namespace PExpr;
using namespace PExpr::type;

TEST_CASE("TypeChecker: integer arithmetic", "[typechecker]")
{
    Environment env;
    auto ast = env.parse("1+2");
    auto t   = ast->expression()->returnType();

    REQUIRE(t.kind() == TypeKind::Integer);
}

TEST_CASE("TypeChecker: number arithmetic", "[typechecker]")
{
    Environment env;
    auto ast = env.parse("1.5+2.25");
    auto t   = ast->expression()->returnType();

    REQUIRE(t.kind() == TypeKind::Number);
}

TEST_CASE("TypeChecker: mixed int and number yields number", "[typechecker]")
{
    Environment env;
    auto ast = env.parse("1+2.0");
    auto t   = ast->expression()->returnType();

    REQUIRE(t.kind() == TypeKind::Number);
}

TEST_CASE("TypeChecker: variable declaration registers variable and used in expression", "[typechecker]")
{
    Environment env;
    auto ast = env.parse("let mut x = 1; x = x+4; x+2");
    auto t   = ast->expression()->returnType();

    REQUIRE(t.kind() == TypeKind::Integer);
}

TEST_CASE("TypeChecker: string literal", "[typechecker]")
{
    Environment env;
    auto ast = env.parse("\"hello\"");
    auto t   = ast->expression()->returnType();

    REQUIRE(t.kind() == TypeKind::String);
}

TEST_CASE("TypeChecker: destructuring declarations", "[typechecker]")
{
    SECTION("Basic destructuring")
    {
        Environment env;
        auto ast = env.parse("let *[a, b] = [1, 2]; a + b");
        auto t   = ast->expression()->returnType();

        REQUIRE(t.kind() == TypeKind::Integer);
    }

    SECTION("Destructuring with type annotations")
    {
        Environment env;
        auto ast = env.parse("let *[a:vec2, b:num] = [[1,2], 3.0]; a.x + b");
        auto t   = ast->expression()->returnType();

        REQUIRE(t.kind() == TypeKind::Number);
    }

    SECTION("Destructuring with mutable variables")
    {
        Environment env;
        auto ast = env.parse("let *[mut a, b] = [1, 2]; a = 3; a + b");
        auto t   = ast->expression()->returnType();

        REQUIRE(t.kind() == TypeKind::Integer);
    }

    SECTION("Size mismatch should fail")
    {
        Environment env;
        env.reporter().setQuiet(true);
        auto ast = env.parse("let *[a, b, c] = [1, 2]; a");
        // This should produce an error
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Infered type in destructuring")
    {
        Environment env;
        auto ast = env.parse("let *[a:num, b] = [1, true]; b");
        auto t   = ast->expression()->returnType();

        REQUIRE(t.kind() == TypeKind::Boolean);
    }
}

TEST_CASE("TypeChecker: destructuring assignments", "[typechecker]")
{
    SECTION("Basic destructuring assignment")
    {
        Environment env;
        auto ast = env.parse("let mut x = 1; let mut y = 2; *[x, y] = [3, 4]; x + y");
        REQUIRE(ast != nullptr);

        auto t = ast->expression()->returnType();

        REQUIRE(t.kind() == TypeKind::Integer);
        REQUIRE(env.reporter().errorCount() == 0);
    }

    SECTION("Destructuring assignment with immutable variable should fail")
    {
        Environment env;
        env.reporter().setQuiet(true);
        auto ast = env.parse("let x = 1; let mut y = 2; *[x, y] = [3, 4]; x");
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Destructuring assignment with non-tuple RHS should fail")
    {
        Environment env;
        env.reporter().setQuiet(true);
        auto ast = env.parse("let mut x = 1; let mut y = 2; *[x, y] = 3; x");
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Destructuring assignment size mismatch should fail")
    {
        Environment env;
        env.reporter().setQuiet(true);
        auto ast = env.parse("let mut x = 1; let mut y = 2; *[x, y] = [3, 4, 5]; x");
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Destructuring assignment type mismatch should fail")
    {
        Environment env;
        env.reporter().setQuiet(true);
        auto ast = env.parse("let mut x:num = 1.0; let mut y = 2; *[x, y] = [true, 4]; x");
        REQUIRE(env.reporter().errorCount() > 0);
    }

    SECTION("Destructuring assignment swap values")
    {
        Environment env;
        auto ast = env.parse("let mut a = 1; let mut b = 2; *[b, a] = [a, b]; a + b");
        REQUIRE(ast != nullptr);

        auto t = ast->expression()->returnType();
        REQUIRE(t.kind() == TypeKind::Integer);
        REQUIRE(env.reporter().errorCount() == 0);
    }
}
