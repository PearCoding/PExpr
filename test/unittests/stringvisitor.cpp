#include <catch2/catch_test_macros.hpp>

#include "StringVisitor.h"
#include "internal/Parser.h"
#include "internal/SymbolTable.h"

using namespace PExpr;

inline static auto parseOnly(std::string_view str)
{
    Reporter reporter;
    reporter.setQuiet(true);
    auto stream = std::istringstream(str.data());
    internal::Lexer lexer(stream, reporter);
    internal::Parser parser(lexer, reporter);
    internal::SymbolTable globals;
    return parser.parse(&globals);
}

TEST_CASE("String visitor should produce a correct parsable version", "[stringvisitor]")
{
    auto ast1 = parseOnly("abc(231*22.231*2.42e-3).xyz");

    REQUIRE(ast1 != nullptr);

    const std::string src1 = StringVisitor::visit(ast1);

    auto ast2 = parseOnly(src1);
    REQUIRE(ast2 != nullptr);

    const std::string src2 = StringVisitor::visit(ast2);
    REQUIRE(src1 == src2);
}