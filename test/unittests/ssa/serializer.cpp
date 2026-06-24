#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSASerializer.h"

using namespace PExpr;
using namespace PExpr::ssa;
using namespace PExpr::type;

TEST_CASE("SSASerializer: round-trip serialization of simple program", "[serializer]")
{
    Environment env;
    auto ast  = env.parse("let mut x = 1; x + 2");
    auto prog = env.map(ast);

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
    auto ast  = env.parse("fn add(a:int, b:int) -> int = a + b; add(1, 2)");
    auto prog = env.map(ast);

    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE(!serialized.empty());

    SSAProgram deserialized  = SSASerializer::deserialize(serialized);
    std::string reserialized = SSASerializer::serialize(deserialized);

    REQUIRE(serialized == reserialized);
}

TEST_CASE("SSASerializer: round-trip with branch and phi", "[serializer]")
{
    Environment env;
    auto ast  = env.parse("if true { 1 } else { 2 }");
    auto prog = env.map(ast);

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
    auto ast  = env.parse("[1.0, 2.0, 3.0]");
    auto prog = env.map(ast);

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
    auto ast  = env.parse("fn square(x:int) -> int = x * x; square(5)");
    auto prog = env.map(ast);

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
    extFunc.Parameters    = { SSAValue::Named("x", Type(TypeKind::Integer)) };
    extFunc.ReturnType    = Type(TypeKind::Integer);

    // Add a call to the external function
    auto call                = std::make_shared<SSAInstrCall>();
    call->Target             = SSAValue::Named("result.1", Type(TypeKind::Integer));
    call->FunctionName       = "_Z8external_P_L5C0";
    call->PublicFunctionName = "external";
    call->Arguments.push_back(SSAValue::Constant(static_cast<Integer>(42)));

    prog.Functions.push_back(extFunc);
    prog.Body.push_back(call);

    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE(!serialized.empty());
    REQUIRE(serialized.find("@[extern]") != std::string::npos);
    REQUIRE(serialized.find("call[") != std::string::npos);

    SSAProgram deserialized  = SSASerializer::deserialize(serialized);
    std::string reserialized = SSASerializer::serialize(deserialized);

    REQUIRE(serialized == reserialized);
}

TEST_CASE("SSASerializer: parseType handles vector types", "[serializer]")
{
    // Test basic types
    REQUIRE(SSASerializer::parseType("bool").kind() == TypeKind::Boolean);
    REQUIRE(SSASerializer::parseType("int").kind() == TypeKind::Integer);
    REQUIRE(SSASerializer::parseType("num").kind() == TypeKind::Number);
    REQUIRE(SSASerializer::parseType("str").kind() == TypeKind::String);
    REQUIRE(SSASerializer::parseType("invalid").kind() == TypeKind::Unspecified);

    // Test vector types
    const auto vec2 = SSASerializer::parseType("vec2");
    const auto vec3 = SSASerializer::parseType("vec3");
    const auto vec4 = SSASerializer::parseType("vec4");

    REQUIRE(vec2.isVector());
    REQUIRE(vec3.isVector());
    REQUIRE(vec4.isVector());

    // Verify size works correctly
    REQUIRE(vec2.size() == 2);
    REQUIRE(vec3.size() == 3);
    REQUIRE(vec4.size() == 4);
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
    auto ast  = env.parse("let mut a = 1; let mut b = 2; a + b");
    auto prog = env.map(ast);

    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE(!serialized.empty());

    SSAProgram deserialized  = SSASerializer::deserialize(serialized);
    std::string reserialized = SSASerializer::serialize(deserialized);

    REQUIRE(serialized == reserialized);
}

