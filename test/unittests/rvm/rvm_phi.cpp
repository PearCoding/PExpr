#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

#include "Environment.h"
#include "opt/SSAOptimizer.h"
#include "rvm/RVMInterpreter.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMOptimizer.h"
#include "rvm/RVMSerializer.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;
using namespace PExpr::ssa;

// Helper: compile source to RVM with SSA + RVM optimization
static RVMProgram compileToOptimizedRVM(const char* source, opt::OptimizerOptions opts = opt::OptimizerOptions::High())
{
    Environment env;
    auto closure    = env.parse(source);
    auto ssaProgram = env.map(closure);
    env.optimize(ssaProgram, opts);
    RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(ssaProgram);
    RVMOptimizer::optimize(opts, rvmProgram);
    return rvmProgram;
}

// Helper: execute RVM program expecting an integer result
static Integer executeInt(RVMProgram& prog)
{
    RVMInterpreter interp;
    auto result = interp.execute(prog, Type(TypeKind::Integer));
    REQUIRE(std::holds_alternative<Integer>(result));
    return std::get<Integer>(result);
}

// Helper: execute RVM program expecting an integer result, with external bool function
static Integer executeIntWithBool(RVMProgram& prog, bool boolValue)
{
    RVMInterpreter interp;
    interp.registerExternalFunction("_Z7getBool_P",
                                    [boolValue](const std::vector<ValueVariant>&) -> ValueVariant { return boolValue; });
    auto result = interp.execute(prog, Type(TypeKind::Integer));
    REQUIRE(std::holds_alternative<Integer>(result));
    return std::get<Integer>(result);
}

