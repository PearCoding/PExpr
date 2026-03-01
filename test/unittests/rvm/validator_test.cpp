#include "opt/OptimizerOptions.h"
#include "rvm/RVMMoveOptimizer.h"
#include "rvm/RVMOptimizer.h"
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
    SECTION("Redundant Move Elimination Verification")
    {
        const std::string source = R"(
mov %r0:int 10:int
mov %r1:int %r0:int
mov %r1:int 20:int
ret 2
)";
        auto original_opt        = RVMSerializer::deserialize(source);
        REQUIRE(original_opt.has_value());
        RVMProgram original  = *original_opt;
        RVMProgram optimized = original;

        REQUIRE(original.size() == 4);

        opt::OptimizerOptions opts;
        opts.OptimizeRedundantMoves = true;
        bool changed                = RVMMoveOptimizer::optimize(opts, optimized);
        REQUIRE(changed == true);

        REQUIRE(RVMValidator::validateOptimizations(original, optimized, Type(TypeKind::Integer)) == true);
    }

    SECTION("Move Chain Optimization Verification")
    {
        const std::string source = R"(
mov %r0:int 10:int
mov %r1:int %r0:int
mov %r2:int %r1:int
ret 3
)";
        auto original_opt        = RVMSerializer::deserialize(source);
        REQUIRE(original_opt.has_value());
        RVMProgram original  = *original_opt;
        RVMProgram optimized = original;

        REQUIRE(original.size() == 4);

        opt::OptimizerOptions opts;
        opts.OptimizeMoveChains = true;
        bool changed            = RVMMoveOptimizer::optimize(opts, optimized);
        REQUIRE(changed == false); // No change as all three registers are used as a return value

        REQUIRE(RVMValidator::validateOptimizations(original, optimized, Type(TypeKind::Integer)) == true);
    }

    SECTION("Identity Move Elimination Verification")
    {
        const std::string source = R"(
mov %r0:int 10:int
mov %r0:int %r0:int
ret 1
)";
        auto original_opt        = RVMSerializer::deserialize(source);
        REQUIRE(original_opt.has_value());
        RVMProgram original  = *original_opt;
        RVMProgram optimized = original;

        REQUIRE(original.size() == 3);

        opt::OptimizerOptions opts;
        opts.OptimizeIdentityMoves = true;
        bool changed               = RVMMoveOptimizer::optimize(opts, optimized);
        REQUIRE(changed == true);

        REQUIRE(RVMValidator::validateOptimizations(original, optimized, Type(TypeKind::Integer)) == true);
    }

    SECTION("Constant Propagation and getNumber Verification")
    {
        std::vector<Type> params     = { Type(TypeKind::String) };
        std::string getNumberMangled = makeMangledNameFromTypes("getNumber", params, nullptr);

        std::stringstream ss;
        ss << "load_string #str1:str \"42.0\"\n";
        ss << "mov %r0:str #str1:str\n";
        ss << "call_external 1 1 " << getNumberMangled << "\n";
        ss << "mov %r2:num %r0:num\n";
        ss << "add %r3:num %r2:num 10.0:num\n";
        ss << "ret 4\n";

        auto original_opt = RVMSerializer::deserialize(ss.str());
        REQUIRE(original_opt.has_value());
        RVMProgram original  = *original_opt;
        RVMProgram optimized = original;

        REQUIRE(original.size() == 6);

        bool changed = RVMOptimizer::optimize(opt::OptimizerOptions::Medium(), optimized);
        REQUIRE(changed == true);

        REQUIRE(RVMValidator::validateOptimizations(original, optimized, Type(TypeKind::Number)) == true);
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
        REQUIRE(changed == false); //< There is nothing we could optimize away

        REQUIRE(RVMValidator::validateOptimizations(original, optimized, Type(TypeKind::Integer)) == true);
    }
}
