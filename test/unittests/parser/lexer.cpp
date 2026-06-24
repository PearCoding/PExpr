#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "parser/Lexer.h"

using namespace PExpr;
using namespace PExpr::parser;

static bool check_sequence(const std::string& input, const std::vector<TokenType>& expected)
{
    utils::Reporter reporter;
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

TEST_CASE("Lexer: integer literal overflow is reported", "[lexer]")
{
    SECTION("INT64_MAX parses without error")
    {
        utils::Reporter reporter;
        reporter.setQuiet(true);
        std::stringstream stream("9223372036854775807");
        Lexer lexer(stream, reporter);

        Token t = lexer.next();
        REQUIRE(t.Type == TokenType::IntegerLiteral);
        REQUIRE(reporter.errorCount() == 0);
        REQUIRE(std::get<Integer>(t.Value) == 9223372036854775807LL);
    }

    SECTION("Above INT64_MAX is reported instead of silently wrapping")
    {
        // Regression: this used strtoull + cast, so it became -9223372036854775808.
        utils::Reporter reporter;
        reporter.setQuiet(true);
        std::stringstream stream("9223372036854775808");
        Lexer lexer(stream, reporter);

        lexer.next();
        REQUIRE(reporter.errorCount() > 0);
    }
}

TEST_CASE("Lexer: oversized location directive does not crash", "[lexer]")
{
    // Regression: the //! location line number was parsed with std::stoull, which
    // threw std::out_of_range (uncaught) on an oversized value.
    utils::Reporter reporter;
    reporter.setQuiet(true);
    std::stringstream stream("//! location 999999999999999999999999 \"f.pexpr\"\n42");
    Lexer lexer(stream, reporter);

    Token t = lexer.next();
    REQUIRE(t.Type == TokenType::IntegerLiteral); // reached the '42' without throwing
}
