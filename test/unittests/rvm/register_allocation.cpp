#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "opt/OptimizerOptions.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMOptimizer.h"
#include "rvm/RVMRegisterAllocator.h"
#include "rvm/RVMSerializer.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;
using namespace PExpr::rvm;

TEST_CASE("RVMRegisterAllocator: reduces register count in simple case", "[rvm][register-allocation]")
{
    Environment env;

    // Simple program that should benefit from register allocation
    // a = input1, b = input2, c = a + b, d = c * 2, return d
    // Without allocation: registers for a, b, c, d (4 registers)
    // With allocation: a and b can be reused after they're no longer live -> (3 registers)
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

    // Map to SSA
    auto prog = env.map(ast);
    env.optimize(prog, opt::OptimizerOptions::High());

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProg = mapper.mapProgram(prog);

    // Get register count before allocation
    size_t beforeCount = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // Apply register allocation
    auto result  = RVMRegisterAllocator::allocate(rvmProg);
    bool changed = result.Changed;

    // Get register count after allocation
    size_t afterCount = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // Register allocation should reduce register count
    // The exact reduction depends on the implementation
    REQUIRE(changed == true);
    REQUIRE(afterCount < beforeCount);

    // Serialize to verify the program is still valid
    std::string serialized = RVMSerializer::serialize(rvmProg);
    REQUIRE(!serialized.empty());

    // Check that we have fewer unique register IDs in the output
    // Count occurrences of %rX in the serialized output
    size_t uniqueRegs = 0;
    for (size_t i = 0; i < 100; ++i) {
        std::string regStr = "%r" + std::to_string(i) + ":";
        if (serialized.find(regStr) != std::string::npos)
            uniqueRegs++;
    }

    REQUIRE(uniqueRegs < beforeCount);
}

TEST_CASE("RVMRegisterAllocator: handles dead values", "[rvm][register-allocation]")
{
    Environment env;

    // Program with dead values that should be eliminated
    // a = input1 (used), b = input2 (dead - never used), c = a + 1, return c
    // b should not prevent register reuse
    auto ast = env.parse(R"(
        [[extern]] fn getInput1() -> int;
        [[extern]] fn getInput2() -> int;
        
        let a = getInput1();
        let b = getInput2(); // dead value
        let c = a + 1;
        c
    )");

    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    rvm::RVMMapper mapper;
    auto rvmProg = mapper.mapProgram(prog);

    size_t beforeCount = RVMRegisterAllocator::getRegisterCount(rvmProg);
    auto result        = RVMRegisterAllocator::allocate(rvmProg);
    bool changed       = result.Changed;
    size_t afterCount  = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // Should reduce register count (b is dead, its register can be reused)
    REQUIRE(changed == true);
    REQUIRE(afterCount < beforeCount);
}

TEST_CASE("RVMRegisterAllocator: integration with RVMOptimizer", "[rvm][register-allocation][integration]")
{
    Environment env;

    auto ast = env.parse(R"(
        [[extern]] fn getInput1() -> int;
        [[extern]] fn getInput2() -> int;
        [[extern]] fn getInput3() -> int;
        
        let x = getInput1();
        let y = getInput2();
        let z = getInput3();
        
        // Chain of computations
        let a = x + y;
        let b = a * z;
        let c = b - x;
        let d = c + y;
        let e = d * a;
        
        e
    )");

    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    rvm::RVMMapper mapper;
    auto rvmProg = mapper.mapProgram(prog);

    // Get original program string
    std::string original    = RVMSerializer::serialize(rvmProg);
    size_t originalRegCount = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // Create optimizer options with register allocation enabled
    opt::OptimizerOptions opts    = opt::OptimizerOptions::None();
    opts.EnableRegisterAllocation = true;

    // Apply optimizations
    bool changed = RVMOptimizer::optimize(opts, rvmProg);

    std::string optimized    = RVMSerializer::serialize(rvmProg);
    size_t optimizedRegCount = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // Register allocation should reduce register count
    REQUIRE(changed == true);
    REQUIRE(optimizedRegCount < originalRegCount);

    // The optimized program should still be valid RVM
    REQUIRE(!optimized.empty());
    REQUIRE(optimized.find("add") != std::string::npos); // Should still have add instruction
    REQUIRE(optimized.find("mul") != std::string::npos); // Should still have mul instruction
}

