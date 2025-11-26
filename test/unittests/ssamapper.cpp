#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "SSAMapper.h"
#include "internal/Lexer.h"
#include "internal/Parser.h"

using namespace PExpr;
using namespace PExpr::ssa;
using namespace PExpr::internal;

TEST_CASE("SSAMapper: simple variable and expression", "[ssamapper]")
{
    std::stringstream stream("mut x = 1; x+2");
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = prog.dump();

    // Expect an assignment for x, and a return
    REQUIRE(dumped.find("assign(") != std::string::npos);
    REQUIRE(dumped.find("x.") != std::string::npos);
    REQUIRE(dumped.find("return ") != std::string::npos);
}

TEST_CASE("SSAMapper: function declaration and call", "[ssamapper]")
{
    std::stringstream stream("fn f(a:int) = a; f(1)");
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = prog.dump();

    // Expect a function named 'f' and a call to f in main body
    REQUIRE(dumped.find("fn f(") != std::string::npos);
    REQUIRE(dumped.find("call f(") != std::string::npos);
}

TEST_CASE("SSAMapper: branch produces phi", "[ssamapper]")
{
    std::stringstream stream("if true { 1 } else { 2 }");
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = prog.dump();

    // Expect a phi node for merged branch results
    REQUIRE(dumped.find("phi(") != std::string::npos);
}
