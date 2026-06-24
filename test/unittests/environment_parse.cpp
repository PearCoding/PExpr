#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "Environment.h"

using namespace PExpr;

// Regression: parse(string_view) constructed the lexer stream from str.data(),
// treating the (not necessarily null-terminated) view as a C-string. A view over
// a prefix of a larger buffer then leaked the trailing bytes into the parse.
TEST_CASE("Environment::parse respects string_view bounds", "[environment][parser]")
{
    // Full buffer is a valid prefix followed by tokens that would make the parse
    // fail; the view covers only the valid prefix.
    std::string buffer = "1 + 2 )";
    std::string_view view(buffer.data(), 5); // "1 + 2"
    REQUIRE(view == "1 + 2");

    Environment env;
    env.reporter().setQuiet(true);
    auto closure = env.parse(view);

    REQUIRE(closure != nullptr);
    REQUIRE(env.reporter().errorCount() == 0);
}
