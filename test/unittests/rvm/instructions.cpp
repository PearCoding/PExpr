#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <sstream>
#include <vector>

#include "rvm/RVMInstruction.h"
#include "rvm/RVMStructs.h"
#include "rvm/RVMValue.h"
#include "rvm/RVMSerializer.h"
#include "type/Type.h"

using namespace PExpr;
using namespace PExpr::rvm;
using namespace PExpr::type;

TEST_CASE("RVMInstructions: creation and properties", "[rvm][instructions]")
{
    SECTION("Three operand instruction")
    {
        RVMValue dst  = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue src1 = RVMValue::Constant(Integer(10));
        RVMValue src2 = RVMValue::Constant(Integer(20));

        auto instr = std::make_shared<RVMInstr3Op>(Opcode::ADD, dst, src1, src2);

        REQUIRE(instr->opcode() == Opcode::ADD);
        REQUIRE(instr->destination() == dst);
        REQUIRE(instr->source1() == src1);
        REQUIRE(instr->source2() == src2);
    }

    SECTION("Move instruction")
    {
        RVMValue dst = RVMValue::Register(0, Type(TypeKind::Integer));
        RVMValue src = RVMValue::Register(1, Type(TypeKind::Integer));

        auto instr = std::make_shared<RVMInstr2Op>(Opcode::MOV, dst, src);

        REQUIRE(instr->opcode() == Opcode::MOV);
        REQUIRE(instr->destination() == dst);
        REQUIRE(instr->source() == src);
    }

    SECTION("Branch instruction")
    {
        RVMValue src = RVMValue::Constant(true);
        auto instr   = std::make_shared<RVMInstrBranch>(Opcode::JZ, src, "target_label");

        REQUIRE(instr->opcode() == Opcode::JZ);
        REQUIRE(instr->condition() == src);
        REQUIRE(instr->targetLabel() == "target_label");
    }

    SECTION("Jump instruction")
    {
        auto instr = std::make_shared<RVMInstrJump>("loop_start");

        REQUIRE(instr->opcode() == Opcode::JMP);
        REQUIRE(instr->targetLabel() == "loop_start");
    }

    SECTION("Label instruction")
    {
        auto instr = std::make_shared<RVMInstrLabel>("my_label");

        REQUIRE(instr->labelName() == "my_label");
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
            REQUIRE(instr->destination() == dst);
        }
    }
}

TEST_CASE("RVMInstructions: call instructions", "[rvm][instructions][calling]")
{
    SECTION("Internal call instruction")
    {
        auto instr = std::make_shared<RVMInstrCall>(false, 0, 0, "my_function");

        REQUIRE(instr->opcode() == Opcode::CALL_INTERNAL);
        REQUIRE(instr->functionName() == "my_function");
    }

    SECTION("Internal call serialization")
    {
        auto instr = std::make_shared<RVMInstrCall>(false, 0, 0, "test_func");

        std::ostringstream oss;
        RVMSerializer::write(oss, *instr);
        std::string instrStr = oss.str();

        REQUIRE(instrStr.find("call_internal") != std::string::npos);
        REQUIRE(instrStr.find("test_func") != std::string::npos);
    }

    SECTION("External call serialization")
    {
        auto instr = std::make_shared<RVMInstrCall>(true, 0, 0, "test_func");

        std::ostringstream oss;
        RVMSerializer::write(oss, *instr);
        std::string instrStr = oss.str();

        REQUIRE(instrStr.find("call_external") != std::string::npos);
        REQUIRE(instrStr.find("test_func") != std::string::npos);
    }
}