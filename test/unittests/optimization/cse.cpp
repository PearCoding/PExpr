#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "opt/SSAOptimizer.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;

[[nodiscard]] inline static auto MakeCSEOnlyOption()
{
    auto options                          = opt::OptimizerOptions::None();
    options.EliminateCommonSubexpressions = true;
    options.EnableConstantFolding         = true;
    options.RemoveDeadCode                = true;
    return options;
}

TEST_CASE("SSAOptimizer: common subexpression elimination basic", "[sscp][cse]")
{
    std::stringstream stream("let a = 2.0; let b = 3.0; let x = a * b; let y = a * b; x + y");
    Environment env;
    auto ast = env.parse(stream);

    auto prog = env.map(ast);

    // Count occurrences of "a * b" before optimization
    auto before             = SSASerializer::serialize(prog);
    size_t mul_count_before = 0;
    size_t pos              = 0;
    while ((pos = before.find("mul(", pos)) != std::string::npos) {
        mul_count_before++;
        pos += 4;
    }

    // Run SSCP pass
    opt::SSAOptimizer::Run(MakeCSEOnlyOption(), prog);

    auto after = SSASerializer::serialize(prog);

    // Count occurrences of "mul" after optimization
    size_t mul_count_after = 0;
    pos                    = 0;
    while ((pos = after.find("mul(", pos)) != std::string::npos) {
        mul_count_after++;
        pos += 4;
    }

    // Common subexpression elimination should reduce the number of multiplications
    REQUIRE(mul_count_after < mul_count_before);
    // At least one multiplication should remain
    REQUIRE(mul_count_after >= 1);
}

TEST_CASE("SSAOptimizer: common subexpression elimination with constants", "[sscp][cse]")
{
    std::stringstream stream("@[extern, pure] fn sin(a:num)->num; @[extern, pure] fn cos(a:num)->num; let a = 5.0; let x = sin(a) * cos(a); let y = sin(a) * cos(a); x + y");
    Environment env;
    auto ast = env.parse(stream);

    auto prog = env.map(ast);

    auto before = SSASerializer::serialize(prog);

    // Run SSCP pass
    opt::SSAOptimizer::Run(MakeCSEOnlyOption(), prog);

    auto after = SSASerializer::serialize(prog);

    // Check that we have fewer sin/cos calls (CSE should eliminate duplicates)
    size_t sin_count_before = 0, sin_count_after = 0;
    size_t cos_count_before = 0, cos_count_after = 0;
    size_t mul_count_before = 0, mul_count_after = 0;

    size_t pos = 0;
    while ((pos = before.find("call[_Z3sin", pos)) != std::string::npos) {
        sin_count_before++;
        pos += 12;
    }
    pos = 0;
    while ((pos = before.find("call[_Z3cos", pos)) != std::string::npos) {
        cos_count_before++;
        pos += 12;
    }
    pos = 0;
    while ((pos = before.find("mul(", pos)) != std::string::npos) {
        mul_count_before++;
        pos += 4;
    }

    pos = 0;
    while ((pos = after.find("call[_Z3sin", pos)) != std::string::npos) {
        sin_count_after++;
        pos += 12;
    }
    pos = 0;
    while ((pos = after.find("call[_Z3cos", pos)) != std::string::npos) {
        cos_count_after++;
        pos += 12;
    }
    pos = 0;
    while ((pos = after.find("mul(", pos)) != std::string::npos) {
        mul_count_after++;
        pos += 4;
    }

    // CSE should eliminate duplicate sin/cos calls and multiplications
    REQUIRE(sin_count_after <= sin_count_before);
    REQUIRE(cos_count_after <= cos_count_before);
    REQUIRE(mul_count_after < mul_count_before);
}

TEST_CASE("SSAOptimizer: common subexpression elimination with different names", "[sscp][cse]")
{
    // Same computation with different variable names should still be eliminated
    Environment env;
    auto ast = env.parse("let a = 2.0; let b = 3.0; let c = 4.0; let x = a + b; let y = b + c; let z = a + b; x + y + z");

    auto prog = env.map(ast);

    auto before             = SSASerializer::serialize(prog);
    size_t add_count_before = 0;
    size_t pos              = 0;
    while ((pos = before.find("add(", pos)) != std::string::npos) {
        add_count_before++;
        pos += 4;
    }

    // Run SSCP pass
    opt::SSAOptimizer::Run(MakeCSEOnlyOption(), prog);

    auto after             = SSASerializer::serialize(prog);
    size_t add_count_after = 0;
    pos                    = 0;
    while ((pos = after.find("add(", pos)) != std::string::npos) {
        add_count_after++;
        pos += 4;
    }

    // a+b appears twice (x and z), should be eliminated to one computation
    REQUIRE(add_count_after < add_count_before);
}

TEST_CASE("SSAOptimizer: common subexpression elimination preserves side effects", "[sscp][cse]")
{
    // Functions with side effects should not be eliminated
    Environment env;
    auto ast = env.parse(R"(
        @[extern] fn side_effect() -> num;
        let x = side_effect();
        let y = side_effect();
        x + y
    )");

    auto prog = env.map(ast);

    auto before                     = SSASerializer::serialize(prog);
    size_t side_effect_count_before = 0;
    size_t pos                      = 0;
    while ((pos = before.find("call[_Z11side_effect", pos)) != std::string::npos) {
        side_effect_count_before++;
        pos += 21;
    }

    // Run SSCP pass
    opt::SSAOptimizer::Run(MakeCSEOnlyOption(), prog);

    auto after                     = SSASerializer::serialize(prog);
    size_t side_effect_count_after = 0;
    pos                            = 0;
    while ((pos = after.find("call[_Z11side_effect", pos)) != std::string::npos) {
        side_effect_count_after++;
        pos += 21;
    }

    // Side-effect calls should not be eliminated
    REQUIRE(side_effect_count_after == side_effect_count_before);
}

TEST_CASE("SSAOptimizer: common subexpression elimination complex pattern", "[sscp][cse]")
{
    Environment env;
    auto ast = env.parse(R"(
        let a = 2.0;
        let b = 3.0;
        let c = 4.0;
        let x = (a + b) * c;
        let y = (a + b) * c;
        let z = (b + a) * c;  // Same as above due to commutativity
        x + y + z
    )");

    auto prog = env.map(ast);

    auto before             = SSASerializer::serialize(prog);
    size_t add_count_before = 0, mul_count_before = 0;
    size_t pos = 0;
    while ((pos = before.find("add(", pos)) != std::string::npos) {
        add_count_before++;
        pos += 4;
    }
    pos = 0;
    while ((pos = before.find("mul(", pos)) != std::string::npos) {
        mul_count_before++;
        pos += 4;
    }

    // Run SSCP pass
    opt::SSAOptimizer::Run(MakeCSEOnlyOption(), prog);

    auto after             = SSASerializer::serialize(prog);
    size_t add_count_after = 0, mul_count_after = 0;
    pos = 0;
    while ((pos = after.find("add(", pos)) != std::string::npos) {
        add_count_after++;
        pos += 4;
    }
    pos = 0;
    while ((pos = after.find("mul(", pos)) != std::string::npos) {
        mul_count_after++;
        pos += 4;
    }

    // Should eliminate duplicate (a+b)*c computations
    REQUIRE(add_count_after < add_count_before);
    REQUIRE(mul_count_after < mul_count_before);
}