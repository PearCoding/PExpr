#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"

using namespace PExpr;
using namespace PExpr::ssa;
using namespace PExpr::internal;

TEST_CASE("SSAMapper: simple variable and expression", "[ssamapper]")
{
    std::stringstream stream("let mut x = 1; x+2");
    Environment env;
    auto ast = env.parse(stream);

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
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = prog.dump();

    // Expect a function named '_Z1f*' (mangled) and a call to f in main body
    REQUIRE(dumped.find("fn _Z1f") != std::string::npos);
    REQUIRE(dumped.find("call[_Z1f") != std::string::npos);
}

TEST_CASE("SSAMapper: branch produces phi", "[ssamapper]")
{
    std::stringstream stream("if true { 1 } else { 2 }");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog   = mapper.map(ast);
    auto dumped = prog.dump();

    // Expect a phi node for merged branch results
    REQUIRE(dumped.find("phi(") != std::string::npos);
}

TEST_CASE("SSAMapper: closure captures const parent", "[ssamapper]") {
    std::stringstream stream("let k = 1; let c = { k + 2 }; c");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    bool found = false;
    for (const auto& f : prog.Functions) {
        if (f.Name.rfind("closure", 0) == 0) {
            found = true;
            REQUIRE(f.AccessedConstParents.find("k") != f.AccessedConstParents.end());
            REQUIRE(f.AccessedMutableParents.empty());
        }
    }
    REQUIRE(found);
}

TEST_CASE("SSAMapper: closure captures mutable parent", "[ssamapper]") {
    std::stringstream stream("let mut k = 1; let c = { k + 2 }; c");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    bool found = false;
    for (const auto& f : prog.Functions) {
        if (f.Name.rfind("closure", 0) == 0) {
            found = true;
            REQUIRE(f.AccessedMutableParents.find("k") != f.AccessedMutableParents.end());
            REQUIRE(f.AccessedConstParents.empty());
        }
    }
    REQUIRE(found);
}

TEST_CASE("SSAMapper: recursion function mapping", "[ssamapper]") {
    std::stringstream stream("fn fact(n:int) = if n < 2 { 1 } else { n * fact(n - 1) }; fact(5)");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    bool foundFunc = false;
    for (const auto& f : prog.Functions) {
        if (f.Name.find("fact") != std::string::npos) {
            foundFunc = true;
            break;
        }
    }
    REQUIRE(foundFunc);

    auto dumped = prog.dump();
    REQUIRE(dumped.find("call") != std::string::npos);
}

TEST_CASE("SSAMapper: shadowing does not capture outer", "[ssamapper]") {
    std::stringstream stream("let x = 1; let c = { let x = 2; x }; c");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    bool found = false;
    for (const auto& f : prog.Functions) {
        if (f.Name.rfind("closure", 0) == 0) {
            found = true;
            REQUIRE(f.AccessedConstParents.find("x") == f.AccessedConstParents.end());
            REQUIRE(f.AccessedMutableParents.find("x") == f.AccessedMutableParents.end());
        }
    }
    REQUIRE(found);
}
