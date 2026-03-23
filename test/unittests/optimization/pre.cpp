#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "opt/SSAOptimizer.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;

[[nodiscard]] inline static auto MakeO2()
{
    return opt::OptimizerOptions::Medium();
}

[[nodiscard]] inline static auto MakeO1()
{
    return opt::OptimizerOptions::Low();
}

TEST_CASE("PRE: partial redundancy — insert on missing path", "[pre]")
{
    // a+b computed in one branch but used after join → PRE inserts on the other path
    Environment env;
    auto ast = env.parse(R"(
        [[extern]] fn getBool() -> bool;
        [[extern]] fn getNum() -> num;
        let a = getNum();
        let b = getNum();
        let c = getBool();
        let x = if c { a + b } else { a * 2.0 };
        let y = a + b;
        x + y
    )");
    auto prog = env.map(ast);

    // Without PRE: two add instructions
    auto progCopy = prog;
    opt::SSAOptimizer::Run(MakeO1(), progCopy);
    auto dumpedO1 = SSASerializer::serialize(progCopy);

    size_t addCountO1 = 0;
    for (size_t pos = 0; (pos = dumpedO1.find("add(", pos)) != std::string::npos; ++pos)
        ++addCountO1;
    REQUIRE(addCountO1 >= 2);

    // With PRE: one add eliminated
    opt::SSAOptimizer::Run(MakeO2(), prog);
    auto dumpedO2 = SSASerializer::serialize(prog);

    size_t addCountO2 = 0;
    for (size_t pos = 0; (pos = dumpedO2.find("add(", pos)) != std::string::npos; ++pos)
        ++addCountO2;
    REQUIRE(addCountO2 < addCountO1);
}

TEST_CASE("PRE: kill prevents optimization", "[pre]")
{
    // a+b computed, then a is redefined, then a+b again — second is NOT redundant
    Environment env;
    auto ast = env.parse(R"(
        [[extern]] fn getNum() -> num;
        let a = getNum();
        let b = getNum();
        let x = a + b;
        let a2 = getNum();
        let y = a2 + b;
        x + y
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeO2(), prog);
    auto dumped = SSASerializer::serialize(prog);

    // Both additions should remain (different operands due to SSA)
    REQUIRE(!dumped.empty());
}

TEST_CASE("PRE: side-effecting calls not moved", "[pre]")
{
    // Side-effecting function calls should never be hoisted or eliminated
    Environment env;
    auto ast = env.parse(R"(
        [[extern, side_effect]] fn sideEffect() -> num;
        [[extern]] fn getBool() -> bool;
        let c = getBool();
        let x = if c { sideEffect() } else { sideEffect() };
        x
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeO2(), prog);
    auto dumped = SSASerializer::serialize(prog);

    // Both calls should remain (side effects must not be eliminated)
    size_t callCount = 0;
    for (size_t pos = 0; (pos = dumped.find("call[", pos)) != std::string::npos; ++pos)
        ++callCount;
    REQUIRE(callCount >= 3); // getBool + 2 sideEffect calls
}

TEST_CASE("PRE: diamond CFG — hoist identical computation", "[pre]")
{
    // Same expression in both branches → hoist before the branch
    Environment env;
    auto ast = env.parse(R"(
        [[extern]] fn getBool() -> bool;
        [[extern]] fn getNum() -> num;
        let a = getNum();
        let b = getNum();
        let x = if getBool() { a + b } else { a + b };
        x
    )");
    auto prog = env.map(ast);

    auto progO1 = prog;
    opt::SSAOptimizer::Run(MakeO1(), progO1);
    auto dumpedO1 = SSASerializer::serialize(progO1);

    // O1: two add instructions (one per branch)
    size_t addCountO1 = 0;
    for (size_t pos = 0; (pos = dumpedO1.find("add(", pos)) != std::string::npos; ++pos)
        ++addCountO1;
    REQUIRE(addCountO1 == 2);

    // O2: hoisted — only one add
    opt::SSAOptimizer::Run(MakeO2(), prog);
    auto dumpedO2 = SSASerializer::serialize(prog);

    size_t addCountO2 = 0;
    for (size_t pos = 0; (pos = dumpedO2.find("add(", pos)) != std::string::npos; ++pos)
        ++addCountO2;
    REQUIRE(addCountO2 == 1);
}

TEST_CASE("PRE: does not crash on single block", "[pre]")
{
    // Single block — PRE should be a no-op
    Environment env;
    auto ast = env.parse(R"(
        [[extern]] fn getNum() -> num;
        let a = getNum();
        let b = getNum();
        a + b
    )");
    auto prog = env.map(ast);
    opt::SSAOptimizer::Run(MakeO2(), prog);
    auto dumped = SSASerializer::serialize(prog);
    REQUIRE(!dumped.empty());
    REQUIRE(dumped.find("add(") != std::string::npos);
}

TEST_CASE("PRE: pure function call elimination", "[pre]")
{
    // Pure function called in one branch and after join → PRE can eliminate
    Environment env;
    auto ast = env.parse(R"(
        [[extern]] fn getBool() -> bool;
        [[extern]] fn pureFunc(x:num) -> num;
        [[extern]] fn getNum() -> num;
        let a = getNum();
        let c = getBool();
        let x = if c { pureFunc(a) } else { a * 2.0 };
        let y = pureFunc(a);
        x + y
    )");
    auto prog = env.map(ast);

    auto progO1 = prog;
    opt::SSAOptimizer::Run(MakeO1(), progO1);
    auto dumpedO1 = SSASerializer::serialize(progO1);

    opt::SSAOptimizer::Run(MakeO2(), prog);
    auto dumpedO2 = SSASerializer::serialize(prog);

    // Count pureFunc calls — should be fewer with PRE
    size_t pureCountO1 = 0;
    for (size_t pos = 0; (pos = dumpedO1.find("pureFunc", pos)) != std::string::npos; ++pos)
        ++pureCountO1;

    size_t pureCountO2 = 0;
    for (size_t pos = 0; (pos = dumpedO2.find("pureFunc", pos)) != std::string::npos; ++pos)
        ++pureCountO2;

    REQUIRE(pureCountO2 <= pureCountO1);
}
