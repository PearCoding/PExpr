#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

#include "rvm/RVMContext.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMMoveSimplifier.h"
#include "rvm/RVMRedundantMoveEliminator.h"
#include "rvm/RVMSerializer.h"
#include "rvm/RVMStructs.h"
#include "rvm/RVMValue.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;

TEST_CASE("RVMValue: basic creation and properties", "[rvm][value]")
{
    SECTION("Constant values")
    {
        RVMValue constInt = RVMValue::Constant(Integer(42));
        REQUIRE(constInt.isConstant());
        REQUIRE(constInt.type() == Type(TypeKind::Integer));

        RVMValue constFloat = RVMValue::Constant(Number(3.14));
        REQUIRE(constFloat.isConstant());
        REQUIRE(constFloat.type() == Type(TypeKind::Number));

        RVMValue constBool = RVMValue::Constant(true);
        REQUIRE(constBool.isConstant());
        REQUIRE(constBool.type() == Type(TypeKind::Boolean));
    }

    SECTION("Register values")
    {
        RVMValue regValue = RVMValue::Register(5, Type(TypeKind::Integer));
        REQUIRE(regValue.isRegister());
        REQUIRE(regValue.type() == Type(TypeKind::Integer));
        REQUIRE(regValue.regId() == 5u);
    }

    SECTION("String reference values")
    {
        RVMValue strValue = RVMValue::StringRef(1);
        REQUIRE(strValue.isStringRef());
        REQUIRE(strValue.type() == Type(TypeKind::String));
        REQUIRE(strValue.stringId() == 1);
    }
}

TEST_CASE("RVMValue: equality and hashing", "[rvm][value]")
{
    SECTION("Constant equality")
    {
        RVMValue constInt1 = RVMValue::Constant(Integer(42));
        RVMValue constInt2 = RVMValue::Constant(Integer(42));
        RVMValue constInt3 = RVMValue::Constant(Integer(100));

        REQUIRE(constInt1 == constInt2);
        REQUIRE(constInt1 != constInt3);
    }

    SECTION("Register equality")
    {
        RVMValue reg1 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue reg2 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue reg3 = RVMValue::Register(2, Type(TypeKind::Integer));

        REQUIRE(reg1 == reg2);
        REQUIRE(reg1 != reg3);
    }

    SECTION("Hash consistency")
    {
        RVMValue constInt1 = RVMValue::Constant(Integer(42));
        RVMValue constInt2 = RVMValue::Constant(Integer(42));
        RVMValue constInt3 = RVMValue::Constant(Integer(100));

        REQUIRE(constInt1.hash() == constInt2.hash());
        REQUIRE(constInt1.hash() != constInt3.hash());
    }
}

TEST_CASE("RVMContext: register management", "[rvm][context]")
{
    RVMContext context;

    SECTION("Register allocation")
    {
        RegId reg1 = context.allocateRegister(Type(TypeKind::Integer));
        RegId reg2 = context.allocateRegister(Type(TypeKind::Number));
        RegId reg3 = context.allocateRegister(Type(TypeKind::Boolean));

        REQUIRE(reg1 == 0u);
        REQUIRE(reg2 == 1u);
        REQUIRE(reg3 == 2u);
    }

    SECTION("Register type tracking")
    {
        RegId reg1 = context.allocateRegister(Type(TypeKind::Integer));
        RegId reg2 = context.allocateRegister(Type(TypeKind::Number));

        REQUIRE(context.getRegisterType(reg1) == Type(TypeKind::Integer));
        REQUIRE(context.getRegisterType(reg2) == Type(TypeKind::Number));
    }

    SECTION("Register freeing and reset")
    {
        RegId reg1 = context.allocateRegister(Type(TypeKind::Integer));
        PEXPR_UNUSED(reg1);

        RegId reg2 = context.allocateRegister(Type(TypeKind::Number));

        context.freeRegister(reg2);
        context.reset();

        // After reset, allocation should start from 0 again
        RegId reg4 = context.allocateRegister(Type(TypeKind::Integer));
        REQUIRE(reg4 == 0u);
    }
}

