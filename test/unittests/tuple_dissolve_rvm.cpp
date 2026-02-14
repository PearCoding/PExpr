#include <catch2/catch_test_macros.hpp>

#include "Environment.h"
#include "opt/SSAOptimizer.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMSerializer.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;

[[nodiscard]] inline static auto MakeTupleOptimization()
{
    auto opts           = opt::OptimizerOptions::None();
    opts.RemoveDeadCode = true;
    opts.DissolveTuples = true;
    return opts;
}

TEST_CASE("Tuple dissolve with RVM mapping - simple tuple return", "[tuple_dissolve][rvm]")
{
    const char* source = R"(
        fn make_tuple() -> [int, num] = {
            [42, 3.14]
        };
        let t = make_tuple();
        t[0] + t[1]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply tuple dissolve optimization
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

    // Should have no tuple instructions in RVM
    REQUIRE(rvmCode.find("tuple[") == std::string::npos);

    // Should have proper function calls and register assignments
    REQUIRE(rvmCode.find("call_internal") != std::string::npos);
}

TEST_CASE("Tuple dissolve with RVM mapping - nested tuple arguments", "[tuple_dissolve][rvm]")
{
    const char* source = R"(
        fn process(pair:[int, num]) -> num = {
            let [x, y] = pair;
            x + y
        };
        let t = [5, 2.5];
        process(t)
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply tuple dissolve optimization
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

    // Should have no tuple instructions in RVM
    REQUIRE(rvmCode.find("tuple[") == std::string::npos);

    // Should have proper function calls with multiple arguments
    REQUIRE(rvmCode.find("call_internal") != std::string::npos);
}

TEST_CASE("Tuple dissolve with RVM mapping - tuple in phi node", "[tuple_dissolve][rvm]")
{
    const char* source = R"(
        [[extern, pure]] fn get_bool() -> bool;
        
        let t = if get_bool() {
            [1, 2.0]
        } else {
            [3, 4.0]
        };
        t[0] + t[1]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply tuple dissolve optimization
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

    // Should have no tuple instructions in RVM
    REQUIRE(rvmCode.find("tuple[") == std::string::npos);

    // Should have proper RVM code (phi nodes may be optimized away)
    REQUIRE_FALSE(rvmCode.empty());
    // Check that we have some form of conditional logic
    REQUIRE(rvmCode.find("jnz") != std::string::npos);
}

TEST_CASE("Tuple dissolve with RVM mapping - complex nested tuple", "[tuple_dissolve][rvm]")
{
    const char* source = R"(
        let outer = [[1, 2.0], [3, 4.0]];
        let inner = outer[0];
        inner[0] + inner[1]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply tuple dissolve optimization
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

    // Should have no tuple instructions in RVM
    REQUIRE(rvmCode.find("tuple[") == std::string::npos);

    // Should have element access operations
    REQUIRE(rvmCode.find("mov") != std::string::npos);
}

TEST_CASE("Tuple dissolve with RVM mapping - tuple swizzle", "[tuple_dissolve][rvm]")
{
    const char* source = R"(
        let v = [1.0, 2.0, 3.0, 4.0];
        let swizzled = v.zyx;
        swizzled[0] + swizzled[1] + swizzled[2]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply tuple dissolve optimization
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

    // Should have no tuple instructions in RVM
    REQUIRE(rvmCode.find("tuple[") == std::string::npos);

    // Should have multiple element assignments
    REQUIRE(rvmCode.find("mov") != std::string::npos);
}

TEST_CASE("Tuple dissolve with RVM mapping - tuple binary operations", "[tuple_dissolve][rvm]")
{
    const char* source = R"(
        let a = [1.0, 2.0, 3.0];
        let b = [4.0, 5.0, 6.0];
        let c = a + b;
        c.x + c.y + c.z
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply tuple dissolve optimization
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

    // Should have no tuple instructions in RVM
    REQUIRE(rvmCode.find("tuple[") == std::string::npos);

    // Should have multiple add operations
    REQUIRE(rvmCode.find("add") != std::string::npos);
}

TEST_CASE("Tuple dissolve with RVM mapping - tuple copy propagation", "[tuple_dissolve][rvm]")
{
    const char* source = R"(
        let t1 = [1, 2.0, true];
        let t2 = t1;
        t2[0] + t2[1]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply tuple dissolve optimization
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

    // Should have no tuple instructions in RVM
    REQUIRE(rvmCode.find("tuple[") == std::string::npos);

    // Should have element-wise copy operations
    REQUIRE(rvmCode.find("mov") != std::string::npos);
}

TEST_CASE("Tuple dissolve with RVM mapping - function returning tuple", "[tuple_dissolve][rvm]")
{
    const char* source = R"(
        fn make_pair() -> [int, num] = {
            [42, 3.14]
        };
        
        fn use_pair(pair:[int, num]) -> num = {
            let [x, y] = pair;
            x + y
        };
        
        use_pair(make_pair())
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply tuple dissolve optimization
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

    // Should have no tuple instructions in RVM
    REQUIRE(rvmCode.find("tuple[") == std::string::npos);

    // Should have proper function calls with multiple return values
    REQUIRE(rvmCode.find("call_internal") != std::string::npos);
}

TEST_CASE("Tuple dissolve with RVM mapping - mixed scalar/tuple operations", "[tuple_dissolve][rvm]")
{
    const char* source = R"(
        let scalar = 2.0;
        let vec = [1.0, 2.0, 3.0];
        let scaled = vec * scalar;
        scaled.x + scaled.y + scaled.z
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);

    // Apply tuple dissolve optimization
    bool changed = env.optimize(program, MakeTupleOptimization());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

    // Should have no tuple instructions in RVM
    REQUIRE(rvmCode.find("tuple[") == std::string::npos);

    // Should have multiple mul operations
    REQUIRE(rvmCode.find("mul") != std::string::npos);
}