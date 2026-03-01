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
// Helper function to count MOV instructions in a program
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

// Helper to check if a MOV instruction is identity
bool isIdentityMov(const RVMInstr2Op* mov)
{
    auto dst  = mov->dst();
    auto srcs = mov->srcs();
    return dst.has_value() && srcs.size() == 1 && dst.value() == srcs[0];
}

RVMProgram deserializeSafe(const std::string& ir)
{
    auto prog_opt = RVMSerializer::deserialize(ir);
    REQUIRE(prog_opt.has_value());
    return *prog_opt;
}
} // namespace

TEST_CASE("RVMMoveOptimizer: control flow edge cases", "[rvm][move][optimization][control-flow]")
{
    SECTION("MOV across basic blocks with labels")
    {
        // Parse RVM IR using RVMSerializer
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            jz label1 %r1:bool
            mov %r2:int %r1:int
            jmp label2
        label1:
            mov %r3:int %r1:int
        label2:
            add %r0:int %r2:int %r3:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Apply all move optimizations
        opt::OptimizerOptions opts;
        opts.OptimizeIdentityMoves  = true;
        opts.OptimizeMoveChains     = true;
        opts.OptimizeRedundantMoves = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);

        // Verify optimization occurred
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // Count MOV instructions after optimization
        int movCount = countMovInstructions(program);

        // Should have reduced MOV count
        REQUIRE(movCount < 3); // Started with 3 MOVs
    }

    SECTION("Redundant MOV in different basic blocks")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            jz skip %r0:bool
            mov %r2:int %r1:int
            add %r3:int %r2:int %r0:int
            jmp end
        skip:
            mov %r2:int %r1:int  // Same MOV as above block, but in different block
        end:
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        opt::OptimizerOptions opts;
        opts.OptimizeRedundantMoves = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);

        // At least one MOV should be optimized
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
    }

    SECTION("MOV chain interrupted by control flow")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            jz middle %r0:bool
            mov %r2:int %r1:int
            jmp end
        middle:
            mov %r3:int %r1:int
            mov %r2:int %r3:int
        end:
            add %r0:int %r2:int %r0:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // Verify no identity MOVs remain
        for (const auto& instr : program) {
            if (auto* mov = dynamic_cast<RVMInstr2Op*>(instr.get())) {
                if (mov->opcode() == Opcode::MOV)
                    REQUIRE_FALSE(isIdentityMov(mov));
            }
        }
    }

    SECTION("MOV used in branch condition")
    {
        std::string rvmIr = R"(
            mov %r1:bool %r0:bool
            jz label %r1:bool
            mov %r2:int 42:int
            jmp end
        label:
            mov %r2:int 24:int
        end:
            ret 1
        )";

        RVMProgram program = deserializeSafe(rvmIr);

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE_FALSE(changed); //< Nothing should be optimized above

        // The MOV to r1 is final (used in branch), should not be removed
        // But identity MOV elimination might still apply if there are any
        std::string optimized = RVMSerializer::serialize(program);
        bool hasMovR1         = optimized.find("mov %r1:bool") != std::string::npos;
        bool hasJzR0          = optimized.find("jz label %r0:bool") != std::string::npos;
        REQUIRE((hasMovR1 || hasJzR0));
    }
}

TEST_CASE("RVMMoveOptimizer: type-specific optimizations", "[rvm][move][optimization][types]")
{
    SECTION("MOV with different types")
    {
        std::string rvmIr = R"(
            mov %r1:int 42:int
            mov %r2:num 3.14:num
            mov %r3:bool true:bool
            mov %r4:str #str0:str
            add %r0:int %r1:int 10:int
            ret 1
        )";

        RVMProgram program = deserializeSafe(rvmIr);

        opt::OptimizerOptions opts;
        opts.OptimizeIdentityMoves  = true;
        opts.OptimizeRedundantMoves = true;

        // Should not crash with type mismatches
        REQUIRE_NOTHROW(RVMMoveOptimizer::optimize(opts, program));
    }

    SECTION("Mixed-type move chains with conversions")
    {
        std::string rvmIr = R"(
            mov %r1:int 42:int
            i2f %r2:num %r1:int
            mov %r3:num %r2:num
            add %r4:num %r3:num 1.0:num
            ret 1
        )";

        RVMProgram program = deserializeSafe(rvmIr);

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        // MOV chain with i2f conversion in middle - should handle gracefully
        REQUIRE_NOTHROW(RVMMoveOptimizer::optimize(opts, program));

        // TODO: We could get rid of the 'mov' in line 3
    }
}

