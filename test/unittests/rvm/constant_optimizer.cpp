#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <sstream>
#include <vector>

#include "opt/OptimizerOptions.h"
#include "rvm/RVMConstantOptimizer.h"
#include "rvm/RVMOptimizer.h"
#include "rvm/RVMSerializer.h"
#include "rvm/RVMStructs.h"
#include "rvm/RVMValidator.h"
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

TEST_CASE("RVMConstantOptimizer: basic constant propagation", "[rvm][constant][optimization]")
{
    SECTION("Propagate integer constant")
    {
        std::string rvmIr = R"(
            mov %r4:int 42:int
            add %r1:int %r4:int %r2:int
            ret 3
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply constant propagation only
        opt::OptimizerOptions opts;
        opts.OptimizeConstantPropagation = true;
        bool changed                     = RVMConstantOptimizer::optimize(opts, program);

        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // The MOV should still be there (only propagation, no removal)
        // Move optimization is now part of register allocation
        REQUIRE(countMovInstructions(program) == 1);
    }

    SECTION("Propagate number constant")
    {
        std::string rvmIr = R"(
            mov %r4:num 3.14:num
            add %r1:num %r4:num %r2:num
            ret 3
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply constant propagation only
        opt::OptimizerOptions opts;
        opts.OptimizeConstantPropagation = true;
        bool changed                     = RVMConstantOptimizer::optimize(opts, program);

        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Number)));
    }

    SECTION("Propagate boolean constant")
    {
        std::string rvmIr = R"(
            mov %r4:bool true:bool
            and %r1:bool %r4:bool %r2:bool
            ret 3
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply constant propagation only
        opt::OptimizerOptions opts;
        opts.OptimizeConstantPropagation = true;
        bool changed                     = RVMConstantOptimizer::optimize(opts, program);

        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Boolean)));
    }

    SECTION("Multiple constant propagations")
    {
        std::string rvmIr = R"(
            mov %r4:int 42:int
            mov %r5:int 10:int
            add %r1:int %r4:int %r5:int
            ret 3
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply constant propagation only
        opt::OptimizerOptions opts;
        opts.OptimizeConstantPropagation = true;
        bool changed                     = RVMConstantOptimizer::optimize(opts, program);

        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
    }
}

TEST_CASE("RVMConstantOptimizer: basic block boundaries", "[rvm][constant][optimization][blocks]")
{
    SECTION("No propagation across label")
    {
        std::string rvmIr = R"(
            mov %r4:int 42:int
            .label1:
            add %r1:int %r4:int %r2:int
            ret 3
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply constant propagation only
        opt::OptimizerOptions opts;
        opts.OptimizeConstantPropagation = true;
        bool changed                     = RVMConstantOptimizer::optimize(opts, program);

        // Should NOT propagate because label starts new basic block
        REQUIRE(changed == false);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
    }

    SECTION("No propagation across jump")
    {
        std::string rvmIr = R"(
            mov %r4:int 42:int
            jmp .label1
            .label1:
            add %r1:int %r4:int %r2:int
            ret 3
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply constant propagation only
        opt::OptimizerOptions opts;
        opts.OptimizeConstantPropagation = true;
        bool changed                     = RVMConstantOptimizer::optimize(opts, program);

        // Should NOT propagate because jump separates blocks
        REQUIRE(changed == false);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
    }

    SECTION("Propagation within same block")
    {
        std::string rvmIr = R"(
            mov %r4:int 42:int
            add %r1:int %r4:int %r2:int
            sub %r3:int %r4:int %r1:int
            ret 4
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply constant propagation only
        opt::OptimizerOptions opts;
        opts.OptimizeConstantPropagation = true;
        bool changed                     = RVMConstantOptimizer::optimize(opts, program);

        // Should propagate to both ADD and SUB
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
    }
}

TEST_CASE("RVMConstantOptimizer: redefinition invalidates constant", "[rvm][constant][optimization]")
{
    SECTION("Redefinition of register")
    {
        std::string rvmIr = R"(
            mov %r4:int 42:int
            mov %r4:int 10:int
            add %r1:int %r4:int %r2:int
            ret 3
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply constant propagation only
        opt::OptimizerOptions opts;
        opts.OptimizeConstantPropagation = true;
        bool changed                     = RVMConstantOptimizer::optimize(opts, program);

        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
        // The constant 10 should be propagated, not 42
    }

    SECTION("Redefinition via arithmetic")
    {
        std::string rvmIr = R"(
            mov %r4:int 42:int
            add %r4:int %r1:int %r2:int
            add %r3:int %r4:int %r1:int
            ret 4
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply constant propagation only
        opt::OptimizerOptions opts;
        opts.OptimizeConstantPropagation = true;
        bool changed                     = RVMConstantOptimizer::optimize(opts, program);

        // No propagation should happen because r4 is overwritten before being read as a source
        REQUIRE(changed == false);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
        // The MOV sets r4 = 42, but then ADD overwrites r4 before any use of r4 as a source.
        // The second ADD uses r4 after it has been overwritten, so no constant propagation.
    }
}

TEST_CASE("RVMConstantOptimizer: no propagation for register source", "[rvm][constant][optimization]")
{
    SECTION("MOV with register source not propagated")
    {
        std::string rvmIr = R"(
            mov %r4:int %r0:int
            add %r1:int %r4:int %r2:int
            ret 3
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply constant propagation only
        opt::OptimizerOptions opts;
        opts.OptimizeConstantPropagation = true;
        bool changed                     = RVMConstantOptimizer::optimize(opts, program);

        // Should not change because source is register, not constant
        REQUIRE(changed == false);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
    }
}

TEST_CASE("RVMConstantOptimizer: full optimizer integration", "[rvm][constant][optimization][integration]")
{
    SECTION("Constant propagation with redundant move elimination")
    {
        std::string rvmIr = R"(
            mov %r4:int 42:int
            add %r0:int %r4:int %r2:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply full optimizer with constant propagation and redundant moves
        bool changed = RVMOptimizer::optimize(opt::OptimizerOptions::Medium(), program);

        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // MOV should be removed
        REQUIRE(countMovInstructions(program) == 0);

        // Only ADD and RET should remain
        REQUIRE(program.size() == 2);
    }

    SECTION("Constant propagation with all move optimizations")
    {
        std::string rvmIr = R"(
            mov %r4:int 42:int
            mov %r2:int 17:int
            mov %r5:int %r4:int
            add %r0:int %r5:int %r2:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply full optimizer with all optimizations
        opt::OptimizerOptions opts = opt::OptimizerOptions::Low();
        bool changed               = RVMOptimizer::optimize(opts, program);

        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // Both MOVs should be eliminated: first becomes redundant after propagation,
        // second is a chain that gets collapsed.
        REQUIRE(countMovInstructions(program) == 0);
    }
}