TEST_CASE("RVMMapper: phi node mapping", "[rvm][mapper][phi]")
{
    SECTION("Simple phi with two branches")
    {
        const char* source = R"(
            let x = if true {
                10
            } else {
                20
            };
            x
        )";

        Environment env;
        auto closure = env.parse(source);
        REQUIRE(closure);

        auto ssaProgram = env.map(closure);

        // Verify SSA has phi node
        std::string ssaStr = SSASerializer::serialize(ssaProgram);
        REQUIRE(ssaStr.find("phi[") != std::string::npos);

        // Map to RVM
        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        // Phi nodes are lowered to conditional select sequences with jz and phi_end_ labels
        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());
        REQUIRE(rvmStr.find("phi_end_") != std::string::npos);

        // Verify execution: condition is true, so x should be 10
        REQUIRE(executeInt(rvmProgram) == 10);
    }

    SECTION("Phi with else branch")
    {
        const char* source = R"(
            @[extern, pure] fn getBool() -> bool;
            let x = if getBool() {
                10
            } elif getBool() {
                20
            } else {
                30
            };
            x
        )";

        Environment env;
        auto closure = env.parse(source);
        REQUIRE(closure);

        auto ssaProgram = env.map(closure);

        // Map to RVM
        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        REQUIRE(rvmProgram.size() > 0);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());

        // Verify execution: getBool returns true → first branch wins → 10
        REQUIRE(executeIntWithBool(rvmProgram, true) == 10);
        // Verify execution: getBool returns false → else branch → 30
        REQUIRE(executeIntWithBool(rvmProgram, false) == 30);
    }

    SECTION("Phi with single condition (no else)")
    {
        const char* source = R"(
            @[extern, pure] fn getBool() -> bool;
            let mut x = 22;
            if getBool() {
                x = 10;
            };
            x
        )";

        Environment env;
        auto closure = env.parse(source);
        REQUIRE(closure);

        auto ssaProgram = env.map(closure);

        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        REQUIRE(rvmProgram.size() > 0);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());
        REQUIRE(rvmStr.find("jnz") != std::string::npos);

        // Verify execution
        REQUIRE(executeIntWithBool(rvmProgram, true) == 10);
        REQUIRE(executeIntWithBool(rvmProgram, false) == 22);
    }

    SECTION("Phi with multiple conditions")
    {
        const char* source = R"(
            @[extern, pure] fn getBool() -> bool;
            let x = if getBool() {
                10
            } elif getBool() {
                20
            } elif getBool() {
                30
            } else {
                40
            };
            x
        )";

        Environment env;
        auto closure = env.parse(source);
        REQUIRE(closure);

        auto ssaProgram = env.map(closure);

        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(rvmStr.find("jnz") != std::string::npos);
        REQUIRE(rvmStr.find("phi_end_") != std::string::npos);

        // Verify execution
        REQUIRE(executeIntWithBool(rvmProgram, true) == 10);
        REQUIRE(executeIntWithBool(rvmProgram, false) == 40);
    }

    SECTION("Phi with boolean condition values")
    {
        const char* source = R"(
            let x = if true {
                1
            } else {
                0
            };
            x
        )";

        Environment env;
        auto closure = env.parse(source);
        REQUIRE(closure);

        auto ssaProgram = env.map(closure);

        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());

        // Condition is true, so result should be 1
        REQUIRE(executeInt(rvmProgram) == 1);
    }

    SECTION("Complex branch pattern with multiple variables")
    {
        const char* source = R"(
            @[extern, pure] fn getBool() -> bool;
            let mut x = 1;
            let mut y = 2;
            let mut z = 3;

            if getBool() {
                x = 10;
                y = 20;
            } elif getBool() {
                x = 30;
                z = 40;
            } else {
                y = 50;
                z = 60;
            };

            x + y + z
        )";

        Environment env;
        auto closure = env.parse(source);
        REQUIRE(closure);

        auto ssaProgram = env.map(closure);

        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());
        REQUIRE(rvmStr.find("add") != std::string::npos);

        // getBool() returns true: first branch → x=10, y=20, z=3 → 33
        REQUIRE(executeIntWithBool(rvmProgram, true) == 33);
        // getBool() returns false: else branch → x=1, y=50, z=60 → 111
        REQUIRE(executeIntWithBool(rvmProgram, false) == 111);
    }

    SECTION("Phi with same value in multiple branches")
    {
        const char* source = R"(
            @[extern, pure] fn getBool() -> bool;
            let x = if getBool() {
                42
            } elif getBool() {
                42
            } else {
                42
            };
            x
        )";

        Environment env;
        auto closure = env.parse(source);
        REQUIRE(closure);

        auto ssaProgram = env.map(closure);

        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());
        REQUIRE(rvmStr.find("42") != std::string::npos);

        // Always 42 regardless of condition
        REQUIRE(executeIntWithBool(rvmProgram, true) == 42);
        REQUIRE(executeIntWithBool(rvmProgram, false) == 42);
    }

    SECTION("Variable updated in only one branch (from PExpr)")
    {
        const char* source = R"(
            let mut x = 5;
            let mut y = 10;

            if true {
                x = 15;
            } else {
                y = 20;
            };

            x + y
        )";

        Environment env;
        auto closure = env.parse(source);
        REQUIRE(closure);

        auto ssaProgram = env.map(closure);

        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());
        REQUIRE(rvmStr.find("add") != std::string::npos);

        // true branch: x=15, y=10 → 25
        REQUIRE(executeInt(rvmProgram) == 25);
    }

    SECTION("Regression: phi with constant branches after optimization")
    {
        // This is the bug case: the SSA optimizer collapses if/else with constant
        // branches, removing the branch/label structure. The phi lowering must still
        // produce correct conditional code.
        const char* source = R"(
            @[extern, pure] fn getBool() -> bool;
            let x = if getBool() { 1 } else { 0 };
            x
        )";

        Environment env;
        auto closure = env.parse(source);
        REQUIRE(closure);

        auto ssaProgram = env.map(closure);

        // Apply optimization to collapse the branch structure
        auto opts = opt::OptimizerOptions::Low();
        opt::SSAOptimizer::Run(opts, ssaProgram);

        // Verify the phi still exists in optimized SSA
        std::string ssaStr = SSASerializer::serialize(ssaProgram);
        REQUIRE(ssaStr.find("phi[") != std::string::npos);

        // Map to RVM
        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);

        // Must have a conditional branch (jz) for phi resolution
        REQUIRE(rvmStr.find("jz") != std::string::npos);
        REQUIRE(rvmStr.find("phi_end_") != std::string::npos);
        REQUIRE(rvmStr.find("0:int") != std::string::npos);
        REQUIRE(rvmStr.find("1:int") != std::string::npos);

        // Verify execution: getBool() true → 1, false → 0
        REQUIRE(executeIntWithBool(rvmProgram, true) == 1);
        REQUIRE(executeIntWithBool(rvmProgram, false) == 0);
    }

    SECTION("Regression: checkerboard pattern with constant if/else")
    {
        // Simulates the checkerboard function's final if/else: if c % 2 == 0 { 1 } else { 0 }
        const char* source = R"(
            @[extern, pure] fn getInt() -> int;
            let c = getInt();
            if c % 2 == 0 { 1 } else { 0 }
        )";

        Environment env;
        auto closure = env.parse(source);
        REQUIRE(closure);

        auto ssaProgram = env.map(closure);

        // Apply full optimization
        auto opts = opt::OptimizerOptions::Low();
        opt::SSAOptimizer::Run(opts, ssaProgram);

        // Map to RVM
        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);

        // The RVM must contain the mod computation (not eliminated by DCE)
        REQUIRE(rvmStr.find("mod") != std::string::npos);
        REQUIRE(rvmStr.find("jz") != std::string::npos);
        REQUIRE(rvmStr.find("0:int") != std::string::npos);
        REQUIRE(rvmStr.find("1:int") != std::string::npos);

        // Verify execution with different inputs
        auto runWith = [&](Integer val) -> Integer {
            RVMInterpreter interp;
            interp.registerExternalFunction("_Z6getInt_P",
                                            [val](const std::vector<ValueVariant>&) -> ValueVariant { return val; });
            auto result = interp.execute(rvmProgram, Type(TypeKind::Integer));
            REQUIRE(std::holds_alternative<Integer>(result));
            return std::get<Integer>(result);
        };

        REQUIRE(runWith(0) == 1);  // 0 % 2 == 0 → 1
        REQUIRE(runWith(1) == 0);  // 1 % 2 != 0 → 0
        REQUIRE(runWith(2) == 1);  // 2 % 2 == 0 → 1
        REQUIRE(runWith(3) == 0);  // 3 % 2 != 0 → 0
        REQUIRE(runWith(4) == 1);  // 4 % 2 == 0 → 1
    }
}