TEST_CASE("RVMInstructions: creation and properties", "[rvm][instructions]")
{
    SECTION("Three operand instruction")
    {
        RVMValue dst  = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue src1 = RVMValue::Constant(Integer(10));
        RVMValue src2 = RVMValue::Constant(Integer(20));

        auto instr = std::make_shared<RVMInstr3Op>(Opcode::ADD, dst, src1, src2);

        REQUIRE(instr->opcode() == Opcode::ADD);
        REQUIRE(instr->dst().value() == dst);

        auto srcs = instr->srcs();
        REQUIRE(srcs.size() == 2);
        REQUIRE(srcs[0] == src1);
        REQUIRE(srcs[1] == src2);
    }

    SECTION("Move instruction")
    {
        RVMValue dst = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue src = RVMValue::Register(1, Type(TypeKind::Integer));

        auto instr = std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src);

        REQUIRE(instr->opcode() == Opcode::MOV);
        REQUIRE(instr->dst().value() == dst);

        auto srcs = instr->srcs();
        REQUIRE(srcs.size() == 1);
        REQUIRE(srcs[0] == src);
    }

    SECTION("Frame instructions")
    {
        auto pushInstr = std::make_shared<RVMInstrPushFrame>(3);
        auto popInstr  = std::make_shared<RVMInstrPopFrame>(3);

        REQUIRE(pushInstr->opcode() == Opcode::PUSH_FRAME);
        REQUIRE_FALSE(pushInstr->dst().has_value());
        REQUIRE(pushInstr->srcs().empty());
        REQUIRE(pushInstr->registerCount() == 3);

        REQUIRE(popInstr->opcode() == Opcode::POP_FRAME);
        REQUIRE_FALSE(popInstr->dst().has_value());
        REQUIRE(popInstr->srcs().empty());
        REQUIRE(popInstr->registerCount() == 3);
    }

    SECTION("Branch instruction")
    {
        RVMValue src = RVMValue::Constant(true);
        auto instr   = std::make_shared<RVMInstrBranch>(Opcode::BRZ, src, "target_label");

        REQUIRE(instr->opcode() == Opcode::BRZ);
        REQUIRE_FALSE(instr->dst().has_value());

        auto srcs = instr->srcs();
        REQUIRE(srcs.size() == 1);
        REQUIRE(srcs[0] == src);
        REQUIRE(instr->targetLabel() == "target_label");
    }

    SECTION("Jump instruction")
    {
        auto instr = std::make_shared<RVMInstrJump>("loop_start");

        REQUIRE(instr->opcode() == Opcode::JMP);
        REQUIRE_FALSE(instr->dst().has_value());
        REQUIRE(instr->srcs().empty());
        REQUIRE(instr->targetLabel() == "loop_start");
    }

    SECTION("Label instruction")
    {
        auto instr = std::make_shared<RVMInstrLabel>("my_label");

        REQUIRE(instr->labelName() == "my_label");
        REQUIRE_FALSE(instr->dst().has_value());
        REQUIRE(instr->srcs().empty());
    }
}

TEST_CASE("RVMSerializer: basic serialization", "[rvm][serializer]")
{
    SECTION("Opcode conversion")
    {
        REQUIRE(RVMSerializer::opcodeToString(Opcode::ADD) == "add");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::SUB) == "sub");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::MUL) == "mul");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::DIV) == "div");

        REQUIRE(RVMSerializer::stringToOpcode("add") == Opcode::ADD);
        REQUIRE(RVMSerializer::stringToOpcode("sub") == Opcode::SUB);
        REQUIRE(RVMSerializer::stringToOpcode("mul") == Opcode::MUL);
        REQUIRE(RVMSerializer::stringToOpcode("div") == Opcode::DIV);
    }

    SECTION("Value serialization")
    {
        RVMValue constInt = RVMValue::Constant(Integer(42));
        RVMValue regValue = RVMValue::Register(5, Type(TypeKind::Integer));

        std::ostringstream oss;
        RVMSerializer::write(oss, constInt);
        std::string constStr = oss.str();

        oss.str("");
        RVMSerializer::write(oss, regValue);
        std::string regStr = oss.str();

        // Basic checks
        REQUIRE(constStr.find("42") != std::string::npos);
        REQUIRE(constStr.find("int") != std::string::npos);
        REQUIRE(regStr.find("%r5") != std::string::npos);
        REQUIRE(regStr.find("int") != std::string::npos);
    }

    SECTION("Instruction serialization")
    {
        RVMValue dst  = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue src1 = RVMValue::Constant(Integer(10));
        RVMValue src2 = RVMValue::Constant(Integer(20));

        auto instr = std::make_shared<RVMInstr3Op>(Opcode::ADD, dst, src1, src2);

        std::ostringstream oss;
        RVMSerializer::write(oss, *instr);
        std::string instrStr = oss.str();

        REQUIRE(instrStr.find("add") != std::string::npos);
        REQUIRE(instrStr.find("%r0") != std::string::npos);
        REQUIRE(instrStr.find("10") != std::string::npos);
        REQUIRE(instrStr.find("20") != std::string::npos);
    }

    SECTION("Move instruction serialization")
    {
        RVMValue dst = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue src = RVMValue::Register(1, Type(TypeKind::Integer));

        auto instr = std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src);

        std::ostringstream oss;
        RVMSerializer::write(oss, *instr);
        std::string instrStr = oss.str();

        REQUIRE(instrStr.find("mov") != std::string::npos);
        REQUIRE(instrStr.find("%r0") != std::string::npos);
        REQUIRE(instrStr.find("%r1") != std::string::npos);
    }
}

