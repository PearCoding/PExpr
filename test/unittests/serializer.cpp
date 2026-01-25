#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;
using namespace PExpr::internal;

TEST_CASE("SSASerializer: round-trip serialization of simple program", "[serializer]")
{
    Environment env;
    auto ast = env.parse("let mut x = 1; x + 2");

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    // Serialize to string
    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE(!serialized.empty());

    // Deserialize back
    SSAProgram deserialized = SSASerializer::deserialize(serialized);

    // Reserialize and compare
    std::string reserialized = SSASerializer::serialize(deserialized);
    REQUIRE(serialized == reserialized);
}

TEST_CASE("SSASerializer: round-trip with function", "[serializer]")
{
    Environment env;
    auto ast = env.parse("fn add(a:int, b:int) -> int = a + b; add(1, 2)");

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE(!serialized.empty());

    SSAProgram deserialized  = SSASerializer::deserialize(serialized);
    std::string reserialized = SSASerializer::serialize(deserialized);

    REQUIRE(serialized == reserialized);
}

TEST_CASE("SSASerializer: round-trip with branch and phi", "[serializer]")
{
    Environment env;
    auto ast = env.parse("if true { 1 } else { 2 }");

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE(!serialized.empty());
    REQUIRE(serialized.find("phi[") != std::string::npos);

    SSAProgram deserialized  = SSASerializer::deserialize(serialized);
    std::string reserialized = SSASerializer::serialize(deserialized);

    REQUIRE(serialized == reserialized);
}

TEST_CASE("SSASerializer: round-trip with vector types", "[serializer]")
{
    Environment env;
    auto ast = env.parse("[1.0, 2.0, 3.0]");

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE(!serialized.empty());
    REQUIRE((serialized.find("vec3") != std::string::npos || serialized.find("[1.000000,2.000000,3.000000]") != std::string::npos));

    SSAProgram deserialized  = SSASerializer::deserialize(serialized);
    std::string reserialized = SSASerializer::serialize(deserialized);

    REQUIRE(serialized == reserialized);
}

TEST_CASE("SSASerializer: round-trip with call instruction", "[serializer]")
{
    Environment env;
    auto ast = env.parse("fn square(x:int) -> int = x * x; square(5)");

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE(!serialized.empty());
    REQUIRE(serialized.find("call[") != std::string::npos);

    SSAProgram deserialized  = SSASerializer::deserialize(serialized);
    std::string reserialized = SSASerializer::serialize(deserialized);

    REQUIRE(serialized == reserialized);
}

TEST_CASE("SSASerializer: round-trip with external function", "[serializer]")
{
    // Create a simple program with external function
    SSAProgram prog;
    SSAFunction extFunc;
    extFunc.Name          = "_Z8external_P_L5C0";
    extFunc.External      = true;
    extFunc.HasSideEffect = true;
    extFunc.Parameters    = { "x" };
    extFunc.ReturnType    = ElementaryType::Integer;

    // Add a call to the external function
    auto call                = std::make_shared<SSAInstrCall>();
    call->Target             = SSAValue(SSAValue::Kind::Named, "result.1", ElementaryType::Integer);
    call->FunctionName       = "_Z8external_P_L5C0";
    call->PublicFunctionName = "external";
    call->Arguments.push_back(SSAValue(SSAValue::Kind::Constant, {}, ElementaryType::Integer, static_cast<int64_t>(42)));

    prog.Functions.push_back(extFunc);
    prog.Body.push_back(call);

    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE(!serialized.empty());
    REQUIRE(serialized.find("[[extern]]") != std::string::npos);
    REQUIRE(serialized.find("call[") != std::string::npos);

    SSAProgram deserialized  = SSASerializer::deserialize(serialized);
    std::string reserialized = SSASerializer::serialize(deserialized);

    REQUIRE(serialized == reserialized);
}

TEST_CASE("SSASerializer: parseType handles vector types", "[serializer]")
{
    // Test basic types
    REQUIRE(SSASerializer::parseType("bool") == ElementaryType::Boolean);
    REQUIRE(SSASerializer::parseType("int") == ElementaryType::Integer);
    REQUIRE(SSASerializer::parseType("num") == ElementaryType::Number);
    REQUIRE(SSASerializer::parseType("str") == ElementaryType::String);
    REQUIRE(SSASerializer::parseType("invalid") == ElementaryType::Unspecified);

    // Test vector types - they should be calculated relative to Vec1
    ElementaryType vec1 = SSASerializer::parseType("vec1");
    ElementaryType vec2 = SSASerializer::parseType("vec2");
    ElementaryType vec3 = SSASerializer::parseType("vec3");
    ElementaryType vec4 = SSASerializer::parseType("vec4");

    REQUIRE(vec1 >= ElementaryType::Vec1);
    REQUIRE(vec2 > vec1);
    REQUIRE(vec3 > vec2);
    REQUIRE(vec4 > vec3);

    // Verify typeArraySize works correctly
    REQUIRE(typeArraySize(vec1) == 1);
    REQUIRE(typeArraySize(vec2) == 2);
    REQUIRE(typeArraySize(vec3) == 3);
    REQUIRE(typeArraySize(vec4) == 4);
}

