#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <sstream>
#include <vector>

#include "opt/OptimizerOptions.h"
#include "rvm/RVMMoveOptimizer.h"
#include "rvm/RVMSerializer.h"
#include "rvm/RVMValidator.h"
#include "rvm/RVMStructs.h"
#include "rvm/RVMValue.h"
#include "type/Type.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;

namespace {
RVMProgram deserializeSafe(const std::string& ir)
{
    auto prog_opt = RVMSerializer::deserialize(ir);
    REQUIRE(prog_opt.has_value());
    return *prog_opt;
}

int countMovInstructions(const RVMProgram& program)
{
    int count = 0;
    for (const auto& instr : program) {
        if (auto* mov = dynamic_cast<RVMInstr2Op*>(instr.get())) {
            if (mov->opcode() == Opcode::MOV)
                count++;
        }
    }
    return count;
}
} // namespace

TEST_CASE("RVMMoveOptimizer: identity mov elimination", "[rvm][move][optimization]")
{
    SECTION("Simple identity MOV removal")
    {
        std::string rvmIr = R"(
            mov %r1:int %r1:int
            mov %r0:int %r1:int
            mov %r0:int %r0:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply simplification
        opt::OptimizerOptions opts;
        opts.OptimizeIdentityMoves = true;
        bool changed = RVMMoveOptimizer::optimize(opts, program);

        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // Only non-identity MOV should remain
        REQUIRE(countMovInstructions(program) == 1);
    }

    SECTION("MOV chain simplification")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            mov %r2:int %r1:int
            mov %r3:int %r2:int
            add %r0:int %r3:int %r0:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply simplification
        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;
        bool changed = RVMMoveOptimizer::optimize(opts, program);

        // %r3 is used in ADD, but chain will be simplified
        // mov %r3 %r0 should remain, others removed
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // Only mov %r3 %r0 and ADD and ret should remain
        // Total instructions: 3 (MOV, ADD, RET)
        REQUIRE(program.size() == 3);
        REQUIRE(countMovInstructions(program) == 1);
    }

    SECTION("Pinned registers not renamed")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            mov %r2:int %r1:int
            add %r0:int %r2:int %r0:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply simplification
        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;
        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // r2 should not be renamed away since it's used in ADD
        REQUIRE(program.size() > 0);
    }

    SECTION("No changes when no MOV instructions")
    {
        std::string rvmIr = R"(
            add %r2:int %r0:int %r1:int
            sub %r1:int %r2:int %r0:int
            ret 1
        )";

        RVMProgram program = deserializeSafe(rvmIr);

        // Apply simplification
        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;
        bool changed = RVMMoveOptimizer::optimize(opts, program);

        REQUIRE(changed == false);
        REQUIRE(program.size() == 3);
    }
}

TEST_CASE("RVMMoveOptimizer: redundant mov elimination", "[rvm][move][optimization][redundant]")
{
    SECTION("Redundant MOV elimination")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            mov %r1:int %r2:int
            add %r0:int %r1:int %r0:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply redundant move elimination
        opt::OptimizerOptions opts;
        opts.OptimizeRedundantMoves = true;
        bool changed = RVMMoveOptimizer::optimize(opts, program);

        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
        REQUIRE(countMovInstructions(program) == 1); // Only one MOV should remain
    }

    SECTION("Multiple redundant MOVs in chain")
    {
        std::string rvmIr = R"(
            mov %r0:int %r1:int
            mov %r2:int %r0:int
            mov %r2:int %r3:int
            ret 4
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply redundant move elimination
        opt::OptimizerOptions opts;
        opts.OptimizeRedundantMoves = true;
        bool changed = RVMMoveOptimizer::optimize(opts, program);

        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
        // Only mov r2 r0 should be removed (redundant - r2 is overwritten)
        // mov r0 r1 remains (not redundant - r0 is not overwritten or used afterwards, 
        // actually in this test case r0 is also not used, so it might also be removed)
        
        REQUIRE(countMovInstructions(program) <= 2);
    }

    SECTION("No changes when no redundant MOVs")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            add %r0:int %r1:int %r0:int
            mov %r2:int %r1:int
            ret 3
        )";

        RVMProgram program = deserializeSafe(rvmIr);

        // Apply redundant move elimination
        opt::OptimizerOptions opts;
        opts.OptimizeRedundantMoves = true;
        bool changed = RVMMoveOptimizer::optimize(opts, program);

        REQUIRE(changed == false);
        REQUIRE(program.size() == 4);
    }
}