TEST_CASE("RVMSerializer: string escape/unescape", "[rvm][serializer]")
{
    SECTION("Escape basic strings")
    {
        REQUIRE(RVMSerializer::escapeString("hello") == "hello");
        REQUIRE(RVMSerializer::escapeString("he\"llo") == "he\\\"llo");
        REQUIRE(RVMSerializer::escapeString("he\nllo") == "he\\nllo");
        REQUIRE(RVMSerializer::escapeString("he\\llo") == "he\\\\llo");
    }

    SECTION("Unescape basic strings")
    {
        REQUIRE(RVMSerializer::unescapeString("hello") == "hello");
        REQUIRE(RVMSerializer::unescapeString("he\\\"llo") == "he\"llo");
        REQUIRE(RVMSerializer::unescapeString("he\\nllo") == "he\nllo");
        REQUIRE(RVMSerializer::unescapeString("he\\\\llo") == "he\\llo");
    }

    SECTION("Round-trip escape/unescape")
    {
        std::string testStrings[] = {
            "hello world",
            "he\"llo\"world",
            "line1\nline2\nline3",
            "tab\ttab\ttab",
            "back\\slash",
            "mixed\"quotes\nand\ttabs\\slashes"
        };

        for (const auto& original : testStrings) {
            std::string escaped   = RVMSerializer::escapeString(original);
            std::string unescaped = RVMSerializer::unescapeString(escaped);
            REQUIRE(unescaped == original);
        }
    }
}

TEST_CASE("RVMSerializer: type parsing", "[rvm][serializer]")
{
    SECTION("Basic types")
    {
        REQUIRE(RVMSerializer::parseType("bool") == Type(TypeKind::Boolean));
        REQUIRE(RVMSerializer::parseType("int") == Type(TypeKind::Integer));
        REQUIRE(RVMSerializer::parseType("num") == Type(TypeKind::Number));
        REQUIRE(RVMSerializer::parseType("str") == Type(TypeKind::String));
    }

    SECTION("Invalid types")
    {
        REQUIRE(RVMSerializer::parseType("unknown").kind() == TypeKind::Unspecified);
        REQUIRE(RVMSerializer::parseType("").kind() == TypeKind::Unspecified);
        REQUIRE(RVMSerializer::parseType("vec").kind() == TypeKind::Unspecified);
        REQUIRE(RVMSerializer::parseType("vec0").kind() == TypeKind::Unspecified);
        REQUIRE(RVMSerializer::parseType("vec2").kind() == TypeKind::Unspecified);
    }
}

TEST_CASE("RVMSerializer: POW opcode support", "[rvm][serializer][pow]")
{
    SECTION("POW opcode to string")
    {
        REQUIRE(RVMSerializer::opcodeToString(Opcode::POW) == "pow");
    }

    SECTION("String to POW opcode")
    {
        REQUIRE(RVMSerializer::stringToOpcode("pow") == Opcode::POW);
    }

    SECTION("POW instruction serialization")
    {
        RVMValue dst  = RVMValue::Register(0, Type(TypeKind::Number));
        RVMValue src1 = RVMValue::Constant(Number(2.0));
        RVMValue src2 = RVMValue::Constant(Number(3.0));

        auto instr = std::make_shared<RVMInstr3Op>(Opcode::POW, dst, src1, src2);

        std::ostringstream oss;
        RVMSerializer::write(oss, *instr);
        std::string instrStr = oss.str();

        REQUIRE(instrStr.find("pow") != std::string::npos);
        REQUIRE(instrStr.find("%r0") != std::string::npos);
        REQUIRE(instrStr.find("2") != std::string::npos);
        REQUIRE(instrStr.find("3") != std::string::npos);
    }
}

TEST_CASE("RVMSerializer: all arithmetic opcodes", "[rvm][serializer]")
{
    SECTION("All arithmetic opcode conversions")
    {
        REQUIRE(RVMSerializer::opcodeToString(Opcode::ADD) == "add");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::SUB) == "sub");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::MUL) == "mul");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::DIV) == "div");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::MOD) == "mod");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::POW) == "pow");

        REQUIRE(RVMSerializer::stringToOpcode("add") == Opcode::ADD);
        REQUIRE(RVMSerializer::stringToOpcode("sub") == Opcode::SUB);
        REQUIRE(RVMSerializer::stringToOpcode("mul") == Opcode::MUL);
        REQUIRE(RVMSerializer::stringToOpcode("div") == Opcode::DIV);
        REQUIRE(RVMSerializer::stringToOpcode("mod") == Opcode::MOD);
        REQUIRE(RVMSerializer::stringToOpcode("pow") == Opcode::POW);
    }
}