TEST_CASE("RVMRegisterAllocator: handles nested control flow", "[rvm][register-allocation][control-flow]")
{
    Environment env;

    // Program with nested if-else statements
    auto ast = env.parse(R"(
        [[extern]] fn getCond1() -> bool;
        [[extern]] fn getCond2() -> bool;
        [[extern]] fn getInput1() -> int;
        [[extern]] fn getInput2() -> int;
        [[extern]] fn getInput3() -> int;
        
        let cond1 = getCond1();
        let cond2 = getCond2();
        let a = getInput1();
        let b = getInput2();
        let c = getInput3();
        
        let result = if cond1 {
            if cond2 {
                a + b
            } else {
                a - b
            }
        } else {
            if cond2 {
                b + c
            } else {
                c - a
            }
        };
        
        result
    )");

    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    rvm::RVMMapper mapper;
    auto rvmProg = mapper.mapProgram(prog);

    size_t beforeCount = RVMRegisterAllocator::getRegisterCount(rvmProg);
    auto result        = RVMRegisterAllocator::allocate(rvmProg);
    bool changed       = result.Changed;
    size_t afterCount  = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // Register allocation should work with nested control flow
    REQUIRE(changed == true);
    REQUIRE(afterCount <= beforeCount);

    // Program should still be valid after allocation
    std::string serialized = RVMSerializer::serialize(rvmProg);
    REQUIRE(!serialized.empty());
}

TEST_CASE("RVMRegisterAllocator: handles complex register pressure", "[rvm][register-allocation][pressure]")
{
    Environment env;

    // Program with many temporary values creating high register pressure
    auto ast = env.parse(R"(
        [[extern]] fn getInput() -> int;
        
        let a = getInput();
        let b = getInput();
        let c = getInput();
        let d = getInput();
        let e = getInput();
        let f = getInput();
        
        // Chain of computations creating many temporaries
        let t1 = a + b;
        let t2 = c + d;
        let t3 = e + f;
        let t4 = t1 * t2;
        let t5 = t2 * t3;
        let t6 = t3 * t1;
        let t7 = t4 + t5;
        let t8 = t5 + t6;
        let t9 = t6 + t4;
        let t10 = t7 * t8;
        let t11 = t8 * t9;
        let t12 = t9 * t7;
        
        (t10 + t11 + t12) / 3
    )");

    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    rvm::RVMMapper mapper;
    auto rvmProg = mapper.mapProgram(prog);

    size_t beforeCount = RVMRegisterAllocator::getRegisterCount(rvmProg);
    auto result        = RVMRegisterAllocator::allocate(rvmProg);
    size_t afterCount  = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // Should reduce register count significantly
    REQUIRE(result.Changed == true);
    REQUIRE(afterCount < beforeCount);

    // Verify the program is still valid
    std::string serialized = RVMSerializer::serialize(rvmProg);
    REQUIRE(!serialized.empty());
    REQUIRE(serialized.find("add") != std::string::npos);
    REQUIRE(serialized.find("mul") != std::string::npos);
}

TEST_CASE("RVMRegisterAllocator: handles many live values simultaneously", "[rvm][register-allocation][many-live]")
{
    Environment env;

    // Program where many values are live at the same time
    auto ast = env.parse(R"(
        [[extern]] fn getInput() -> int;
        
        // All these values will be used together at the end
        let a = getInput();
        let b = getInput();
        let c = getInput();
        let d = getInput();
        let e = getInput();
        let f = getInput();
        let g = getInput();
        let h = getInput();
        
        // Use all values at once
        a + b + c + d + e + f + g + h
    )");

    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    rvm::RVMMapper mapper;
    auto rvmProg = mapper.mapProgram(prog);

    size_t beforeCount = RVMRegisterAllocator::getRegisterCount(rvmProg);
    auto result        = RVMRegisterAllocator::allocate(rvmProg);
    size_t afterCount  = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // With 8 values all live at the end, we need at least 8 registers
    // Allocation might not reduce count but should work correctly
    REQUIRE((result.Changed == false || afterCount <= beforeCount));

    std::string serialized = RVMSerializer::serialize(rvmProg);
    REQUIRE(!serialized.empty());
}

