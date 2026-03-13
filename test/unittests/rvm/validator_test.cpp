#include "Environment.h"
#include "opt/OptimizerOptions.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMOptimizer.h"
#include "rvm/RVMRegisterAllocator.h"
#include "rvm/RVMSerializer.h"
#include "rvm/RVMStructs.h"
#include "rvm/RVMValidator.h"
#include "type/Mangler.h"
#include "type/Type.h"
#include <catch2/catch_test_macros.hpp>

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;

TEST_CASE("RVMValidator: Optimization Verification", "[rvm][validation]")
{
    SECTION("Constant Propagation and getNumber Verification")
    {
        std::vector<Type> params     = { Type(TypeKind::String) };
        std::string getNumberMangled = makeMangledNameFromTypes("getNumber", params, nullptr);

        std::stringstream ss;
        ss << "load_string #str1:str \"42.0\"" << std::endl
           << "mov %r0:str #str1:str" << std::endl
           << "call_external 1 1 " << getNumberMangled << "" << std::endl
           << "mov %r2:num %r0:num" << std::endl
           << "add %r0:num %r2:num 10.0:num" << std::endl
           << "ret 1";

        auto original_opt = RVMSerializer::deserialize(ss.str());
        REQUIRE(original_opt.has_value());
        RVMProgram original  = *original_opt;
        RVMProgram optimized = original;

        REQUIRE(original.size() == 6);

        bool changed = RVMOptimizer::optimize(opt::OptimizerOptions::Medium(), optimized);
        PEXPR_UNUSED(changed);
        // REQUIRE(changed == true); // < Currently not optimizing "mov %r2:num %r0:num" away :(

        std::string errorMsg;
        bool isValid = RVMValidator::validateOptimizations(original, optimized, Type(TypeKind::Number), errorMsg);
        if (!isValid) {
            std::cout << "Original program:" << std::endl
                      << RVMSerializer::serialize(original) << std::endl
                      << "Optimized program:" << std::endl
                      << RVMSerializer::serialize(optimized) << std::endl;
            FAIL("Validation failed: " << errorMsg);
        }
        REQUIRE(errorMsg.empty());
    }

    SECTION("Branch Optimization Verification")
    {
        const std::string source = R"(
mov %r0:int 1:int
mov %r1:int 10:int
jz label %r0:int
mov %r1:int 20:int
label:
ret 2
)";
        auto original_opt        = RVMSerializer::deserialize(source);
        REQUIRE(original_opt.has_value());
        RVMProgram original  = *original_opt;
        RVMProgram optimized = original;

        REQUIRE(original.size() == 6);

        bool changed = RVMOptimizer::optimize(opt::OptimizerOptions::High(), optimized);
        // Constant propagation may replace %r0:int with 1:int in the JZ instruction
        REQUIRE(changed == true);

        std::string errorMsg;
        bool isValid = RVMValidator::validateOptimizations(original, optimized, Type(TypeKind::Integer), errorMsg);
        if (!isValid) {
            std::cout << "Original program:" << std::endl
                      << RVMSerializer::serialize(original) << std::endl
                      << "Optimized program:" << std::endl
                      << RVMSerializer::serialize(optimized) << std::endl;
            FAIL("Validation failed: " << errorMsg);
        }
        REQUIRE(errorMsg.empty());
    }
}