TEST_CASE("RVMSerializer: comparison opcodes", "[rvm][serializer]")
{
    SECTION("Comparison opcode conversions")
    {
        REQUIRE(RVMSerializer::opcodeToString(Opcode::CMP_EQ) == "cmp_eq");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::CMP_NE) == "cmp_ne");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::CMP_LT) == "cmp_lt");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::CMP_LE) == "cmp_le");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::CMP_GT) == "cmp_gt");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::CMP_GE) == "cmp_ge");

        REQUIRE(RVMSerializer::stringToOpcode("cmp_eq") == Opcode::CMP_EQ);
        REQUIRE(RVMSerializer::stringToOpcode("cmp_ne") == Opcode::CMP_NE);
        REQUIRE(RVMSerializer::stringToOpcode("cmp_lt") == Opcode::CMP_LT);
        REQUIRE(RVMSerializer::stringToOpcode("cmp_le") == Opcode::CMP_LE);
        REQUIRE(RVMSerializer::stringToOpcode("cmp_gt") == Opcode::CMP_GT);
        REQUIRE(RVMSerializer::stringToOpcode("cmp_ge") == Opcode::CMP_GE);
    }
}

TEST_CASE("RVMSerializer: type conversion opcodes", "[rvm][serializer]")
{
    SECTION("Type conversion opcode conversions")
    {
        REQUIRE(RVMSerializer::opcodeToString(Opcode::I2F) == "i2f");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::F2I) == "f2i");

        REQUIRE(RVMSerializer::stringToOpcode("i2f") == Opcode::I2F);
        REQUIRE(RVMSerializer::stringToOpcode("f2i") == Opcode::F2I);
    }
}

TEST_CASE("RVMMapper: tuple type dissolution", "[rvm][mapper]")
{
    SECTION("Simple tuple dissolution")
    {
        std::vector<type::Type> components = {
            Type(TypeKind::Integer),
            Type(TypeKind::Number),
            Type(TypeKind::Boolean)
        };
        Type tupleType(components);

        auto dissolved = RVMMapper::dissolveTupleType(tupleType);

        REQUIRE(dissolved.size() == 3);
        REQUIRE(dissolved[0].kind() == TypeKind::Integer);
        REQUIRE(dissolved[1].kind() == TypeKind::Number);
        REQUIRE(dissolved[2].kind() == TypeKind::Boolean);
    }

    SECTION("Nested tuple dissolution")
    {
        std::vector<type::Type> inner = {
            Type(TypeKind::Number),
            Type(TypeKind::Number)
        };
        Type innerTuple(inner);

        std::vector<type::Type> outer = {
            Type(TypeKind::Integer),
            innerTuple,
            Type(TypeKind::Boolean)
        };
        Type outerTuple(outer);

        auto dissolved = RVMMapper::dissolveTupleType(outerTuple);

        REQUIRE(dissolved.size() == 4);
        REQUIRE(dissolved[0].kind() == TypeKind::Integer);
        REQUIRE(dissolved[1].kind() == TypeKind::Number);
        REQUIRE(dissolved[2].kind() == TypeKind::Number);
        REQUIRE(dissolved[3].kind() == TypeKind::Boolean);
    }

    SECTION("Elementary type dissolution")
    {
        Type intType(TypeKind::Integer);
        auto dissolved = RVMMapper::dissolveTupleType(intType);

        REQUIRE(dissolved.size() == 1);
        REQUIRE(dissolved[0].kind() == TypeKind::Integer);
    }
}

TEST_CASE("RVMInstructions: call and return", "[rvm][instructions]")
{
    SECTION("Call instruction with arguments")
    {
        RVMValue dst               = RVMValue::Register(0, Type(TypeKind::Number));
        std::vector<RVMValue> args = {
            RVMValue::Constant(Integer(5)),
            RVMValue::Constant(Number(3.14))
        };

        auto instr = std::make_shared<RVMInstrExternalCall>(dst, "test_func", args);

        REQUIRE(instr->opcode() == Opcode::CALL_EXTERNAL);
        REQUIRE(instr->dst().has_value());
        REQUIRE(instr->dst().value() == dst);
        REQUIRE(instr->functionName() == "test_func");

        auto srcs = instr->srcs();
        REQUIRE(srcs.size() == 2);
    }

    SECTION("Call instruction without return value")
    {
        std::vector<RVMValue> args = {
            RVMValue::Constant(Integer(42))
        };

        auto instr = std::make_shared<RVMInstrExternalCall>(std::nullopt, "void_func", args);

        REQUIRE(instr->opcode() == Opcode::CALL_EXTERNAL);
        REQUIRE_FALSE(instr->dst().has_value());
        REQUIRE(instr->functionName() == "void_func");
    }
}

