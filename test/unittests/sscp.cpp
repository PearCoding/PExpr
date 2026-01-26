#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSAOptimizer.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;
using namespace PExpr::internal;

[[nodiscard]] inline static auto MakeConstantFoldingOptimizer()
{
    auto opts                        = SSAOptions::None();
    opts.EnableConstantFolding       = true;
    opts.EnableConstantFoldingNumber = true;
    opts.RemoveDeadCode              = true;
    return opts;
}

TEST_CASE("SSAOptimizer: constant folding of binary ops", "[sscp]")
{
    std::stringstream stream("let mut a = 2; let mut b = 3; let mut c = a + b; c");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    // run SSCP pass
    SSAOptimizer::Run(MakeConstantFoldingOptimizer(), prog);

    auto dumped = SSASerializer::serialize(prog);

    // Expect the constant value "5" present
    REQUIRE(dumped.find("5") != std::string::npos);
}

TEST_CASE("SSAOptimizer: dead code elimination removes unused assigns", "[sscp]")
{
    std::stringstream stream("let x = 1; let y = 2; x");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    // Ensure y assign exists before pass (sanity)
    auto before = SSASerializer::serialize(prog);
    REQUIRE((before.find("y.") != std::string::npos || before.find("y:") != std::string::npos));

    SSAOptimizer::Run(MakeConstantFoldingOptimizer(), prog);

    auto after = SSASerializer::serialize(prog);

    // After pass, 'y' assignment should be removed (dead)
    REQUIRE(after.find("y.") == std::string::npos);
    // x's return should still be present
    REQUIRE(after.find("return ") != std::string::npos);
}

TEST_CASE("SSAOptimizer: constant folding for vectors", "[sscp]")
{
    std::stringstream stream("let v1 = [1.0, 2.0]; let v2 = [3.0, 4.0]; let v3 = v1 + v2; v3.x");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    // run SSCP pass
    SSAOptimizer::Run(MakeConstantFoldingOptimizer(), prog);

    auto dumped = SSASerializer::serialize(prog);

    // The vector addition should be folded to 4.0
    REQUIRE(dumped.find("4") != std::string::npos);
}

TEST_CASE("SSAOptimizer: vector arithmetic operations", "[sscp]")
{
    // Test various vector operations: add, sub, mul, div
    std::stringstream stream("let v1 = [1.0, 2.0, 3.0]; let v2 = [2.0, 3.0, 4.0]; let add = v1 + v2; let sub = v1 - v2; let mul = v1 * v2; let div = v1 / v2; add.x + sub.y + mul.z + div.x");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    SSAOptimizer::Run(MakeConstantFoldingOptimizer(), prog);

    auto dumped = SSASerializer::serialize(prog);

    // Check that constant folding occurred for vector operations
    REQUIRE(dumped.find("14.5") != std::string::npos);
}
