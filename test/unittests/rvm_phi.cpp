#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

#include "Environment.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMSerializer.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;
using namespace PExpr::ssa;

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

        // Verify RVM program has conditional branches for phi
        std::string rvmStr = RVMSerializer::serialize(rvmProgram);

        // Should have conditional branches (brz) for phi resolution
        REQUIRE(rvmStr.find("brz") != std::string::npos);
        // Should have labels for phi resolution
        REQUIRE((rvmStr.find("phi_end_") != std::string::npos || rvmStr.find("phi_next_") != std::string::npos));
    }

    SECTION("Phi with else branch")
    {
        const char* source = R"(
            [[extern, pure]] fn getBool() -> bool;
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

        // Verify RVM program is created
        REQUIRE(rvmProgram.size() > 0);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());
    }

    SECTION("Phi with single condition (no else)")
    {
        const char* source = R"(
            [[extern, pure]] fn getBool() -> bool;
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

        // Map to RVM
        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        // Verify RVM program is created
        REQUIRE(rvmProgram.size() > 0);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());

        // Should have conditional branch for the single condition
        REQUIRE(rvmStr.find("brz") != std::string::npos);
    }

    SECTION("Phi with multiple conditions")
    {
        const char* source = R"(
            [[extern, pure]] fn getBool() -> bool;
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

        // Map to RVM
        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        // Verify RVM program has multiple conditional branches
        std::string rvmStr = RVMSerializer::serialize(rvmProgram);

        // Count brz instructions (should be at least 3 for the 3 conditions)
        size_t brzCount = 0;
        size_t pos      = 0;
        while ((pos = rvmStr.find("brz", pos)) != std::string::npos) {
            brzCount++;
            pos += 3;
        }

        REQUIRE(brzCount >= 3);
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

        // Map to RVM
        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        // Verify RVM program handles constant condition
        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());

        // Should have true constant (or optimized away)
        REQUIRE((rvmStr.find("true") != std::string::npos || rvmStr.find("1:bool") != std::string::npos));
    }
}