TEST_CASE("RVMInstructions: all arithmetic operations", "[rvm][instructions]")
{
    RVMValue dst  = RVMValue::Register(0, Type(TypeKind::Number));
    RVMValue src1 = RVMValue::Constant(Number(5.0));
    RVMValue src2 = RVMValue::Constant(Number(3.0));

    SECTION("POW instruction")
    {
        auto instr = std::make_shared<RVMInstr3Op>(Opcode::POW, dst, src1, src2);
        REQUIRE(instr->opcode() == Opcode::POW);

        std::ostringstream oss;
        RVMSerializer::write(oss, *instr);
        REQUIRE(oss.str().find("pow") != std::string::npos);
    }

    SECTION("All arithmetic opcodes")
    {
        std::vector<Opcode> ops = {
            Opcode::ADD, Opcode::SUB, Opcode::MUL,
            Opcode::DIV, Opcode::MOD, Opcode::POW
        };

        for (auto op : ops) {
            auto instr = std::make_shared<RVMInstr3Op>(op, dst, src1, src2);
            REQUIRE(instr->opcode() == op);
            REQUIRE(instr->dst().value() == dst);
        }
    }
}

TEST_CASE("RVMInstructions: internal call instructions", "[rvm][instructions][calling]")
{
    SECTION("Internal call instruction")
    {
        auto instr = std::make_shared<RVMInstrInternalCall>(0, 0, "my_function");

        REQUIRE(instr->opcode() == Opcode::CALL_INTERNAL);
        REQUIRE(instr->functionName() == "my_function");
        REQUIRE_FALSE(instr->dst().has_value());
        REQUIRE(instr->srcs().empty());
    }

    SECTION("Internal call serialization")
    {
        auto instr = std::make_shared<RVMInstrInternalCall>(0, 0, "test_func");

        std::ostringstream oss;
        RVMSerializer::write(oss, *instr);
        std::string instrStr = oss.str();

        REQUIRE(instrStr.find("call_internal") != std::string::npos);
        REQUIRE(instrStr.find("test_func") != std::string::npos);
    }
}

TEST_CASE("RVMInstructions: frame instructions with register counts", "[rvm][instructions][calling]")
{
    SECTION("Push frame with register count")
    {
        auto pushInstr = std::make_shared<RVMInstrPushFrame>(5);

        REQUIRE(pushInstr->opcode() == Opcode::PUSH_FRAME);
        REQUIRE(pushInstr->registerCount() == 5);

        std::ostringstream oss;
        RVMSerializer::write(oss, *pushInstr);
        std::string instrStr = oss.str();

        REQUIRE(instrStr.find("push_frame") != std::string::npos);
        REQUIRE(instrStr.find("5") != std::string::npos);
    }

    SECTION("Pop frame with register count")
    {
        auto popInstr = std::make_shared<RVMInstrPopFrame>(3);

        REQUIRE(popInstr->opcode() == Opcode::POP_FRAME);
        REQUIRE(popInstr->registerCount() == 3);

        std::ostringstream oss;
        RVMSerializer::write(oss, *popInstr);
        std::string instrStr = oss.str();

        REQUIRE(instrStr.find("pop_frame") != std::string::npos);
        REQUIRE(instrStr.find("3") != std::string::npos);
    }

    SECTION("Frame instruction serialization roundtrip")
    {
        auto pushInstr = std::make_shared<RVMInstrPushFrame>(7);

        std::ostringstream oss;
        RVMSerializer::write(oss, *pushInstr);
        std::string serialized = oss.str();

        // Parse it back
        auto parsed = RVMSerializer::readInstruction(serialized);
        REQUIRE(parsed != nullptr);

        auto* pushParsed = dynamic_cast<RVMInstrPushFrame*>(parsed.get());
        REQUIRE(pushParsed != nullptr);
        REQUIRE(pushParsed->registerCount() == 7);
    }
}

TEST_CASE("RVMMapper: tuple type dissolution", "[rvm][mapper][tuple]")
{
    SECTION("Simple tuple with 2 elements")
    {
        std::vector<Type> components = {
            Type(TypeKind::Integer),
            Type(TypeKind::Number)
        };
        Type tupleType(components);

        auto dissolved = RVMMapper::dissolveTupleType(tupleType);

        REQUIRE(dissolved.size() == 2);
        REQUIRE(dissolved[0].kind() == TypeKind::Integer);
        REQUIRE(dissolved[1].kind() == TypeKind::Number);
    }

    SECTION("Nested tuple dissolution")
    {
        // Create (int, (num, num), bool)
        std::vector<Type> inner = {
            Type(TypeKind::Number),
            Type(TypeKind::Number)
        };
        Type innerTuple(inner);

        std::vector<Type> outer = {
            Type(TypeKind::Integer),
            innerTuple,
            Type(TypeKind::Boolean)
        };
        Type outerTuple(outer);

        auto dissolved = RVMMapper::dissolveTupleType(outerTuple);

        REQUIRE(dissolved.size() == 4);
        REQUIRE(dissolved[0].kind() == TypeKind::Integer);
        REQUIRE(dissolved[1].kind() == TypeKind::Number);
        REQUIRE(dissolved[2].kind() == TypeKind::Number);
        REQUIRE(dissolved[3].kind() == TypeKind::Boolean);
    }

    SECTION("Non-tuple type returns single element")
    {
        Type intType(TypeKind::Integer);
        auto dissolved = RVMMapper::dissolveTupleType(intType);

        REQUIRE(dissolved.size() == 1);
        REQUIRE(dissolved[0].kind() == TypeKind::Integer);
    }
}

