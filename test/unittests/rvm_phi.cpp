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

        // Verify RVM program is created (phi nodes are handled via backpropagation)
        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());
        // With backpropagation, phi nodes don't generate jz or phi_end_/phi_next_ labels
        // Instead, moves are inserted in branch blocks
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

        // With backpropagation, phi nodes generate MOV instructions in branch blocks
        // There should still be a conditional branch (jnz) for the if condition
        REQUIRE(rvmStr.find("jnz") != std::string::npos);
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

        // Count conditional branch instructions (should be at least 3 for the 3 conditions)
        // With backpropagation, phi nodes don't generate extra jz/jnz for phi resolution
        // Only the original conditional branches remain (jnz for if conditions)
        size_t jmpCount = 0;
        size_t pos      = 0;
        while ((pos = rvmStr.find("jnz", pos)) != std::string::npos) {
            jmpCount++;
            pos += 4;
        }

        REQUIRE(jmpCount >= 3);
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

    SECTION("Complex branch pattern with multiple variables")
    {
        const char* source = R"(
            [[extern, pure]] fn getBool() -> bool;
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

        // Map to RVM
        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());

        // Should have phi nodes for x, y, and z with backpropagated MOVs
        REQUIRE(rvmStr.find("add") != std::string::npos);
    }

    SECTION("Phi with same value in multiple branches")
    {
        const char* source = R"(
            [[extern, pure]] fn getBool() -> bool;
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

        // Map to RVM
        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());

        // Should handle duplicate values correctly (produces redundant MOVs that get optimized later)
        REQUIRE(rvmStr.find("42") != std::string::npos);
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

        // Map to RVM
        auto mapper     = RVMMapper();
        auto rvmProgram = mapper.mapProgram(ssaProgram);

        std::string rvmStr = RVMSerializer::serialize(rvmProgram);
        REQUIRE(!rvmStr.empty());

        // Should have phi nodes for both x and y (even though only one updated per branch)
        REQUIRE(rvmStr.find("add") != std::string::npos);
    }
}