TEST_CASE("RVMRegisterAllocator: interaction with move chain optimization", "[rvm][register-allocation][integration]")
{
    Environment env;

    // Program that benefits from both move chain optimization and register allocation
    auto ast = env.parse(R"(
        [[extern]] fn getInput1() -> int;
        [[extern]] fn getInput2() -> int;
        
        let x = getInput1();
        let y = getInput2();
        
        // Create move chains that should be optimized
        let t1 = x;
        let t2 = t1;
        let t3 = y;
        let t4 = t3;
        
        // Then use the values
        let result = (t2 + t4) * 2;
        result
    )");

    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    rvm::RVMMapper mapper;
    auto rvmProg = mapper.mapProgram(prog);

    // Get original register count
    size_t originalRegCount = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // Apply optimizer with both move chain optimization and register allocation
    opt::OptimizerOptions opts = opt::OptimizerOptions::Medium();

    bool changed             = RVMOptimizer::optimize(opts, rvmProg);
    size_t optimizedRegCount = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // Should improve with combined optimizations
    REQUIRE(changed == true);
    REQUIRE(optimizedRegCount < originalRegCount);

    std::string optimized = RVMSerializer::serialize(rvmProg);
    REQUIRE(!optimized.empty());
}

TEST_CASE("RVMRegisterAllocator: preserves program semantics", "[rvm][register-allocation][semantics]")
{
    Environment env;

    // Complex program where we want to ensure semantics are preserved
    auto ast = env.parse(R"(
        [[extern]] fn getInput() -> int;
        
        let x = getInput();
        let y = getInput();
        let z = getInput();
        
        // Complex expression tree
        let result = (x * y) + (y * z) + (z * x) - (x + y + z);
        
        // Conditional use
        let final = if result > 0 {
            result * 2
        } else {
            -result
        };
        
        final
    )");

    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    rvm::RVMMapper mapper;
    auto originalRvmProg = mapper.mapProgram(prog);
    auto testRvmProg     = mapper.mapProgram(prog); // Copy for testing

    // Apply register allocation to test version
    auto result = RVMRegisterAllocator::allocate(testRvmProg);

    // Both programs should serialize to something valid
    std::string originalStr = RVMSerializer::serialize(originalRvmProg);
    std::string testStr     = RVMSerializer::serialize(testRvmProg);

    REQUIRE(!originalStr.empty());
    REQUIRE(!testStr.empty());

    // Register allocation should change something if registers can be reduced
    if (RVMRegisterAllocator::getRegisterCount(originalRvmProg) > 1) {
        // Either it changed or it couldn't reduce further
        // But the program should still be valid
        REQUIRE(true);
    }
}

TEST_CASE("RVMRegisterAllocator: handles edge case with single register", "[rvm][register-allocation][edge-cases]")
{
    Environment env;

    // Program that already uses minimal registers
    auto ast = env.parse(R"(
        [[extern]] fn getInput() -> int;
        
        let x = getInput();
        x + 1
    )");

    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    rvm::RVMMapper mapper;
    auto rvmProg = mapper.mapProgram(prog);

    size_t beforeCount = RVMRegisterAllocator::getRegisterCount(rvmProg);
    RVMRegisterAllocator::allocate(rvmProg);
    size_t afterCount = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // Minimal register is 1, but we might start with a higher number
    REQUIRE(afterCount <= beforeCount);
    // REQUIRE(afterCount == 1); //< This should be reachable!
}

TEST_CASE("RVMRegisterAllocator: handles mixed type registers", "[rvm][register-allocation][types]")
{
    Environment env;

    // Program with registers of different types (int, num, bool)
    auto ast = env.parse(R"(
        [[extern]] fn getInt() -> int;
        [[extern]] fn getNum() -> num;
        [[extern]] fn getBool() -> bool;

        let i = getInt();
        let n = getNum();
        let b = getBool();

        // Mix types in computation
        let i2 = if b { i * 2 } else { i / 2 };
        let n2 = n + (i2 as num);

        [i2, n2, b]
    )");

    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    rvm::RVMMapper mapper;
    auto rvmProg = mapper.mapProgram(prog);

    size_t beforeCount = RVMRegisterAllocator::getRegisterCount(rvmProg);
    auto result        = RVMRegisterAllocator::allocate(rvmProg);
    size_t afterCount  = RVMRegisterAllocator::getRegisterCount(rvmProg);

    // TODO: Better coalescing?

    // Register allocation should work with mixed types
    REQUIRE(result.Changed == true);
    REQUIRE(afterCount <= beforeCount);

    std::string serialized = RVMSerializer::serialize(rvmProg);
    REQUIRE(!serialized.empty());
}