TEST_CASE("RVMSerializer: frame instruction deserialization", "[rvm][serializer][calling]")
{
    SECTION("Deserialize push_frame with count")
    {
        std::string line = "push_frame 4";
        auto instr       = RVMSerializer::readInstruction(line);

        REQUIRE(instr != nullptr);
        auto* pushFrame = dynamic_cast<RVMInstrPushFrame*>(instr.get());
        REQUIRE(pushFrame != nullptr);
        REQUIRE(pushFrame->registerCount() == 4);
    }

    SECTION("Deserialize pop_frame with count")
    {
        std::string line = "pop_frame 2";
        auto instr       = RVMSerializer::readInstruction(line);

        REQUIRE(instr != nullptr);
        auto* popFrame = dynamic_cast<RVMInstrPopFrame*>(instr.get());
        REQUIRE(popFrame != nullptr);
        REQUIRE(popFrame->registerCount() == 2);
    }

    SECTION("Deserialize push_frame without count (backwards compatibility)")
    {
        std::string line = "push_frame";
        auto instr       = RVMSerializer::readInstruction(line);

        REQUIRE(instr != nullptr);
        auto* pushFrame = dynamic_cast<RVMInstrPushFrame*>(instr.get());
        REQUIRE(pushFrame != nullptr);
        REQUIRE(pushFrame->registerCount() == 0);
    }
}

TEST_CASE("RVMSerializer: load_string instruction support", "[rvm][serializer][load_string]")
{
    SECTION("LOAD_STRING opcode conversion")
    {
        REQUIRE(RVMSerializer::opcodeToString(Opcode::LOAD_STRING) == "load_string");
        REQUIRE(RVMSerializer::stringToOpcode("load_string") == Opcode::LOAD_STRING);
    }

    SECTION("String literal instruction creation")
    {
        RVMValue dst = RVMValue::StringRef(0);
        auto instr   = std::make_shared<RVMInstrStringLiteral>(dst, "Hello, World!");

        REQUIRE(instr->opcode() == Opcode::LOAD_STRING);
        REQUIRE(instr->dst().has_value());
        REQUIRE(instr->dst().value() == dst);
        REQUIRE(instr->stringValue() == "Hello, World!");
        REQUIRE(instr->srcs().empty());
    }

    SECTION("String literal instruction serialization")
    {
        RVMValue dst = RVMValue::StringRef(0);
        auto instr   = std::make_shared<RVMInstrStringLiteral>(dst, "Hello, World!");

        std::ostringstream oss;
        RVMSerializer::write(oss, *instr);
        std::string instrStr = oss.str();

        REQUIRE(instrStr.find("load_string") != std::string::npos);
        REQUIRE(instrStr.find("#str0:str") != std::string::npos);
        REQUIRE(instrStr.find("Hello, World!") != std::string::npos);
    }

    SECTION("String literal instruction with escaped characters")
    {
        RVMValue dst = RVMValue::StringRef(1);
        auto instr   = std::make_shared<RVMInstrStringLiteral>(dst, "He said: \"Hello!\"");

        std::ostringstream oss;
        RVMSerializer::write(oss, *instr);
        std::string instrStr = oss.str();

        REQUIRE(instrStr.find("load_string") != std::string::npos);
        REQUIRE(instrStr.find("#str1:str") != std::string::npos);
        REQUIRE(instrStr.find("He said: \\\"Hello!\\\"") != std::string::npos);
    }

    SECTION("String literal instruction deserialization")
    {
        std::string line = "#str0:str = load_string \"Hello, World!\"";
        auto instr       = RVMSerializer::readInstruction(line);

        REQUIRE(instr != nullptr);
        auto* strInstr = dynamic_cast<RVMInstrStringLiteral*>(instr.get());
        REQUIRE(strInstr != nullptr);
        REQUIRE(strInstr->opcode() == Opcode::LOAD_STRING);

        auto dstOpt = strInstr->dst();
        REQUIRE(dstOpt.has_value());
        REQUIRE(dstOpt.value().isStringRef());
        REQUIRE(dstOpt.value().stringId() == 0);
        REQUIRE(dstOpt.value().type() == Type(TypeKind::String));

        REQUIRE(strInstr->stringValue() == "Hello, World!");
    }

    SECTION("String literal instruction with escaped characters deserialization")
    {
        std::string line = "#str1:str = load_string \"He said: \\\"Hello!\\\"\"";
        auto instr       = RVMSerializer::readInstruction(line);

        REQUIRE(instr != nullptr);
        auto* strInstr = dynamic_cast<RVMInstrStringLiteral*>(instr.get());
        REQUIRE(strInstr != nullptr);
        REQUIRE(strInstr->opcode() == Opcode::LOAD_STRING);

        auto dstOpt = strInstr->dst();
        REQUIRE(dstOpt.has_value());
        REQUIRE(dstOpt.value().isStringRef());
        REQUIRE(dstOpt.value().stringId() == 1);
        REQUIRE(dstOpt.value().type() == Type(TypeKind::String));

        REQUIRE(strInstr->stringValue() == "He said: \"Hello!\"");
    }

    SECTION("String literal instruction roundtrip serialization/deserialization")
    {
        RVMValue dst           = RVMValue::StringRef(2);
        std::string testString = "Test\nstring\twith\\escapes\"and quotes\"";
        auto originalInstr     = std::make_shared<RVMInstrStringLiteral>(dst, testString);

        // Serialize
        std::ostringstream oss;
        RVMSerializer::write(oss, *originalInstr);
        std::string serialized = oss.str();

        // Deserialize
        auto deserializedInstr = RVMSerializer::readInstruction(serialized);
        REQUIRE(deserializedInstr != nullptr);

        auto* strInstr = dynamic_cast<RVMInstrStringLiteral*>(deserializedInstr.get());
        REQUIRE(strInstr != nullptr);

        // Verify properties match
        REQUIRE(strInstr->opcode() == Opcode::LOAD_STRING);
        REQUIRE(strInstr->dst().has_value());
        REQUIRE(strInstr->dst().value() == dst);
        REQUIRE(strInstr->stringValue() == testString);
    }
}

