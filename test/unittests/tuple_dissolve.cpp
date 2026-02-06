#include <catch2/catch_test_macros.hpp>

#include "Environment.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;

[[nodiscard]] inline static auto MakeTupleOptimization()
{
    auto opts           = opt::OptimizerOptions::Minimum();
    opts.RemoveDeadCode = true;
    opts.DissolveTuples = true;
    return opts;
}

TEST_CASE("SSATupleDissolvePass - Simple tuple creation and access", "[tuple_dissolve]")
{
    const char* source = R"(
        let t = [1.0, 2.0, 3.0];
        let x = t[0];
        x
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Before dissolve, we should have tuple values
    bool hasTuplesBefore = false;
    for (const auto& instr : program.Body) {
        instr->forEachValue([&hasTuplesBefore](const ssa::SSAValue& val) {
            if (val.type().isTuple())
                hasTuplesBefore = true;
        });
    }
    REQUIRE(hasTuplesBefore);

    // Apply dissolve pass
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // After dissolve, we should have no tuple values
    bool hasTuplesAfter = false;
    for (const auto& instr : program.Body) {
        instr->forEachValue([&hasTuplesAfter](const ssa::SSAValue& val) {
            if (val.type().isTuple())
                hasTuplesAfter = true;
        });
    }
    REQUIRE_FALSE(hasTuplesAfter);
}

TEST_CASE("SSATupleDissolvePass - Nested tuple access", "[tuple_dissolve]")
{
    const char* source = R"(
        let t = [1.0, 2.0];
        let s = [t, 3.0];
        let x = s[0];
        x[1]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply dissolve pass iteratively until no tuples remain
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Verify no tuples remain
    bool hasTuples = false;
    for (const auto& instr : program.Body) {
        instr->forEachValue([&hasTuples](const ssa::SSAValue& val) {
            if (val.type().isTuple())
                hasTuples = true;
        });
    }
    REQUIRE_FALSE(hasTuples);
}

TEST_CASE("SSATupleDissolvePass - Tuple in phi node", "[tuple_dissolve]")
{
    const char* source = R"(
        let t = if true { [1.0, 2.0] } else { [3.0, 4.0] };
        t.x
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply dissolve pass iteratively
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Verify no tuples remain
    bool hasTuples = false;
    for (const auto& instr : program.Body) {
        instr->forEachValue([&hasTuples](const ssa::SSAValue& val) {
            if (val.type().isTuple())
                hasTuples = true;
        });
    }
    REQUIRE_FALSE(hasTuples);
}

TEST_CASE("SSATupleDissolvePass - Swizzle operation", "[tuple_dissolve]")
{
    const char* source = R"(
        let v = [1.0, 2.0, 3.0];
        let s = v.xy;
        s[0]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply dissolve pass iteratively
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Verify no tuples remain
    bool hasTuples = false;
    for (const auto& instr : program.Body) {
        instr->forEachValue([&hasTuples](const ssa::SSAValue& val) {
            if (val.type().isTuple())
                hasTuples = true;
        });
    }
    REQUIRE_FALSE(hasTuples);
}

TEST_CASE("SSATupleDissolvePass - Tuple cast", "[tuple_dissolve]")
{
    const char* source = R"(
        let t = [1, 2];
        let s = t as [num, num];
        s.x
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply dissolve pass iteratively
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Verify no tuples remain
    bool hasTuples = false;
    for (const auto& instr : program.Body) {
        instr->forEachValue([&hasTuples](const ssa::SSAValue& val) {
            if (val.type().isTuple())
                hasTuples = true;
        });
    }
    REQUIRE_FALSE(hasTuples);
}