TEST_CASE("RVMMoveOptimizer: complex chain scenarios", "[rvm][move][optimization][complex-chains]")
{
    SECTION("Long move chains (5 registers)")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            mov %r2:int %r1:int
            mov %r3:int %r2:int
            mov %r4:int %r3:int
            mov %r5:int %r4:int
            add %r0:int %r5:int %r0:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // Count remaining MOV instructions
        int movCount = countMovInstructions(program);

        // Long chain should be collapsed significantly
        REQUIRE(movCount <= 2);
    }

    SECTION("Interleaved multiple move chains")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            mov %r2:num %r10:num
            mov %r3:int %r1:int
            mov %r4:num %r2:num
            mov %r5:int %r3:int
            mov %r6:num %r4:num
            add %r0:int %r5:int %r0:int
            add %r10:num %r6:num %r10:num
            ret 2
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
    }

    SECTION("Self-referential chain (cycle detection)")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            mov %r2:int %r1:int
            mov %r0:int %r2:int  // Creates cycle: r0 -> r1 -> r2 -> r0
            ret 1
        )";

        RVMProgram program = deserializeSafe(rvmIr);

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        // Should handle cycles without infinite loops
        REQUIRE_NOTHROW(RVMMoveOptimizer::optimize(opts, program));
    }

    SECTION("Move chain with pinned intermediate registers")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            mov %r2:int %r1:int
            add %r3:int %r2:int %r0:int  // Uses r2
            mov %r4:int %r2:int
            mov %r5:int %r4:int
            add %r0:int %r5:int %r0:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // r2 should still exist (used by ADD)
        std::string optimized = RVMSerializer::serialize(program);
        bool hasR2            = optimized.find("%r2:int") != std::string::npos;
        REQUIRE(hasR2 == true);
    }
}

TEST_CASE("RVMMoveOptimizer: constant propagation", "[rvm][move][optimization][constants]")
{
    SECTION("Identity MOV with constants")
    {
        std::string rvmIr = R"(
            mov %r2:int 42:int
            mov %r1:int %r2:int //<- Should be collapsed
            add %r0:int %r1:int 10:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        opt::OptimizerOptions opts;
        opts.OptimizeIdentityMoves = true;
        opts.OptimizeMoveChains    = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true); 
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
    }

    SECTION("Redundant moves involving constants")
    {
        std::string rvmIr = R"(
            mov %r1:int 42:int
            mov %r1:int 24:int  // Overwrites before use
            add %r0:int %r1:int 10:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        opt::OptimizerOptions opts;
        opts.OptimizeRedundantMoves = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // First MOV (42) should be removed
        std::string optimized = RVMSerializer::serialize(program);
        REQUIRE(optimized.find("24") != std::string::npos);
        REQUIRE(optimized.find("42") == std::string::npos);
    }

    SECTION("Constant propagation through move chains")
    {
        std::string rvmIr = R"(
            mov %r1:int 42:int
            mov %r2:int %r1:int
            mov %r3:int %r2:int
            add %r0:int %r3:int 10:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // Chain should be collapsed
        int movCount = countMovInstructions(program);
        REQUIRE(movCount <= 1); 
    }
}

TEST_CASE("RVMMoveOptimizer: optimization interactions", "[rvm][move][optimization][interactions]")
{
    SECTION("All optimizations enabled simultaneously")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            mov %r1:int %r1:int // Identity
            mov %r2:int %r1:int
            mov %r2:int %r3:int // Redundant (overwrites before use)
            add %r0:int %r2:int %r0:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        opt::OptimizerOptions opts;
        opts.OptimizeIdentityMoves  = true;
        opts.OptimizeMoveChains     = true;
        opts.OptimizeRedundantMoves = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // Count remaining MOVs
        int movCount = countMovInstructions(program);

        // Should have significantly reduced MOV count
        REQUIRE(movCount < 4);
    }

    SECTION("Conflicting optimization scenarios")
    {
        // MOV that is both part of chain and potentially redundant
        std::string rvmIr = R"(
            mov %r1:int %r0:int // Redundant - see below
            mov %r2:int %r1:int // Redundant - see below
            mov %r2:int %r3:int // Overwrites r2 with undefined %r3
            add %r0:int %r2:int %r0:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains     = true;
        opts.OptimizeRedundantMoves = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        CHECK(changed == true);
        CHECK(original.size() == 5);
        CHECK(program.size() == 3); // Dropped first two instructions
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
    }

    SECTION("Order-dependent optimization outcomes")
    {
        // Test that optimization order doesn't cause issues
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            mov %r1:int %r1:int // Identity
            mov %r2:int %r1:int
            mov %r2:int %r2:int // Identity
            add %r0:int %r2:int %r0:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        // Run optimization multiple times to check idempotence
        opt::OptimizerOptions opts;
        opts.OptimizeIdentityMoves  = true;
        opts.OptimizeMoveChains     = true;
        opts.OptimizeRedundantMoves = true;

        bool changed1 = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));
        bool changed2 = RVMMoveOptimizer::optimize(opts, program);

        // We should have optimized away the identities
        REQUIRE(changed1 == true);

        // Second run should not make changes (idempotence)
        REQUIRE(changed2 == false);
    }
}

