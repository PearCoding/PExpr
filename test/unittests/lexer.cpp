#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "internal/Lexer.h"

using namespace PExpr;
using namespace PExpr::internal;

TEST_CASE("Lexer tokenizes identifiers, numbers and symbols", "[lexer]") {
    std::stringstream stream("abc(231*22.231*2.42e-3).xyz");
    Lexer lexer(stream);

    REQUIRE(lexer.next().Type == TokenType::Identifier);
    REQUIRE(lexer.next().Type == TokenType::OpenParentheses);
    REQUIRE(lexer.next().Type == TokenType::IntegerLiteral);
    REQUIRE(lexer.next().Type == TokenType::Mul);
    REQUIRE(lexer.next().Type == TokenType::NumberLiteral);
    REQUIRE(lexer.next().Type == TokenType::Mul);
    REQUIRE(lexer.next().Type == TokenType::NumberLiteral);
    REQUIRE(lexer.next().Type == TokenType::ClosedParentheses);
    REQUIRE(lexer.next().Type == TokenType::Dot);
    REQUIRE(lexer.next().Type == TokenType::Identifier);
    REQUIRE(lexer.next().Type == TokenType::Eof);
}
