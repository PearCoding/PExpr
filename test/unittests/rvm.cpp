#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

#include "rvm/RVMSerializer.h"
#include "rvm/RVMValue.h"
#include "rvm/RVMContext.h"
#include "rvm/RVMMapper.h"
#include "rvm/RVMStructs.h"

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
        auto stringTable = std::make_shared<RVMStringTable>();
        uint32_t strId   = stringTable->addString("Hello");

        RVMValue strValue = RVMValue::StringRef(strId, Type(TypeKind::String));
        REQUIRE(strValue.isStringRef());
        REQUIRE(strValue.type() == Type(TypeKind::String));
        REQUIRE(strValue.stringId() == strId);
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

TEST_CASE("RVMStringTable: basic operations", "[rvm][stringtable]")
{
    auto stringTable = std::make_shared<RVMStringTable>();

    SECTION("Adding strings")
    {
        uint32_t id1 = stringTable->addString("Hello");
        uint32_t id2 = stringTable->addString("World");
        uint32_t id3 = stringTable->addString("Hello"); // Duplicate

        REQUIRE(id1 == 0u);
        REQUIRE(id2 == 1u);
        REQUIRE(id3 == id1); // Should return existing ID
    }

    SECTION("Retrieving strings")
    {
        uint32_t id1 = stringTable->addString("Hello");
        uint32_t id2 = stringTable->addString("World");

        REQUIRE(stringTable->getString(id1) == "Hello");
        REQUIRE(stringTable->getString(id2) == "World");
    }

    SECTION("String lookup")
    {
        uint32_t id1 = stringTable->addString("Hello");
        uint32_t id2 = stringTable->addString("World");

        REQUIRE(stringTable->contains("Hello"));
        REQUIRE(stringTable->contains("World"));
        REQUIRE_FALSE(stringTable->contains("Test"));

        REQUIRE(stringTable->getId("Hello") == id1);
        REQUIRE(stringTable->getId("World") == id2);
        REQUIRE_FALSE(stringTable->getId("Test").has_value());
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
        REQUIRE(RVMSerializer::opcodeToString(Opcode::B2I) == "b2i");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::I2B) == "i2b");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::F2B) == "f2b");
        REQUIRE(RVMSerializer::opcodeToString(Opcode::B2F) == "b2f");

        REQUIRE(RVMSerializer::stringToOpcode("i2f") == Opcode::I2F);
        REQUIRE(RVMSerializer::stringToOpcode("f2i") == Opcode::F2I);
        REQUIRE(RVMSerializer::stringToOpcode("b2i") == Opcode::B2I);
        REQUIRE(RVMSerializer::stringToOpcode("i2b") == Opcode::I2B);
        REQUIRE(RVMSerializer::stringToOpcode("f2b") == Opcode::F2B);
        REQUIRE(RVMSerializer::stringToOpcode("b2f") == Opcode::B2F);
    }
}

TEST_CASE("RVMSerializer: function serialization", "[rvm][serializer]")
{
    SECTION("Simple function serialization")
    {
        RVMFunction func;
        func.name = "test_func";
        func.parameters.push_back(Type(TypeKind::Integer));
        func.parameters.push_back(Type(TypeKind::Number));
        func.returnType = Type(TypeKind::Boolean);
        func.external = false;

        // Add a simple instruction
        RVMValue dst = RVMValue::Register(0, Type(TypeKind::Boolean));
        RVMValue src1 = RVMValue::Register(1, Type(TypeKind::Integer));
        RVMValue src2 = RVMValue::Register(2, Type(TypeKind::Number));
        func.body.push_back(std::make_shared<RVMInstr3Op>(Opcode::CMP_EQ, dst, src1, src2));

        std::ostringstream oss;
        RVMSerializer::write(oss, func);
        std::string funcStr = oss.str();

        REQUIRE(funcStr.find("fn test_func") != std::string::npos);
        REQUIRE(funcStr.find("%p0:int") != std::string::npos);
        REQUIRE(funcStr.find("%p1:num") != std::string::npos);
        REQUIRE(funcStr.find(": bool") != std::string::npos);
        REQUIRE(funcStr.find("cmp_eq") != std::string::npos);
        REQUIRE(funcStr.find("endfn") != std::string::npos);
    }

    SECTION("External function serialization")
    {
        RVMFunction func;
        func.name = "external_func";
        func.parameters.push_back(Type(TypeKind::Number));
        func.returnType = Type(TypeKind::Number);
        func.external = true;
        func.hasSideEffect = false;

        std::ostringstream oss;
        RVMSerializer::write(oss, func);
        std::string funcStr = oss.str();

        REQUIRE(funcStr.find("[[extern") != std::string::npos);
        REQUIRE(funcStr.find("pure") != std::string::npos);
        REQUIRE(funcStr.find("fn external_func") != std::string::npos);
    }
}