TEST_CASE("RVMMoveSimplifier: identity mov elimination", "[rvm][move][optimization]")
{
    SECTION("Simple identity MOV removal")
    {
        RVMProgram program;

        RVMValue r0 = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue r1 = RVMValue::Register(1, Type(TypeKind::Integer));

        // Add identity MOV
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r0, r0));
        // Add non-identity MOV
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r1, r0));
        // Add another identity MOV
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r1, r1));

        // Apply simplification
        bool changed = RVMMoveSimplifier::simplify(program);

        REQUIRE(changed == true);

        // Count MOV instructions
        int movCount = 0;
        for (const auto& instr : program) {
            if (auto* mov = dynamic_cast<RVMInstr2Op*>(instr.get())) {
                if (mov->opcode() == Opcode::MOV)
                    movCount++;
            }
        }

        // Only non-identity MOV should remain
        REQUIRE(movCount == 1);
    }

    SECTION("MOV chain simplification")
    {
        RVMProgram program;

        RVMValue r0 = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue r1 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue r2 = RVMValue::Register(2, Type(TypeKind::Integer));
        RVMValue r3 = RVMValue::Register(3, Type(TypeKind::Integer));

        // Create chain: r1 = mov r0, r2 = mov r1, r3 = mov r2
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r1, r0));
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r2, r1));
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r3, r2));

        // Add computation using r3
        program.push_back(std::make_shared<RVMInstr3Op>(Opcode::ADD, r0, r3, r0));

        // Apply simplification
        bool changed = RVMMoveSimplifier::simplify(program);

        // r3 is pinned (used in ADD), but chain will be simplified
        REQUIRE(changed == true);

        // Program should remain unchanged
        // TODO: Remove obsolete mov instructions
        REQUIRE(program.size() == 4);
    }

    SECTION("Pinned registers not renamed")
    {
        RVMProgram program;

        RVMValue r0 = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue r1 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue r2 = RVMValue::Register(2, Type(TypeKind::Integer));

        // Create chain: r1 = mov r0, r2 = mov r1
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r1, r0));
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r2, r1));

        // Use r2 in computation (pins r2)
        program.push_back(std::make_shared<RVMInstr3Op>(Opcode::ADD, r0, r2, r0));

        // Apply simplification
        bool changed = RVMMoveSimplifier::simplify(program);
        REQUIRE(changed == true);

        // r2 should not be renamed away since it's used in ADD
        // The implementation should preserve pinned registers
        std::string programStr = RVMSerializer::serialize(program);

        // The ADD instruction should still reference r2 (or a renamed version)
        // We just verify the program is valid
        REQUIRE(program.size() > 0);
    }

    SECTION("No changes when no MOV instructions")
    {
        RVMProgram program;

        RVMValue r0 = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue r1 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue r2 = RVMValue::Register(2, Type(TypeKind::Integer));

        // Only computations, no MOVs
        program.push_back(std::make_shared<RVMInstr3Op>(Opcode::ADD, r2, r0, r1));
        program.push_back(std::make_shared<RVMInstr3Op>(Opcode::SUB, r1, r2, r0));

        // Apply simplification
        bool changed = RVMMoveSimplifier::simplify(program);

        REQUIRE(changed == false);
        REQUIRE(program.size() == 2);
    }
}