TEST_CASE("RVMMoveOptimizer: edge conditions", "[rvm][move][optimization][edge]")
{
    SECTION("Empty program")
    {
        RVMProgram program;

        opt::OptimizerOptions opts;
        opts.OptimizeIdentityMoves = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == false);
        REQUIRE(program.empty() == true);
    }

    SECTION("Single instruction programs")
    {
        SECTION("Single MOV instruction")
        {
            std::string rvmIr  = "mov %r1:int %r0:int";
            RVMProgram program = deserializeSafe(rvmIr);

            opt::OptimizerOptions opts;
            opts.OptimizeIdentityMoves = true;
            opts.OptimizeMoveChains    = true;

            // Single MOV with no uses might be removed if redundant
            // Implementation-dependent
            REQUIRE_NOTHROW(RVMMoveOptimizer::optimize(opts, program));
        }

        SECTION("Single non-MOV instruction")
        {
            std::string rvmIr  = "add %r0:int %r1:int %r2:int";
            RVMProgram program = deserializeSafe(rvmIr);

            opt::OptimizerOptions opts;
            opts.OptimizeIdentityMoves = true;

            bool changed = RVMMoveOptimizer::optimize(opts, program);
            REQUIRE(changed == false);
        }
    }

    SECTION("Programs with only MOV instructions")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            mov %r2:int %r1:int
            mov %r3:int %r2:int
        )";

        RVMProgram program = deserializeSafe(rvmIr);

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains     = true;
        opts.OptimizeRedundantMoves = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true);

        // All MOVs might be removed if they're redundant
        // (no uses of destination registers)
    }

    SECTION("Maximum register count considerations")
    {
        // Test with many registers to ensure no limits are hit
        std::ostringstream oss;
        oss << "mov %r1:int %r0:int\n";
        for (int i = 1; i < 50; i++) {
            oss << "mov %r" << (i + 1) << ":int %r" << i << ":int\n";
        }
        oss << "add %r0:int %r50:int %r0:int\n";
        oss << "ret 1";

        RVMProgram program = deserializeSafe(oss.str());

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        // Should not crash or hit limits
        REQUIRE_NOTHROW(RVMMoveOptimizer::optimize(opts, program));
    }

    SECTION("MOV with string references")
    {
        std::string rvmIr = R"(
            load_string #str0:str "Hello"
            mov %r1:str #str0:str
            ret 1
        )";

        RVMProgram program = deserializeSafe(rvmIr);

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        // Should handle string references without issues
        REQUIRE_NOTHROW(RVMMoveOptimizer::optimize(opts, program));
    }
}

TEST_CASE("RVMMoveOptimizer: behavioral correctness", "[rvm][move][optimization][correctness]")
{
    SECTION("No identity MOVs remain after optimization")
    {
        std::string rvmIr = R"(
            mov %r1:int %r0:int
            mov %r2:int %r2:int  // Identity
            mov %r3:int %r1:int
            mov %r3:int %r3:int  // Identity
            add %r0:int %r3:int %r0:int
            ret 1
        )";

        RVMProgram program  = deserializeSafe(rvmIr);
        RVMProgram original = program;

        opt::OptimizerOptions opts;
        opts.OptimizeIdentityMoves = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // Verify no identity MOVs remain
        for (const auto& instr : program) {
            if (auto* mov = dynamic_cast<RVMInstr2Op*>(instr.get())) {
                if (mov->opcode() == Opcode::MOV)
                    REQUIRE_FALSE(isIdentityMov(mov));
            }
        }
    }

    SECTION("MOV count decreases after optimization")
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

        // Count MOVs before optimization
        int movCountBefore = countMovInstructions(program);

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(RVMValidator::validateOptimizations(original, program, Type(TypeKind::Integer)));

        // Count MOVs after optimization
        int movCountAfter = countMovInstructions(program);

        REQUIRE(changed == true);
        REQUIRE(movCountAfter < movCountBefore);
    }

    SECTION("Program semantics preserved (basic check)")
    {
        // Create a simple program, optimize it, and verify
        // that ADD still uses correct source
        std::string rvmIr = R"(
            mov %r1:int 42:int
            mov %r2:int %r1:int
            add %r0:int %r2:int 10:int
            ret 1
        )";

        RVMProgram program = deserializeSafe(rvmIr);

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;

        bool changed = RVMMoveOptimizer::optimize(opts, program);
        REQUIRE(changed == true);

        // After optimization, ADD should still compute 42 + 10 = 52
        // We can't easily execute, but we can check that ADD exists
        bool hasAdd = false;
        for (const auto& instr : program) {
            if (auto* add = dynamic_cast<RVMInstr3Op*>(instr.get())) {
                if (add->opcode() == Opcode::ADD) {
                    hasAdd = true;
                    break;
                }
            }
        }
        REQUIRE(hasAdd == true);
    }
}