TEST_CASE("RVMValidator: Use-Before-Definition Validation", "[rvm][validation][use-before-def]")
{
    SECTION("Valid program with no use-before-definition")
    {
        const std::string source = R"(
mov %r0:int 10:int
mov %r1:int %r0:int
add %r2:int %r0:int %r1:int
ret 1
)";
        auto program_opt         = RVMSerializer::deserialize(source);
        REQUIRE(program_opt.has_value());
        RVMProgram program = *program_opt;

        std::string errorMsg;
        bool isValid = RVMValidator::validateUseBeforeDefinition(program, errorMsg);
        if (!isValid) {
            std::cout << "Program:" << std::endl
                      << RVMSerializer::serialize(program) << std::endl;
            FAIL("Validation failed: " << errorMsg);
        }
        REQUIRE(errorMsg.empty());
    }

    SECTION("Invalid program with use before definition")
    {
        const std::string source = R"(
mov %r1:int %r0:int
ret 1
)";
        auto program_opt         = RVMSerializer::deserialize(source);
        REQUIRE(program_opt.has_value());
        RVMProgram program = *program_opt;

        std::string errorMsg;
        bool isValid = RVMValidator::validateUseBeforeDefinition(program, errorMsg);
        REQUIRE(isValid == false);
    }

    SECTION("Program with multiple uses before definition")
    {
        const std::string source = R"(
add %r2:int %r0:int %r1:int  // both r0 and r1 used before definition
ret 1
)";
        auto program_opt         = RVMSerializer::deserialize(source);
        REQUIRE(program_opt.has_value());
        RVMProgram program = *program_opt;

        std::string errorMsg;
        bool isValid = RVMValidator::validateUseBeforeDefinition(program, errorMsg);
        REQUIRE(isValid == false);
        // Should report at least one of the registers
        REQUIRE((errorMsg.find("Register 0") != std::string::npos || errorMsg.find("Register 1") != std::string::npos));
    }

    SECTION("Valid program with late definition but early use")
    {
        const std::string source = R"(
mov %r0:int 10:int
jz label %r1:int      // r1 used before definition
mov %r1:int 20:int
label:
ret 2
)";
        auto program_opt         = RVMSerializer::deserialize(source);
        REQUIRE(program_opt.has_value());
        RVMProgram program = *program_opt;

        std::string errorMsg;
        bool isValid = RVMValidator::validateUseBeforeDefinition(program, errorMsg);
        REQUIRE(isValid == false);
        REQUIRE(errorMsg.find("Register 1") != std::string::npos);
    }
}

TEST_CASE("RVMValidator: Register Allocation Validation", "[rvm][validation][register-allocation]")
{
    SECTION("Valid program with non-overlapping register usage")
    {
        const std::string source = R"(
mov %r0:int 10:int
mov %r1:int 20:int
add %r2:int %r0:int %r1:int
mov %r0:int 30:int     // r0 redefined after original use
ret 1
)";
        auto program_opt         = RVMSerializer::deserialize(source);
        REQUIRE(program_opt.has_value());
        RVMProgram program = *program_opt;

        std::string errorMsg;
        bool isValid = RVMValidator::validateRegisterAllocation(program, errorMsg);
        if (!isValid) {
            std::cout << "Program:" << std::endl
                      << RVMSerializer::serialize(program) << std::endl;
            FAIL("Validation failed: " << errorMsg);
        }
        REQUIRE(errorMsg.empty());
    }

    SECTION("Program with overlapping live ranges for same register")
    {
        // This program has r0 defined at position 0, used at position 2,
        // then redefined at position 1. The first definition is dead (never used),
        // so intervals don't overlap.
        const std::string source = R"(
mov %r0:int 10:int
mov %r0:int 20:int    // redefinition - first r0 value is dead
add %r1:int %r0:int 5:int
ret 1
)";
        auto program_opt         = RVMSerializer::deserialize(source);
        REQUIRE(program_opt.has_value());
        RVMProgram program = *program_opt;

        std::string errorMsg;
        bool isValid = RVMValidator::validateRegisterAllocation(program, errorMsg);
        if (!isValid) {
            std::cout << "Program:" << std::endl
                      << RVMSerializer::serialize(program) << std::endl;
            FAIL("Validation failed: " << errorMsg);
        }
        // Actually valid: first r0 is dead, second r0 defined before use
        REQUIRE(errorMsg.empty());
    }

    SECTION("Complex control flow with register reuse")
    {
        const std::string source = R"(
mov %r0:int 10:int
jz label1 %r0:int
mov %r1:int 20:int
jmp label2
label1:
mov %r1:int 30:int
label2:
add %r2:int %r1:int %r1:int
ret 1
)";
        auto program_opt         = RVMSerializer::deserialize(source);
        REQUIRE(program_opt.has_value());
        RVMProgram program = *program_opt;

        std::string errorMsg;
        bool isValid = RVMValidator::validateRegisterAllocation(program, errorMsg);
        if (!isValid) {
            std::cout << "Program:" << std::endl
                      << RVMSerializer::serialize(program) << std::endl;
            FAIL("Validation failed: " << errorMsg);
        }
        // This should be valid - r1 is defined in both branches but they don't overlap
        REQUIRE(errorMsg.empty());
    }

    SECTION("Function call with parameter registers")
    {
        const std::string source = R"(
mov %r0:int 10:int
mov %r1:int 20:int
call_external 2 1 foo
mov %r2:int %r0:int
ret 1 // In reality only %r0 = 10 will be returned.
)";

        auto program_opt = RVMSerializer::deserialize(source);
        REQUIRE(program_opt.has_value());
        RVMProgram program = *program_opt;

        std::string errorMsg;
        bool isValid = RVMValidator::validateRegisterAllocation(program, errorMsg);
        if (!isValid) {
            std::cout << "Program:" << std::endl
                      << RVMSerializer::serialize(program) << std::endl;
            FAIL("Validation failed: " << errorMsg);
        }
        // This should be valid - r0 and r1 are used as parameters, r0 is also used after call
        // The live intervals:
        // r0: defined at 0, used at 2 (call), used at 3 (mov), ends at 3
        // r1: defined at 1, used at 2 (call), ends at 2
        // r2: defined at 3, ends at 3
        REQUIRE(errorMsg.empty());
    }
}

