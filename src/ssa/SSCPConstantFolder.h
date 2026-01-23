#pragma once

#include "SSAMapper.h"

#include <memory>
#include <optional>
#include <unordered_map>

namespace PExpr::ssa {

class SSCPConstantFolder {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    [[nodiscard]] bool replaceOperandIfConst(InstructionList& instructions);
    [[nodiscard]] bool foldToConstants(bool foldNumber, InstructionList& instructions);

private:
    [[nodiscard]] bool replaceOperandIfConst(SSAValue& op);

    [[nodiscard]] std::optional<SSAValue> foldAssign(bool foldNumber, const SSAInstrAssign* asg);
    [[nodiscard]] std::optional<SSAValue> foldUnaryOp(const SSAValue& operand, UnaryOperation unaryOp);
    [[nodiscard]] std::optional<SSAValue> foldBinaryOp(bool foldNumber, const SSAValue& L, const SSAValue& R, BinaryOperation binaryOp);
    [[nodiscard]] std::optional<SSAValue> foldSwizzleOp(const SSAValue& operand, const std::string& swizzle);
    [[nodiscard]] std::optional<SSAValue> foldAccessOp(const SSAValue& operand, const SSAValue& index);
    [[nodiscard]] std::optional<SSAValue> foldVectorOp(const std::vector<SSAValue>& operands);
    [[nodiscard]] std::optional<SSAValue> foldCastOp(const SSAValue& operand, ElementaryType targetType);

    std::unordered_map<std::string, SSAValue> mConstants;

    [[nodiscard]] static bool extractBool(const SSAValue& vv, bool& out);
    [[nodiscard]] static bool extractInteger(const SSAValue& vv, Integer& out);
    [[nodiscard]] static bool extractNumber(const SSAValue& vv, Number& out);
    [[nodiscard]] static bool extractString(const SSAValue& vv, std::string& out);
    [[nodiscard]] static bool extractVecN(const SSAValue& vv, VecN& out);
};

} // namespace PExpr::ssa
