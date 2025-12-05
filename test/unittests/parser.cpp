#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "StringVisitor.h"
#include "internal/Lexer.h"
#include "internal/Parser.h"

using namespace PExpr;
using namespace PExpr::internal;

TEST_CASE("Parser: simple arithmetic", "[parser]")
{
    std::stringstream stream("a+1");
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);

    REQUIRE(!parser.hasError());
    REQUIRE(StringVisitor::visit(ast) == "(a)+(1)");
}

TEST_CASE("Parser: complex expression parsing", "[parser]")
{
    const std::string input = "abc(231*22.231*2.42e-3).xyz*Pi-123*(K.x+sin(22^4, 1-2%2, --1))";
    std::stringstream stream(input);
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);

    REQUIRE(!parser.hasError());
    const std::string out = StringVisitor::visit(ast);
    REQUIRE(out.find("sin(") != std::string::npos);
    REQUIRE(out.find("Pi") != std::string::npos);
    REQUIRE(out.find("K") != std::string::npos);
}

TEST_CASE("Parser: closure with variable and expression", "[parser]")
{
    const std::string input = "let mut x = 1; x+2";
    std::stringstream stream(input);
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);

    REQUIRE(!parser.hasError());
    const std::string out = StringVisitor::visit(ast);
    REQUIRE(out.find("mut x:int = 1;") != std::string::npos);
    REQUIRE(out.find("(x)+(2)") != std::string::npos);
}

TEST_CASE("Parser: function declaration and call", "[parser]")
{
    const std::string input = "fn f(a:int) = a; f(1)";
    std::stringstream stream(input);
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);

    REQUIRE(!parser.hasError());
    const std::string out = StringVisitor::visit(ast);
    REQUIRE(out.find("fn f(") != std::string::npos);
    REQUIRE(out.find("f(") != std::string::npos);
}

TEST_CASE("Parser: branch expression (if/else)", "[parser]")
{
    const std::string input = "if true { 1 } else { 2 }";
    std::stringstream stream(input);
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);

    REQUIRE(!parser.hasError());
    const std::string out = StringVisitor::visit(ast);
    REQUIRE(out.find("if ") != std::string::npos);
    REQUIRE(out.find("else") != std::string::npos);
}