TEST_CASE("RVMMapper: phi node with full optimization pipeline", "[rvm][mapper][phi][optimizer]")
{
    SECTION("Simple if/else survives full pipeline")
    {
        const char* source = R"(
            @[extern, pure] fn getBool() -> bool;
            let x = if getBool() { 100 } else { 200 };
            x
        )";

        auto rvmProgram = compileToOptimizedRVM(source);

        // Verify execution after full optimization + register allocation
        REQUIRE(executeIntWithBool(rvmProgram, true) == 100);
        REQUIRE(executeIntWithBool(rvmProgram, false) == 200);
    }

    SECTION("Nested if/else survives full pipeline")
    {
        const char* source = R"(
            @[extern, pure] fn getBool() -> bool;
            let x = if getBool() {
                if getBool() { 1 } else { 2 }
            } else {
                if getBool() { 3 } else { 4 }
            };
            x
        )";

        auto rvmProgram = compileToOptimizedRVM(source);

        // getBool() always true → outer true, inner true → 1
        REQUIRE(executeIntWithBool(rvmProgram, true) == 1);
        // getBool() always false → outer false, inner false → 4
        REQUIRE(executeIntWithBool(rvmProgram, false) == 4);
    }

    SECTION("Mutable variable with conditional update survives full pipeline")
    {
        const char* source = R"(
            @[extern, pure] fn getBool() -> bool;
            let mut x = 5;
            if getBool() {
                x = 42;
            };
            x
        )";

        auto rvmProgram = compileToOptimizedRVM(source);

        REQUIRE(executeIntWithBool(rvmProgram, true) == 42);
        REQUIRE(executeIntWithBool(rvmProgram, false) == 5);
    }

    SECTION("Checkerboard-like pattern with force inline survives full pipeline")
    {
        const char* source = R"(
            @[extern, pure] fn getInt() -> int;
            fn isEven(n: int) = if n % 2 == 0 { 1 } else { 0 };
            isEven(getInt())
        )";

        auto opts                 = opt::OptimizerOptions::High();
        opts.ForceInlineFunctions = true;
        auto rvmProgram           = compileToOptimizedRVM(source, opts);

        auto runWith = [&](Integer val) -> Integer {
            RVMInterpreter interp;
            interp.registerExternalFunction("_Z6getInt_P",
                                            [val](const std::vector<ValueVariant>&) -> ValueVariant { return val; });
            auto result = interp.execute(rvmProgram, Type(TypeKind::Integer));
            REQUIRE(std::holds_alternative<Integer>(result));
            return std::get<Integer>(result);
        };

        REQUIRE(runWith(0) == 1);
        REQUIRE(runWith(1) == 0);
        REQUIRE(runWith(2) == 1);
        REQUIRE(runWith(7) == 0);
    }
}