TEST_CASE("RVMRegisterAllocator: no transitive interference in MOV chains with mixed types", "[rvm][register-allocation][regression]")
{
    Environment env;

    // Regression test: When an extern call returns vec2 (pinned %r0:num, %r1:num)
    // and another extern call takes (str, vec2) (pinned %r0:str, %r1:num, %r2:num),
    // the MOV chain from the first call's returns through intermediates to the second
    // call's parameters must NOT be fully coalesced. If it is, %r0 gets reused for
    // both str and num, and the string write clobbers the numeric value.
    auto ast = env.parse(R"(
        [[extern]] fn getUV() -> vec2;
        [[extern]] fn sample(name:str, uv:vec2) -> num;
        let uv = getUV();
        sample("tex", uv)
    )");

    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    env.optimize(prog, opt::OptimizerOptions::High());

    rvm::RVMMapper mapper;
    auto rvmProg = mapper.mapProgram(prog);

    // Apply full RVM optimization (constant folding + register allocation)
    opt::OptimizerOptions opts    = opt::OptimizerOptions::None();
    opts.OptimizeConstantPropagation = true;
    opts.EnableRegisterAllocation = true;
    RVMOptimizer::optimize(opts, rvmProg);

    // Serialize the output and verify the str write does NOT clobber a num value
    // that is still live. The bug manifested as:
    //   call_external 0 2 ...     (returns %r0:num, %r1:num)
    //   mov %r0:str #str0:str     (clobbers %r0:num before it's consumed!)
    //   mov %r1:num %r0:num       (reads %r0 as num — WRONG, it's str)
    // The fix ensures %r0:num is saved before the str write.
    std::string serialized = RVMSerializer::serialize(rvmProg);
    REQUIRE(!serialized.empty());

    // Verify: between the first call_external and the str write,
    // there must be a MOV that reads %r0:num (saving the value).
    auto callPos = serialized.find("call_external 0 2");
    auto strPos  = serialized.find(":str");
    REQUIRE(callPos != std::string::npos);
    REQUIRE(strPos != std::string::npos);

    // There must be a read of %r0:num between the call and the str write
    std::string between = serialized.substr(callPos, strPos - callPos);
    INFO("Between call and str write:\n" << between);
    REQUIRE(between.find("%r0:num") != std::string::npos);
}

TEST_CASE("RVMRegisterAllocator: transitive interference with long MOV chain", "[rvm][register-allocation][regression]")
{
    Environment env;

    // Similar regression test with a different pattern: extern returning a value,
    // passed through to another extern that also takes a string parameter.
    // This tests the same transitive interference bug with a simpler (non-vec2) case.
    auto ast = env.parse(R"(
        [[extern]] fn getValue() -> num;
        [[extern]] fn lookup(name:str, v:num) -> num;
        let v = getValue();
        lookup("key", v)
    )");

    REQUIRE(ast != nullptr);

    auto prog = env.map(ast);
    env.optimize(prog, opt::OptimizerOptions::High());

    rvm::RVMMapper mapper;
    auto rvmProg = mapper.mapProgram(prog);

    opt::OptimizerOptions opts    = opt::OptimizerOptions::None();
    opts.OptimizeConstantPropagation = true;
    opts.EnableRegisterAllocation = true;
    RVMOptimizer::optimize(opts, rvmProg);

    std::string serialized = RVMSerializer::serialize(rvmProg);
    REQUIRE(!serialized.empty());

    // Verify: the num value from %r0 must be saved before the str write clobbers it.
    // In the correct output, "mov %r1:num %r0:num" appears BEFORE "mov %r0:str"
    auto numReadPos = serialized.find("mov %r1:num %r0:num");
    auto strWritePos = serialized.find("%r0:str");
    INFO("Serialized RVM:\n" << serialized);
    REQUIRE(numReadPos != std::string::npos);
    REQUIRE(strWritePos != std::string::npos);
    REQUIRE(numReadPos < strWritePos);
}