TEST_CASE("SSASerializer: escapeString and unescapeString", "[serializer]")
{
    std::string original  = "Hello\nWorld\t\"Quote\"\\Backslash";
    std::string escaped   = SSASerializer::escapeString(original);
    std::string unescaped = SSASerializer::unescapeString(escaped);

    REQUIRE(escaped.find("\\n") != std::string::npos);
    REQUIRE(escaped.find("\\t") != std::string::npos);
    REQUIRE(escaped.find("\\\"") != std::string::npos);
    REQUIRE(escaped.find("\\\\") != std::string::npos);
    REQUIRE(unescaped == original);
}

TEST_CASE("SSASerializer: handles empty program", "[serializer]")
{
    SSAProgram prog;
    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE((serialized.empty() || serialized == "\n"));

    SSAProgram deserialized  = SSASerializer::deserialize(serialized);
    std::string reserialized = SSASerializer::serialize(deserialized);

    REQUIRE(serialized == reserialized);
}

TEST_CASE("SSASerializer: round-trip with multiple instructions", "[serializer]")
{
    Environment env;
    auto ast = env.parse("let mut a = 1; let mut b = 2; a + b");

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE(!serialized.empty());

    SSAProgram deserialized  = SSASerializer::deserialize(serialized);
    std::string reserialized = SSASerializer::serialize(deserialized);

    REQUIRE(serialized == reserialized);
}

TEST_CASE("SSASerializer: handles comments in PExprIR", "[serializer]")
{
    SECTION("Line comments")
    {
        std::string program = R"(
// This is a line comment
fn test_func(a:num):num
  // Another line comment
  b.1:num = add(a:num, 1.0:num)
  return b.1:num
endfn
)";

        SSAProgram prog = SSASerializer::deserialize(program);
        REQUIRE(prog.Functions.size() == 1);
        REQUIRE(prog.Functions[0].Name == "test_func");
        REQUIRE(prog.Functions[0].Parameters.size() == 1);
        REQUIRE(prog.Functions[0].Parameters[0] == "a");

        // Reserialize and ensure comments are stripped
        std::string reserialized = SSASerializer::serialize(prog);
        REQUIRE(reserialized.find("//") == std::string::npos);
    }

    SECTION("Block comments")
    {
        std::string program = R"(
/* This is a 
   block comment
   spanning multiple lines */
fn test_func(x:int):int
  /* Comment inside function */
  y.1:int = add(x:int, 5:int)
  return y.1:int
endfn
)";

        SSAProgram prog = SSASerializer::deserialize(program);
        REQUIRE(prog.Functions.size() == 1);
        REQUIRE(prog.Functions[0].Name == "test_func");

        // Reserialize and ensure comments are stripped
        std::string reserialized = SSASerializer::serialize(prog);
        REQUIRE(reserialized.find("/*") == std::string::npos);
        REQUIRE(reserialized.find("*/") == std::string::npos);
    }

    SECTION("Mixed comments")
    {
        std::string program = R"(
// Line comment before function
[[extern, pure]] fn test_func():bool
  /* Block comment
     inside function */
  result.1:bool = assign(true:bool)
  return result.1:bool // Inline comment
endfn
// Line comment after function
)";

        SSAProgram prog = SSASerializer::deserialize(program);
        REQUIRE(prog.Functions.size() == 1);
        REQUIRE(prog.Functions[0].Name == "test_func");
        REQUIRE(prog.Functions[0].External == true);
        REQUIRE(prog.Functions[0].HasSideEffect == false);

        // Reserialize and ensure comments are stripped
        std::string reserialized = SSASerializer::serialize(prog);
        REQUIRE(reserialized.find("//") == std::string::npos);
        REQUIRE(reserialized.find("/*") == std::string::npos);
        REQUIRE(reserialized.find("*/") == std::string::npos);
    }

    SECTION("Comment at end of line")
    {
        std::string program = R"(
fn test_func():num
  a.1:num = assign(1.0:num) // inline comment
  b.1:num = add(a.1:num, 2.0:num) /* another inline */
  return b.1:num
endfn
)";

        SSAProgram prog = SSASerializer::deserialize(program);
        REQUIRE(prog.Functions.size() == 1);
        REQUIRE(prog.Functions[0].Name == "test_func");

        // Reserialize and compare
        std::string reserialized = SSASerializer::serialize(prog);
        SSAProgram reparsed      = SSASerializer::deserialize(reserialized);
        REQUIRE(reparsed.Functions.size() == 1);
    }

    SECTION("Unclosed block comment continues across lines")
    {
        std::string program = R"(
/* This block comment is not closed on this line
fn ignored_function():bool
  result.1:bool = assign(false:bool)
  return result.1:bool
endfn
*/ // The comment ends here

fn actual_function():num
  value.1:num = assign(42.0:num)
  return value.1:num
endfn
)";

        SSAProgram prog = SSASerializer::deserialize(program);
        // Only actual_function should be parsed
        REQUIRE(prog.Functions.size() == 1);
        REQUIRE(prog.Functions[0].Name == "actual_function");
    }
}