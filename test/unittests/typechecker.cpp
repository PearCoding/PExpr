#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "internal/Lexer.h"
#include "internal/Parser.h"
#include "internal/TypeChecker.h"

using namespace PExpr;
using namespace PExpr::internal;

TEST_CASE("TypeChecker: integer arithmetic", "[typechecker]") {
    std::stringstream stream("1+2");
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);
    TypeChecker tc(globals);
    auto t = tc.handle(ast);

    REQUIRE(t == ElementaryType::Integer);
}

TEST_CASE("TypeChecker: number arithmetic", "[typechecker]") {
    std::stringstream stream("1.5+2.25");
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);
    TypeChecker tc(globals);
    auto t = tc.handle(ast);

    REQUIRE(t == ElementaryType::Number);
}

TEST_CASE("TypeChecker: mixed int and number yields number", "[typechecker]") {
    std::stringstream stream("1+2.0");
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);
    TypeChecker tc(globals);
    auto t = tc.handle(ast);

    REQUIRE(t == ElementaryType::Number);
}

TEST_CASE("TypeChecker: variable declaration registers variable and used in expression", "[typechecker]") {
    std::stringstream stream("mut x = 1; x = x+4; x+2");
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);
    TypeChecker tc(globals);
    auto t = tc.handle(ast);

    REQUIRE(t == ElementaryType::Integer);
}

TEST_CASE("TypeChecker: string literal", "[typechecker]") {
    std::stringstream stream("\"hello\"");
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);
    TypeChecker tc(globals);
    auto t = tc.handle(ast);

    REQUIRE(t == ElementaryType::String);
}
