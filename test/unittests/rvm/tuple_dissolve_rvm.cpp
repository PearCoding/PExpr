#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Environment.h"
#include "opt/SSAOptimizer.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMInterpreter.h"
#include "rvm/RVMSerializer.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;

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
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

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
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

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
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

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
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

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
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

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
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

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
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

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
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

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
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    // Map to RVM
    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Serialize RVM program
    std::string rvmCode = rvm::RVMSerializer::serialize(rvmProgram);

    // Should have multiple mul operations
    REQUIRE(rvmCode.find("mul") != std::string::npos);
}

// ============================================================================
// Tests for Linear Index Calculation (Nested Tuple Access)
// ============================================================================

TEST_CASE("Tuple dissolve - linear index: double nested access", "[tuple_dissolve][rvm][linear_index]")
{
    const char* source = R"(
        let outer = [[1, 2], [3, 4]];
        outer[0][1]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 2);
}

TEST_CASE("Tuple dissolve - linear index: triple nested access", "[tuple_dissolve][rvm][linear_index]")
{
    const char* source = R"(
        let outer = [[[1, 2], [3, 4]], [[5, 6], [7, 8]]];
        outer[0][1][0]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 3);
}

TEST_CASE("Tuple dissolve - linear index: multiple nested accesses", "[tuple_dissolve][rvm][linear_index]")
{
    const char* source = R"(
        let outer = [[1, 2], [3, 4]];
        outer[0][0] + outer[0][1] + outer[1][0] + outer[1][1]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result: 1 + 2 + 3 + 4 = 10
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 10);
}

TEST_CASE("Tuple dissolve - linear index: deeply nested with mixed types", "[tuple_dissolve][rvm][linear_index]")
{
    const char* source = R"(
        let outer = [[1, 2.5], [true, 3.5]];
        outer[0][1] + outer[1][1]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result: 2.5 + 3.5 = 6.0
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Number));
    
    REQUIRE(std::holds_alternative<Number>(result));
    REQUIRE(std::get<Number>(result) == Catch::Approx(6.0));
}

TEST_CASE("Tuple dissolve - linear index: four-level nesting", "[tuple_dissolve][rvm][linear_index]")
{
    const char* source = R"(
        let deep = [[[[1, 2], [3, 4]], [[5, 6], [7, 8]]], [[[9, 10], [11, 12]], [[13, 14], [15, 16]]]];
        deep[0][0][1][0]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 3);
}

// ============================================================================
// Tests for Previously Mapped Elements
// ============================================================================

TEST_CASE("Tuple dissolve - previously mapped: tuple copy then access", "[tuple_dissolve][rvm][mapped_elements]")
{
    const char* source = R"(
        let t1 = [1, 2, 3];
        let t2 = t1;
        t2[0] + t2[1] + t2[2]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result: 1 + 2 + 3 = 6
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 6);
}

TEST_CASE("Tuple dissolve - previously mapped: multiple accesses to same element", "[tuple_dissolve][rvm][mapped_elements]")
{
    const char* source = R"(
        let t = [10, 20, 30];
        t[0] + t[0] + t[1] + t[1] + t[2] + t[2]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result: 10+10+20+20+30+30 = 120
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 120);
}

TEST_CASE("Tuple dissolve - previously mapped: tuple passed to function then accessed", "[tuple_dissolve][rvm][mapped_elements]")
{
    const char* source = R"(
        fn get_first(t:[int, int, int]) -> int = {
            t[0]
        };
        let t = [5, 10, 15];
        get_first(t)
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 5);
}

TEST_CASE("Tuple dissolve - previously mapped: tuple operation then access", "[tuple_dissolve][rvm][mapped_elements]")
{
    const char* source = R"(
        let a = [1, 2, 3];
        let b = [4, 5, 6];
        let c = a + b;
        c[0] + c[1] + c[2]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result: 5 + 7 + 9 = 21
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 21);
}

// ============================================================================
// Tests for Edge Cases
// ============================================================================

TEST_CASE("Tuple dissolve - edge case: single element tuple", "[tuple_dissolve][rvm][edge_case]")
{
    const char* source = R"(
        let t = [42];
        t[0]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 42);
}

TEST_CASE("Tuple dissolve - edge case: mixed types in tuple", "[tuple_dissolve][rvm][edge_case]")
{
    const char* source = R"(
        let t = [1, 2.5, true, "hello"];
        t[0] + t[1]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result: 1 + 2.5 = 3.5
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Number));
    
    REQUIRE(std::holds_alternative<Number>(result));
    REQUIRE(std::get<Number>(result) == Catch::Approx(3.5));
}

TEST_CASE("Tuple dissolve - edge case: tuple with nested tuple as element", "[tuple_dissolve][rvm][edge_case]")
{
    const char* source = R"(
        let t = [1, [2, 3], 4];
        t[1][0] + t[1][1]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result: 2 + 3 = 5
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 5);
}

TEST_CASE("Tuple dissolve - edge case: tuple phi with constant branches", "[tuple_dissolve][rvm][edge_case]")
{
    const char* source = R"(
        let t = if true {
            [1, 2]
        } else {
            [3, 4]
        };
        t[0]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Register external function
    rvm::RVMInterpreter interpreter;

    // Execute and verify result (should be 1 from first branch)
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 1);
}

TEST_CASE("Tuple dissolve - edge case: tuple parameters in function", "[tuple_dissolve][rvm][edge_case]")
{
    const char* source = R"(
        fn sum_pair(p:[int, int]) -> int = {
            p[0] + p[1]
        };
        sum_pair([10, 20])
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result: 10 + 20 = 30
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 30);
}

TEST_CASE("Tuple dissolve - edge case: tuple returns from function", "[tuple_dissolve][rvm][edge_case]")
{
    const char* source = R"(
        fn make_pair() -> [int, int] = {
            [7, 8]
        };
        let p = make_pair();
        p[0] + p[1]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result: 7 + 8 = 15
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Integer));
    
    REQUIRE(std::holds_alternative<Integer>(result));
    REQUIRE(std::get<Integer>(result) == 15);
}

TEST_CASE("Tuple dissolve - edge case: scalar times tuple", "[tuple_dissolve][rvm][edge_case]")
{
    const char* source = R"(
        let scalar = 3;
        let vec = [1, 2, 3];
        let result = vec * scalar;
        result[0] + result[1] + result[2]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result: 3 + 6 + 9 = 18
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Number));
    
    REQUIRE(std::holds_alternative<Number>(result));
    REQUIRE(std::get<Number>(result) == 18);
}

TEST_CASE("Tuple dissolve - edge case: tuple times scalar", "[tuple_dissolve][rvm][edge_case]")
{
    const char* source = R"(
        let scalar = 2;
        let vec = [1, 2, 3];
        let result = scalar * vec;
        result[0] + result[1] + result[2]
    )";

    Environment env;
    auto closure = env.parse(source);
    REQUIRE(closure);

    auto program = env.map(closure);
    bool changed = env.optimize(program, opt::OptimizerOptions::None());
    REQUIRE(changed);

    rvm::RVMMapper mapper;
    auto rvmProgram = mapper.mapProgram(program);

    // Execute and verify result: 2 + 4 + 6 = 12
    rvm::RVMInterpreter interpreter;
    auto result = interpreter.execute(rvmProgram, type::Type(type::TypeKind::Number));
    
    REQUIRE(std::holds_alternative<Number>(result));
    REQUIRE(std::get<Number>(result) == 12);
}
