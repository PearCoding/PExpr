#pragma once

#include "SSAMapper.h"

#include <memory>
#include <optional>
#include <unordered_map>

namespace PExpr::ssa {

class SSCPConstantFolder {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    bool replaceOperandIfConst(InstructionList& instructions);
    bool foldToConstants(bool foldNumber, InstructionList& instructions);

private:
    bool replaceOperandIfConst(std::vector<SSAValue>& ops);

    std::optional<SSAValue> foldAssign(bool foldNumber, const SSAInstrAssign* asg);
    std::optional<SSAValue> foldUnaryOp(bool foldNumber, const SSAValue& operand, UnaryOperation unaryOp);
    std::optional<SSAValue> foldBinaryOp(bool foldNumber, const SSAValue& L, const SSAValue& R, BinaryOperation binaryOp);
    std::optional<SSAValue> foldAccessOp(const SSAValue& operand, const std::string& swizzle);
    std::optional<SSAValue> foldVectorOp(const std::vector<SSAValue>& operands);
    std::optional<SSAValue> foldCastOp(const SSAValue& operand, ElementaryType targetType);

    std::unordered_map<std::string, SSAValue> mConstants;

    static bool extractBool(const SSAValue& vv, bool& out);
    static bool extractInteger(const SSAValue& vv, Integer& out);
    static bool extractNumber(const SSAValue& vv, Number& out);
    static bool extractString(const SSAValue& vv, std::string& out);
    static bool extractVec2(const SSAValue& vv, Vec2& out);
    static bool extractVec3(const SSAValue& vv, Vec3& out);
    static bool extractVec4(const SSAValue& vv, Vec4& out);
};

} // namespace PExpr::ssa
