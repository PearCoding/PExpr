#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

#include "utils/StringUtils.h"

using namespace PExpr;
using namespace PExpr::utils;

TEST_CASE("StringUtils: string escape/unescape", "[utils][string]")
{
    SECTION("Escape basic strings")
    {
        REQUIRE(escapeString("hello") == "hello");
        REQUIRE(escapeString("he\"llo") == "he\\\"llo");
        REQUIRE(escapeString("he\nllo") == "he\\nllo");
        REQUIRE(escapeString("he\\llo") == "he\\\\llo");
    }

    SECTION("Unescape basic strings")
    {
        REQUIRE(unescapeString("hello") == "hello");
        REQUIRE(unescapeString("he\\\"llo") == "he\"llo");
        REQUIRE(unescapeString("he\\nllo") == "he\nllo");
        REQUIRE(unescapeString("he\\\\llo") == "he\\llo");
    }

    SECTION("Round-trip escape/unescape")
    {
        std::string testStrings[] = {
            "hello world",
            "he\"llo\"world",
            "line1\nline2\nline3",
            "tab\ttab\ttab",
            "back\\slash",
            "mixed\"quotes\nand\ttabs\\slashes"
        };

        for (const auto& original : testStrings) {
            std::string escaped   = escapeString(original);
            std::string unescaped = unescapeString(escaped);
            REQUIRE(unescaped == original);
        }
    }
}
TEST_CASE("StringUtils: formatNumber round-trips without precision loss", "[utils][number]")
{
    // Regression: serializers printed Numbers with the default 6-significant-digit
    // stream precision, so values like pi did not survive a serialize/deserialize.
    const double values[] = { 3.141592653589793, 0.1, 1e20, -2.5e-13, 123456789.123456789 };
    for (double v : values) {
        std::string s = formatNumber(v);
        REQUIRE(std::stod(s) == v);
    }

    // Common values keep their short, human-friendly form.
    REQUIRE(formatNumber(0.1) == "0.1");
    REQUIRE(formatNumber(2.5) == "2.5");
}
