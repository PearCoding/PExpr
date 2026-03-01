#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "rvm/RVMSerializer.h"
#include "rvm/RVMStructs.h"
#include "rvm/RVMValue.h"
#include "type/Type.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;

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
        REQUIRE(instr->destination() == dst);
        REQUIRE(instr->stringValue() == "Hello, World!");
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
        std::string line = "load_string #str0:str \"Hello, World!\"";
        auto instr       = RVMSerializer::readInstruction(line);

        REQUIRE(instr != nullptr);
        auto* strInstr = dynamic_cast<RVMInstrStringLiteral*>(instr.get());
        REQUIRE(strInstr != nullptr);
        REQUIRE(strInstr->opcode() == Opcode::LOAD_STRING);

        const RVMValue& dst = strInstr->destination();
        REQUIRE(dst.isStringRef());
        REQUIRE(dst.stringId() == 0);
        REQUIRE(dst.type() == Type(TypeKind::String));

        REQUIRE(strInstr->stringValue() == "Hello, World!");
    }

    SECTION("String literal instruction with escaped characters deserialization")
    {
        std::string line = "load_string #str1:str \"He said: \\\"Hello!\\\"\"";
        auto instr       = RVMSerializer::readInstruction(line);

        REQUIRE(instr != nullptr);
        auto* strInstr = dynamic_cast<RVMInstrStringLiteral*>(instr.get());
        REQUIRE(strInstr != nullptr);
        REQUIRE(strInstr->opcode() == Opcode::LOAD_STRING);

        const RVMValue& dst = strInstr->destination();
        REQUIRE(dst.isStringRef());
        REQUIRE(dst.stringId() == 1);
        REQUIRE(dst.type() == Type(TypeKind::String));

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
        REQUIRE(strInstr->destination() == dst);
        REQUIRE(strInstr->stringValue() == testString);
    }
}

