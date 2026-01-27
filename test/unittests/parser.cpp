#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "parser/Lexer.h"
#include "parser/Parser.h"
#include "type/Type.h"
#include "utils/StringVisitor.h"

using namespace PExpr;
using namespace PExpr::parser;
using namespace PExpr::type;
using namespace PExpr::utils;

inline static auto parseOnly(std::string_view str, bool shouldPass = true)
{
    utils::Reporter reporter;
    reporter.setQuiet(true);
    auto stream = std::istringstream(str.data());
    Lexer lexer(stream, reporter);
    Parser parser(lexer, reporter);
    SymbolTable globals;
    globals.addDefaultTypeAliases();
    auto ast = parser.parse(&globals);

    REQUIRE(parser.hasError() != shouldPass);
    return ast;
}

TEST_CASE("Parser: simple arithmetic", "[parser]")
{
    auto ast = parseOnly("a+1");
    REQUIRE(StringVisitor::visit(ast) == "(a)+(1)");
}

TEST_CASE("Parser: complex expression parsing", "[parser]")
{
    auto ast = parseOnly("abc(231*22.231*2.42e-3).xyz*Pi-123*(K.x+sin(22^4, 1-2%2, --1))");

    const std::string out = StringVisitor::visit(ast);
    REQUIRE(out.find("sin(") != std::string::npos);
    REQUIRE(out.find("Pi") != std::string::npos);
    REQUIRE(out.find("K") != std::string::npos);
}

TEST_CASE("Parser: closure with variable and expression", "[parser]")
{
    auto ast = parseOnly("let mut x = 1; x+2");

    const std::string out = StringVisitor::visit(ast);
    REQUIRE(out.find("mut x:int = 1;") != std::string::npos);
    REQUIRE(out.find("(x)+(2)") != std::string::npos);
}

TEST_CASE("Parser: function declaration and call", "[parser]")
{
    auto ast = parseOnly("fn f(a:int) = a; f(1)");

    const std::string out = StringVisitor::visit(ast);
    REQUIRE(out.find("fn f(") != std::string::npos);
    REQUIRE(out.find("f(") != std::string::npos);
}

TEST_CASE("Parser: branch expression (if/else)", "[parser]")
{
    auto ast = parseOnly("if true { 1 } else { 2 }");

    const std::string out = StringVisitor::visit(ast);
    REQUIRE(out.find("if ") != std::string::npos);
    REQUIRE(out.find("else") != std::string::npos);
}

TEST_CASE("Parser: attributes before fn keyword", "[parser]")
{
    SECTION("Simple extern attribute")
    {
        auto ast              = parseOnly("[[extern]] fn foo(v:int) -> int; foo(5)");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("extern") != std::string::npos);
        REQUIRE(out.find("foo(") != std::string::npos);
    }

    SECTION("Multiple attributes")
    {
        auto ast              = parseOnly("[[extern, pure]] fn foo(v:int) -> int; foo(5)");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("extern") != std::string::npos);
    }

    SECTION("Attributes with boolean values")
    {
        auto ast              = parseOnly("[[extern=true, pure=false]] fn foo(v:int) -> int; foo(5)");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("extern") != std::string::npos);
    }

    SECTION("Attributes with integer values")
    {
        // Note: integer attribute values are parsed but not used by FunctionAttributes
        auto ast              = parseOnly("[[priority=2, extern=true]] fn foo(v:int) -> int; foo(5)");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("extern") != std::string::npos);
    }

    SECTION("Attributes with string values")
    {
        // Note: string attribute values are parsed but not used by FunctionAttributes
        auto ast              = parseOnly("[[name=\"test\", extern=true]] fn foo(v:int) -> int; foo(5)");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("extern") != std::string::npos);
    }
}

TEST_CASE("Parser: type alias declarations", "[parser]")
{
    SECTION("Simple type alias")
    {
        auto ast              = parseOnly("using MyInt = int; let x: MyInt = 42; x");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("using MyInt = int;") != std::string::npos);
        REQUIRE(out.find("x:int = 42;") != std::string::npos);
    }

    SECTION("Tuple type alias")
    {
        auto ast              = parseOnly("using Pair = [num, int]; let p: Pair = [3.14, 5]; p");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("using Pair = [num, int];") != std::string::npos);
    }

    SECTION("Vector type alias")
    {
        auto ast              = parseOnly("using V2 = vec2; let v: V2 = [1.0, 2.0]; v");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("using V2 = vec2;") != std::string::npos);
    }

    SECTION("Type alias shadowing")
    {
        parseOnly("using T = int; using T = num;", false);
    }
}

TEST_CASE("Parser: attributes on variable declarations", "[parser]")
{
    SECTION("Variable with attribute")
    {
        // Note: const attribute does not exist
        auto ast              = parseOnly("[[const]] let x = 5; x");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("x") != std::string::npos);
    }

    SECTION("Mutable variable with attribute")
    {
        // Note: optimize attribute does not exist
        auto ast              = parseOnly("[[optimize]] let mut y = 10; y");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("mut y:int = 10;") != std::string::npos);
    }
}

TEST_CASE("Parser: attribute parsing edge cases", "[parser]")
{
    SECTION("Empty attribute list")
    {
        auto ast              = parseOnly("[[]] fn foo(v:int) -> int = v; foo(5)");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("foo(") != std::string::npos);
    }

    SECTION("Boolean attribute without value (defaults to true)")
    {
        auto ast              = parseOnly("[[extern, pure]] fn foo(v:int) -> int; foo(5)");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("extern") != std::string::npos);
    }
}

TEST_CASE("Parser: destructuring declarations and assignments", "[parser]")
{
    SECTION("Simple destructuring declaration")
    {
        auto ast              = parseOnly("let *[a, b] = [1, 2]; a + b");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("let *[a, b] = [1, 2];") != std::string::npos);
    }

    SECTION("Destructuring declaration with type annotations")
    {
        auto ast              = parseOnly("let *[a:vec2, mut b:num] = [[1,2], 3.0]; a");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("let *[a:vec2, mut b:num] = [") != std::string::npos);
    }

    SECTION("Destructuring assignment")
    {
        auto ast              = parseOnly("let mut x = 1; let mut y = 2; *[x, y] = [3, 4]; x + y");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("*[x, y] = [3, 4];") != std::string::npos);
    }

    SECTION("Destructuring assignment should not allow mut or type annotations")
    {
        parseOnly("*[mut x, y:num] = [1, 2]; x", false); // mut not allowed in assignment
        parseOnly("*[x:int, y] = [1, 2]; x", false);     // type annotation not allowed in assignment
    }

    SECTION("Destructuring assignment without * prefix should fail")
    {
        parseOnly("[x, y] = [1, 2]; x", false); // Missing * prefix
    }

    SECTION("Nested destructuring")
    {
        auto ast              = parseOnly("let *[[a, b], c] = [[1, 2], 3]; a + b + c");
        const std::string out = StringVisitor::visit(ast);
        REQUIRE(out.find("let *[[a, b], c] = [") != std::string::npos);
    }
}