TEST_CASE("RVMSerializer: program serialization with string table", "[rvm][serializer]")
{
    SECTION("Program with string table")
    {
        RVMProgram program;
        program.stringTable = std::make_shared<RVMStringTable>();
        program.stringTable->addString("Hello");
        program.stringTable->addString("World");

        std::ostringstream oss;
        RVMSerializer::write(oss, program);
        std::string progStr = oss.str();

        REQUIRE(progStr.find("[[strings]]") != std::string::npos);
        REQUIRE(progStr.find("#str0") != std::string::npos);
        REQUIRE(progStr.find("\"Hello\"") != std::string::npos);
        REQUIRE(progStr.find("#str1") != std::string::npos);
        REQUIRE(progStr.find("\"World\"") != std::string::npos);
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
        RVMValue dst = RVMValue::Register(0, Type(TypeKind::Number));
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

    SECTION("Return instruction with value")
    {
        RVMValue retVal = RVMValue::Constant(Integer(42));
        auto instr = std::make_shared<RVMInstrReturn>(retVal);

        REQUIRE(instr->opcode() == Opcode::RET);
        REQUIRE(instr->returnValue().has_value());
        REQUIRE(instr->returnValue().value() == retVal);
    }

    SECTION("Return instruction without value")
    {
        auto instr = std::make_shared<RVMInstrReturn>(std::nullopt);

        REQUIRE(instr->opcode() == Opcode::RET);
        REQUIRE_FALSE(instr->returnValue().has_value());
    }
}

TEST_CASE("RVMInstructions: all arithmetic operations", "[rvm][instructions]")
{
    RVMValue dst = RVMValue::Register(0, Type(TypeKind::Number));
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
        auto instr = std::make_shared<RVMInstrInternalCall>("my_function");
        
        REQUIRE(instr->opcode() == Opcode::CALL_INTERNAL);
        REQUIRE(instr->functionName() == "my_function");
        REQUIRE_FALSE(instr->dst().has_value());
        REQUIRE(instr->srcs().empty());
    }

    SECTION("Internal call serialization")
    {
        auto instr = std::make_shared<RVMInstrInternalCall>("test_func");
        
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
        auto instr = RVMSerializer::readInstruction(line);
        
        REQUIRE(instr != nullptr);
        auto* pushFrame = dynamic_cast<RVMInstrPushFrame*>(instr.get());
        REQUIRE(pushFrame != nullptr);
        REQUIRE(pushFrame->registerCount() == 4);
    }

    SECTION("Deserialize pop_frame with count")
    {
        std::string line = "pop_frame 2";
        auto instr = RVMSerializer::readInstruction(line);
        
        REQUIRE(instr != nullptr);
        auto* popFrame = dynamic_cast<RVMInstrPopFrame*>(instr.get());
        REQUIRE(popFrame != nullptr);
        REQUIRE(popFrame->registerCount() == 2);
    }

    SECTION("Deserialize push_frame without count (backwards compatibility)")
    {
        std::string line = "push_frame";
        auto instr = RVMSerializer::readInstruction(line);
        
        REQUIRE(instr != nullptr);
        auto* pushFrame = dynamic_cast<RVMInstrPushFrame*>(instr.get());
        REQUIRE(pushFrame != nullptr);
        REQUIRE(pushFrame->registerCount() == 0);
    }
}