TEST_CASE("RVMSerializer: comprehensive roundtrip tests", "[rvm][serializer][roundtrip]")
{
    SECTION("2-operand instruction roundtrip")
    {
        RVMValue dst = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue src = RVMValue::Constant(Integer(42));

        auto original          = std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src);
        std::string serialized = RVMSerializer::serialize({ original });

        auto parsed = RVMSerializer::readInstruction(serialized);
        REQUIRE(parsed != nullptr);

        auto* parsedInstr = dynamic_cast<RVMInstr2Op*>(parsed.get());
        REQUIRE(parsedInstr != nullptr);
        REQUIRE(parsedInstr->opcode() == Opcode::MOV);
        REQUIRE(parsedInstr->destination() == dst);
        REQUIRE(parsedInstr->source() == src);
    }

    SECTION("3-operand instruction roundtrip")
    {
        RVMValue dst  = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue src1 = RVMValue::Constant(Integer(10));
        RVMValue src2 = RVMValue::Constant(Integer(20));

        auto original          = std::make_shared<RVMInstr3Op>(Opcode::ADD, dst, src1, src2);
        std::string serialized = RVMSerializer::serialize({ original });

        auto parsed = RVMSerializer::readInstruction(serialized);
        REQUIRE(parsed != nullptr);

        auto* parsedInstr = dynamic_cast<RVMInstr3Op*>(parsed.get());
        REQUIRE(parsedInstr != nullptr);
        REQUIRE(parsedInstr->opcode() == Opcode::ADD);
        REQUIRE(parsedInstr->destination() == dst);
        REQUIRE(parsedInstr->source1() == src1);
        REQUIRE(parsedInstr->source2() == src2);
    }

    SECTION("Branch instruction roundtrip")
    {
        RVMValue src      = RVMValue::Constant(true);
        std::string label = "test_label";

        auto original          = std::make_shared<RVMInstrBranch>(Opcode::JZ, src, label);
        std::string serialized = RVMSerializer::serialize({ original });

        auto parsed = RVMSerializer::readInstruction(serialized);
        REQUIRE(parsed != nullptr);

        auto* parsedInstr = dynamic_cast<RVMInstrBranch*>(parsed.get());
        REQUIRE(parsedInstr != nullptr);
        REQUIRE(parsedInstr->opcode() == Opcode::JZ);
        REQUIRE(parsedInstr->condition() == src);
        REQUIRE(parsedInstr->targetLabel() == label);
    }

    SECTION("Jump instruction roundtrip")
    {
        std::string label = "loop_start";

        auto original          = std::make_shared<RVMInstrJump>(label);
        std::string serialized = RVMSerializer::serialize({ original });

        auto parsed = RVMSerializer::readInstruction(serialized);
        REQUIRE(parsed != nullptr);

        auto* parsedInstr = dynamic_cast<RVMInstrJump*>(parsed.get());
        REQUIRE(parsedInstr != nullptr);
        REQUIRE(parsedInstr->opcode() == Opcode::JMP);
        REQUIRE(parsedInstr->targetLabel() == label);
    }

    SECTION("Label instruction roundtrip")
    {
        std::string label = "my_label";

        auto original          = std::make_shared<RVMInstrLabel>(label);
        std::string serialized = RVMSerializer::serialize({ original });

        auto parsed = RVMSerializer::readInstruction(serialized);
        REQUIRE(parsed != nullptr);

        auto* parsedInstr = dynamic_cast<RVMInstrLabel*>(parsed.get());
        REQUIRE(parsedInstr != nullptr);
        REQUIRE(parsedInstr->labelName() == label);
    }

    SECTION("Return instruction roundtrip")
    {
        size_t returnCount = 2;

        auto original          = std::make_shared<RVMInstrReturn>(returnCount);
        std::string serialized = RVMSerializer::serialize({ original });

        auto parsed = RVMSerializer::readInstruction(serialized);
        REQUIRE(parsed != nullptr);

        auto* parsedInstr = dynamic_cast<RVMInstrReturn*>(parsed.get());
        REQUIRE(parsedInstr != nullptr);
        REQUIRE(parsedInstr->opcode() == Opcode::RET);
        REQUIRE(parsedInstr->returnCount() == returnCount);
    }

    SECTION("Call instruction roundtrip")
    {
        size_t paramCount    = 2;
        size_t returnCount   = 1;
        std::string funcName = "test_func";

        auto original          = std::make_shared<RVMInstrCall>(false, paramCount, returnCount, funcName);
        std::string serialized = RVMSerializer::serialize({ original });

        auto parsed = RVMSerializer::readInstruction(serialized);
        REQUIRE(parsed != nullptr);

        auto* parsedInstr = dynamic_cast<RVMInstrCall*>(parsed.get());
        REQUIRE(parsedInstr != nullptr);
        REQUIRE(parsedInstr->opcode() == Opcode::CALL_INTERNAL);
        REQUIRE(parsedInstr->functionName() == funcName);
        REQUIRE(parsedInstr->parameterCount() == paramCount);
        REQUIRE(parsedInstr->returnCount() == returnCount);
    }

    SECTION("All opcode types roundtrip")
    {
        // Test all 2-operand opcodes
        std::vector<Opcode> twoOpOpcodes = { Opcode::MOV, Opcode::I2F, Opcode::F2I };
        for (auto opcode : twoOpOpcodes) {
            RVMValue dst = RVMValue::Register(0, Type(TypeKind::Integer));
            RVMValue src = RVMValue::Constant(Integer(42));

            auto original          = std::make_shared<RVMInstr2Op>(opcode, dst, src);
            std::string serialized = RVMSerializer::serialize({ original });

            auto parsed = RVMSerializer::readInstruction(serialized);
            REQUIRE(parsed != nullptr);

            auto* parsedInstr = dynamic_cast<RVMInstr2Op*>(parsed.get());
            REQUIRE(parsedInstr != nullptr);
            REQUIRE(parsedInstr->opcode() == opcode);
        }

        // Test all 3-operand opcodes
        std::vector<Opcode> threeOpOpcodes = {
            Opcode::ADD, Opcode::SUB, Opcode::MUL, Opcode::DIV, Opcode::MOD, Opcode::POW,
            Opcode::AND, Opcode::OR, Opcode::XOR, Opcode::SHL, Opcode::SHR,
            Opcode::CMP_EQ, Opcode::CMP_NE, Opcode::CMP_LT, Opcode::CMP_LE, Opcode::CMP_GT, Opcode::CMP_GE
        };
        for (auto opcode : threeOpOpcodes) {
            RVMValue dst  = RVMValue::Register(0, Type(TypeKind::Integer));
            RVMValue src1 = RVMValue::Constant(Integer(10));
            RVMValue src2 = RVMValue::Constant(Integer(20));

            auto original          = std::make_shared<RVMInstr3Op>(opcode, dst, src1, src2);
            std::string serialized = RVMSerializer::serialize({ original });

            auto parsed = RVMSerializer::readInstruction(serialized);
            REQUIRE(parsed != nullptr);

            auto* parsedInstr = dynamic_cast<RVMInstr3Op*>(parsed.get());
            REQUIRE(parsedInstr != nullptr);
            REQUIRE(parsedInstr->opcode() == opcode);
        }
    }
}

TEST_CASE("RVMSerializer: error handling for ill-formed input", "[rvm][serializer][error]")
{
    SECTION("Invalid opcode")
    {
        std::string line = "invalid_op %r0:int 42:int";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Missing destination in 2-operand instruction")
    {
        std::string line = "mov 42:int";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Missing source in 2-operand instruction")
    {
        std::string line = "mov %r0:int";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Missing second source in 3-operand instruction")
    {
        std::string line = "add %r0:int 10:int";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Invalid register format")
    {
        std::string line = "mov %invalid 42:int";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Invalid constant format")
    {
        std::string line = "mov %r0:int not_a_number:int";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Missing type annotation")
    {
        std::string line = "mov %r0 42";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Branch instruction missing label")
    {
        std::string line = "jz true:bool";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Branch instruction missing value")
    {
        std::string line = "jz label";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Jump instruction missing label")
    {
        std::string line = "jmp";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Invalid push_frame format")
    {
        std::string line = "push_frame";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Invalid pop_frame format")
    {
        std::string line = "pop_frame";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Invalid ret format")
    {
        std::string line = "ret";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Invalid call_internal format")
    {
        std::string line = "call_internal";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Invalid call_external format")
    {
        std::string line = "call_external";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Invalid load_string format - missing string")
    {
        std::string line = "load_string #str0:str";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Invalid load_string format - missing destination")
    {
        std::string line = "load_string \"Hello\"";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Malformed comment")
    {
        std::string line = "// This is a comment";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr); // Comments should be ignored, returning nullptr
    }

    SECTION("Empty line")
    {
        std::string line = "";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }

    SECTION("Whitespace only")
    {
        std::string line = "   \t\n";
        auto instr       = RVMSerializer::readInstruction(line);
        REQUIRE(instr == nullptr);
    }
}