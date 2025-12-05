#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "internal/Lexer.h"

using namespace PExpr;
using namespace PExpr::internal;

static bool check_sequence(const std::string& input, const std::vector<TokenType>& expected)
{
    Reporter reporter;
    reporter.setQuiet(true);

    std::stringstream stream(input);
    Lexer lexer(stream, reporter);

    for (size_t i = 0; i < expected.size(); ++i) {
        Token t = lexer.next();
        if (t.Type != expected[i]) {
            CHECK(expected[i] == t.Type);
            return false;
        }
    }
    // ensure next token is EOF
    Token t = lexer.next();
    return t.Type == TokenType::Eof;
}

TEST_CASE("Lexer: identifiers, numbers and symbols", "[lexer]")
{
    REQUIRE(check_sequence("abc(231*22.231*2.42e-3).xyz",
                           { TokenType::Identifier,
                             TokenType::OpenParentheses,
                             TokenType::IntegerLiteral,
                             TokenType::Mul,
                             TokenType::NumberLiteral,
                             TokenType::Mul,
                             TokenType::NumberLiteral,
                             TokenType::ClosedParentheses,
                             TokenType::Dot,
                             TokenType::Identifier }));
}

TEST_CASE("Lexer: keywords and booleans", "[lexer]")
{
    REQUIRE(check_sequence("if elif else mut fn true false",
                           { TokenType::If, TokenType::Elif, TokenType::Else, TokenType::Mutable, TokenType::Function, TokenType::BooleanLiteral, TokenType::BooleanLiteral }));
}

TEST_CASE("Lexer: comparison operators", "[lexer]")
{
    REQUIRE(check_sequence("a==b!=c<=d>=e<f>g",
                           { TokenType::Identifier, TokenType::Equal, TokenType::Identifier, TokenType::NotEqual, TokenType::Identifier,
                             TokenType::LessEqual, TokenType::Identifier, TokenType::GreaterEqual, TokenType::Identifier,
                             TokenType::Less, TokenType::Identifier, TokenType::Greater, TokenType::Identifier }));
}

TEST_CASE("Lexer: numeric formats", "[lexer]")
{
    REQUIRE(check_sequence("123 22.231 2.42e-3",
                           { TokenType::IntegerLiteral, TokenType::NumberLiteral, TokenType::NumberLiteral }));
}

TEST_CASE("Lexer: string literal", "[lexer]")
{
    REQUIRE(check_sequence("\"hello world\"",
                           { TokenType::StringLiteral }));
}

TEST_CASE("Lexer: comments are skipped", "[lexer]")
{
    REQUIRE(check_sequence("1 // this is a comment\n2",
                           { TokenType::IntegerLiteral, TokenType::IntegerLiteral }));
}

TEST_CASE("Lexer: combined complex example", "[lexer]")
{
    REQUIRE(check_sequence("foo(1, -2, true) <= bar.x",
                           { TokenType::Identifier, TokenType::OpenParentheses,
                             TokenType::IntegerLiteral, TokenType::Comma, TokenType::Minus, TokenType::IntegerLiteral, TokenType::Comma,
                             TokenType::BooleanLiteral, TokenType::ClosedParentheses, TokenType::LessEqual, TokenType::Identifier, TokenType::Dot, TokenType::Identifier }));
}
