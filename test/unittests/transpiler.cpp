#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "TranspileVisitor.h"
#include "internal/Lexer.h"
#include "internal/Parser.h"
#include "internal/Transpiler.h"

using namespace PExpr;
using namespace PExpr::internal;

// Minimal TranspileVisitor implementation returning string payloads.
// We only need a concrete class to instantiate the Transpiler; methods won't be invoked
// by the current Transpiler::handle(const Ptr<Closure>&) implementation (it returns a default Payload).
class DummyVisitor : public TranspileVisitor<std::string> {
public:
    std::string onVariable(const std::string& name, ElementaryType) override { return name; }
    std::string onInteger(Integer v) override { return std::to_string(v); }
    std::string onNumber(Number v) override { return std::to_string(v); }
    std::string onBool(bool v) override { return v ? "true" : "false"; }
    std::string onString(const std::string& v) override { return "\"" + v + "\""; }
    std::string onCast(const std::string& v, ElementaryType, ElementaryType) override { return v; }
    std::string onPosNeg(bool isNeg, ElementaryType, const std::string& v) override { return (isNeg ? "-" : "+") + v; }
    std::string onNot(const std::string& v) override { return "!" + v; }
    std::string onAddSub(bool isSub, ElementaryType, const std::string& a, const std::string& b) override
    {
        return "(" + a + (isSub ? "-" : "+") + b + ")";
    }
    std::string onMulDiv(bool isDiv, ElementaryType, const std::string& a, const std::string& b) override
    {
        return "(" + a + (isDiv ? "/" : "*") + b + ")";
    }
    std::string onScale(bool isDiv, ElementaryType, const std::string& a, const std::string& f) override
    {
        return "(" + a + (isDiv ? "/" : "*") + f + ")";
    }
    std::string onPow(ElementaryType, const std::string& a, const std::string& f) override
    {
        return "(" + a + "^" + f + ")";
    }
    std::string onMod(const std::string& a, const std::string& b) override { return "(" + a + "%" + b + ")"; }
    std::string onAndOr(bool isOr, const std::string& a, const std::string& b) override
    {
        return "(" + a + (isOr ? "||" : "&&") + b + ")";
    }
    std::string onRelOp(RelationalOp op, ElementaryType, const std::string& a, const std::string& b) override
    {
        const char* s = (op == RelationalOp::Less) ? "<" : (op == RelationalOp::Greater) ? ">"
                                                       : (op == RelationalOp::LessEqual) ? "<="
                                                                                         : ">=";
        return "(" + a + s + b + ")";
    }
    std::string onEqual(bool isNeg, ElementaryType, const std::string& a, const std::string& b) override
    {
        return isNeg ? "!(" + a + "==" + b + ")" : "(" + a + "==" + b + ")";
    }
    std::string onFunctionCall(const std::string& name, ElementaryType, const std::vector<ElementaryType>&, const std::vector<std::string>& args) override
    {
        std::string s = name + "(";
        for (size_t i = 0; i < args.size(); ++i) {
            s += args[i];
            if (i + 1 < args.size())
                s += ", ";
        }
        s += ")";
        return s;
    }
    std::string onAccess(const std::string& v, size_t, const std::vector<uint8>& perm) override
    {
        std::string s = v + ".";
        for (size_t i = 0; i < perm.size(); ++i)
            s += std::to_string(perm[i]);
        return s;
    }
};

TEST_CASE("Transpiler can be instantiated and returns default payload for closure", "[transpiler]")
{
    Reporter reporter;
    reporter.setQuiet(true);
    std::stringstream stream("1+2");
    Lexer lexer(stream, reporter);
    Parser parser(lexer, reporter);
    SymbolTable globals;

    auto ast = parser.parse(&globals);

    DummyVisitor visitor;
    Transpiler<std::string> transpiler(globals, &visitor);

    REQUIRE(transpiler.handle(ast) == "(1+2)");
}
