#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "internal/Lexer.h"
#include "internal/Parser.h"
#include "StringVisitor.h"

using namespace PExpr;
using namespace PExpr::internal;

TEST_CASE("Parser builds AST for simple arithmetic", "[parser]") {
    std::stringstream stream("a+1");
    Lexer lexer(stream);
    Parser parser(lexer);
    SymbolTable globals;

    auto ast = parser.parse(&globals);

    REQUIRE(!parser.hasError());
    REQUIRE(StringVisitor::visit(ast) == "(a)+(1)");
}