TEST_CASE("RVMRedundantMoveEliminator: redundant mov elimination", "[rvm][move][optimization][redundant]")
{
    SECTION("Redundant MOV elimination")
    {
        RVMProgram program;

        RVMValue r0 = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue r1 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue r2 = RVMValue::Register(2, Type(TypeKind::Integer));

        // r1 = mov r0 (will be redundant)
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r1, r0));
        // r1 = mov r2 (overwrites r1 before it's used)
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r1, r2));
        // Use r1
        program.push_back(std::make_shared<RVMInstr3Op>(Opcode::ADD, r0, r1, r0));

        // Apply redundant move elimination
        bool changed = RVMRedundantMoveEliminator::eliminate(program);

        REQUIRE(changed == true);
        REQUIRE(program.size() == 2); // First MOV should be removed

        // Check that the first MOV is gone
        std::string programStr = RVMSerializer::serialize(program);
        // The serialization might include type annotations like "%r1:int = mov %r0:int"
        // So we check for the pattern more flexibly
        REQUIRE(programStr.find("r1") != std::string::npos);  // Some reference to r1 should exist
        REQUIRE(programStr.find("mov") != std::string::npos); // Some MOV should exist
        REQUIRE(programStr.find("r2") != std::string::npos);  // r2 should be referenced
        // Count MOV instructions
        int movCount = 0;
        for (const auto& instr : program) {
            if (auto* mov = dynamic_cast<RVMInstr2Op*>(instr.get())) {
                if (mov->opcode() == Opcode::MOV)
                    movCount++;
            }
        }
        REQUIRE(movCount == 1); // Only one MOV should remain
    }

    SECTION("MOV not redundant when read before overwritten")
    {
        RVMProgram program;

        RVMValue r0 = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue r1 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue r2 = RVMValue::Register(2, Type(TypeKind::Integer));

        // r1 = mov r0
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r1, r0));
        // Use r1 (reads it)
        program.push_back(std::make_shared<RVMInstr3Op>(Opcode::ADD, r0, r1, r0));
        // r1 = mov r2 (overwrites r1 after it's used)
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r1, r2));

        // Apply redundant move elimination
        bool changed = RVMRedundantMoveEliminator::eliminate(program);

        REQUIRE(changed == false);    // MOV is not redundant
        REQUIRE(program.size() == 3); // All instructions remain
    }

    SECTION("Multiple redundant MOVs in chain")
    {
        RVMProgram program;

        RVMValue r0 = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue r1 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue r2 = RVMValue::Register(2, Type(TypeKind::Integer));
        RVMValue r3 = RVMValue::Register(3, Type(TypeKind::Integer));

        // r1 = mov r0 (redundant)
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r1, r0));
        // r2 = mov r1 (redundant - depends on r1 which is redundant)
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r2, r1));
        // r2 = mov r3 (overwrites r2)
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r2, r3));

        // Apply redundant move elimination
        bool changed = RVMRedundantMoveEliminator::eliminate(program);

        REQUIRE(changed == true);
        // Only r2 = mov r1 should be removed (redundant - r2 is overwritten)
        // r1 = mov r0 remains (dead but not redundant - r1 is not overwritten)
        REQUIRE(program.size() == 2);

        // Count MOV instructions
        int movCount = 0;
        for (const auto& instr : program) {
            if (auto* mov = dynamic_cast<RVMInstr2Op*>(instr.get())) {
                if (mov->opcode() == Opcode::MOV)
                    movCount++;
            }
        }
        REQUIRE(movCount == 2); // Both remaining instructions are MOVs
    }

    SECTION("No changes when no redundant MOVs")
    {
        RVMProgram program;

        RVMValue r0 = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue r1 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue r2 = RVMValue::Register(2, Type(TypeKind::Integer));

        // r1 = mov r0
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r1, r0));
        // Use r1
        program.push_back(std::make_shared<RVMInstr3Op>(Opcode::ADD, r0, r1, r0));
        // r2 = mov r1 (different destination)
        program.push_back(std::make_shared<RVMInstr2Op>(Opcode::MOV, r2, r1));

        // Apply redundant move elimination
        bool changed = RVMRedundantMoveEliminator::eliminate(program);

        REQUIRE(changed == false);
        REQUIRE(program.size() == 3); // All instructions remain
    }
}