TEST_CASE("SSASerializer: round-trip with tuple access instructions", "[serializer]")
{
    // This tests the specific bug where parseValueList splits on commas
    // inside tuple type brackets, e.g. access(t:[int, int], 0:int)
    // was incorrectly parsed as having only one operand (0:int)
    std::string program = R"(
fn _Z10take_tuple_PT2ii(t_L4C15) : int
  %.2:int = access(t_L4C15:[int, int], 0:int)
  %.3:int = access(t_L4C15:[int, int], 1:int)
  %.4:int = add(%.2:int, %.3:int)
  return %.4:int
endfn
)";

    SSAProgram prog = SSASerializer::deserialize(program);
    REQUIRE(prog.Functions.size() == 1);
    REQUIRE(prog.Functions[0].Body.size() == 4);

    // Verify both access instructions have 2 operands (tuple ref + index)
    auto* access0 = dynamic_cast<SSAInstrAssign*>(prog.Functions[0].Body[0].get());
    auto* access1 = dynamic_cast<SSAInstrAssign*>(prog.Functions[0].Body[1].get());
    REQUIRE(access0 != nullptr);
    REQUIRE(access1 != nullptr);
    REQUIRE(access0->Operands.size() == 2);
    REQUIRE(access1->Operands.size() == 2);

    // Verify the operands are correct: first is the tuple, second is the index
    REQUIRE(access0->Operands[0].name() == "t_L4C15");
    REQUIRE(access0->Operands[0].type().isTuple());
    REQUIRE(access0->Operands[1].isConstant());
    REQUIRE(access1->Operands[0].name() == "t_L4C15");
    REQUIRE(access1->Operands[1].isConstant());

    // Verify round-trip
    std::string serialized   = SSASerializer::serialize(prog);
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
        REQUIRE(prog.Functions[0].Parameters[0].name() == "a");

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
@[extern, pure] fn test_func():bool
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
TEST_CASE("SSASerializer: tuple-typed call target preserves function name", "[serializer]")
{
    // Regression: parsing located the call bracket with line.find('['), which for
    // a tuple-typed target picked up the '[' of the target's type, so the function
    // name was parsed as "int, int" instead of the mangled name.
    std::string program = R"(
%.8:[int, int] = call[_Z6myfunc_Pi](a:int)
)";

    SSAProgram prog = SSASerializer::deserialize(program);
    REQUIRE(prog.Body.size() == 1);

    auto* call = dynamic_cast<SSAInstrCall*>(prog.Body[0].get());
    REQUIRE(call != nullptr);
    REQUIRE(call->FunctionName == "_Z6myfunc_Pi");
    REQUIRE(call->Target.type().isTuple());
    REQUIRE(call->Arguments.size() == 1);
}

TEST_CASE("SSASerializer: tuple-typed phi target parses conditions and branches", "[serializer]")
{
    std::string program = R"(
%.6:[int, int] = phi[%.1:bool](%.5:[int, int], %.4:[int, int])
)";

    SSAProgram prog = SSASerializer::deserialize(program);
    REQUIRE(prog.Body.size() == 1);

    auto* phi = dynamic_cast<SSAInstrPhi*>(prog.Body[0].get());
    REQUIRE(phi != nullptr);
    REQUIRE(phi->Target.type().isTuple());
    REQUIRE(phi->Conditions.size() == 1);
    REQUIRE(phi->Branches.size() == 2);
}

TEST_CASE("SSASerializer: round-trip of a program with a tuple-returning call", "[serializer]")
{
    // A mutable-capturing function is uplifted into one that returns a tuple, so
    // its call site has a tuple-typed target. This exercises the call-bracket fix
    // through the full pipeline.
    Environment env;
    auto ast  = env.parse(R"(
        let mut x = 5;
        fn f() = { if x > 0 { x = x + 1; } else { x = x - 1; } };
        f();
        x
    )");
    auto prog = env.map(ast);

    std::string serialized = SSASerializer::serialize(prog);
    REQUIRE(serialized.find("call[") != std::string::npos);

    SSAProgram deserialized  = SSASerializer::deserialize(serialized);
    std::string reserialized = SSASerializer::serialize(deserialized);
    REQUIRE(serialized == reserialized);
}