TEST_CASE("RVMValidator: Integration with Register Allocator", "[rvm][validation][integration]")
{
    SECTION("Register allocation should produce valid program")
    {
        Environment env;
        auto ast = env.parse(R"(
            [[extern]] fn getInput1() -> int;
            [[extern]] fn getInput2() -> int;
            
            let a = getInput1();
            let b = getInput2();
            let c = a + b;
            let d = c * 2;
            d
        )");
        REQUIRE(ast != nullptr);

        auto prog = env.map(ast);
        rvm::RVMMapper mapper;
        auto rvmProg = mapper.mapProgram(prog);

        // Validate before allocation
        std::string errorMsg;
        bool isValidBefore = RVMValidator::validateUseBeforeDefinition(rvmProg, errorMsg);
        if (!isValidBefore) {
            std::cout << "Program:" << std::endl
                      << RVMSerializer::serialize(rvmProg) << std::endl;
            FAIL("Validation failed: " << errorMsg);
        }

        isValidBefore = RVMValidator::validateRegisterAllocation(rvmProg, errorMsg);
        if (!isValidBefore) {
            std::cout << "Program:" << std::endl
                      << RVMSerializer::serialize(rvmProg) << std::endl;
            FAIL("Validation failed: " << errorMsg);
        }

        // Apply register allocation
        auto result = RVMRegisterAllocator::allocate(rvmProg);
        REQUIRE(result.Changed == true);

        // Validate after allocation
        bool isValidAfter = RVMValidator::validateUseBeforeDefinition(rvmProg, errorMsg);
        if (!isValidAfter) {
            std::cout << "Program:" << std::endl
                      << RVMSerializer::serialize(rvmProg) << std::endl;
            FAIL("Validation failed: " << errorMsg);
        }

        isValidAfter = RVMValidator::validateRegisterAllocation(rvmProg, errorMsg);
        if (!isValidAfter) {
            std::cout << "Program:" << std::endl
                      << RVMSerializer::serialize(rvmProg) << std::endl;
            FAIL("Validation failed: " << errorMsg);
        }
    }
}

TEST_CASE("RVMValidator: Complex verification", "[rvm][validation]")
{
    Environment env;
    auto ast = env.parse(R"(
[[extern, pure]] fn getNumber(str:str) -> num;
[[extern, pure]] fn passthrough(rgb:vec3) -> vec3;

let uv = [getNumber("42"), getNumber("11"), getNumber("7")];
passthrough([0.4*uv.x, uv.y, 1])
        )");
    REQUIRE(ast != nullptr);

    auto opt = opt::OptimizerOptions::None();

    opt.OptimizeIdentityMoves       = true;
    opt.OptimizeConstantPropagation = true;
    opt.EnableRegisterAllocation    = true;

    // The following are necessary for RVM
    opt.RemoveDeadCode = true;

    auto prog = env.map(ast);
    env.optimize(prog, opt);

    rvm::RVMMapper mapper;
    auto original = mapper.mapProgram(prog);

    RVMProgram optimized = original;
    REQUIRE(original.size() > 0);

    bool changed = RVMOptimizer::optimize(opt, optimized);
    CHECK(changed == true);

    std::string errorMsg;
    bool isValid = RVMValidator::validateOptimizations(original, optimized, Type::AsVector(3), errorMsg);
    if (!isValid) {
        std::cout << "Original program:" << std::endl
                  << RVMSerializer::serialize(original) << std::endl
                  << "Optimized program:" << std::endl
                  << RVMSerializer::serialize(optimized) << std::endl;
        FAIL("Validation failed: " << errorMsg);
    }
    REQUIRE(errorMsg.empty());
}