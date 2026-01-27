#pragma once

#include "ast/Enums.h"
#include "type/Type.h"

#include <memory>
#include <optional>
#include <unordered_map>

namespace PExpr::ssa {
class SSAInstr;
class SSAInstrAssign;
class SSAValue;
} // namespace PExpr::ssa

namespace PExpr::opt {

class SSCPConstantFolder {
public:
    using InstructionList = std::vector<std::shared_ptr<ssa::SSAInstr>>;

    [[nodiscard]] bool replaceOperandIfConst(InstructionList& instructions);
    [[nodiscard]] bool foldToConstants(bool foldNumber, InstructionList& instructions);

private:
    [[nodiscard]] bool replaceOperandIfConst(ssa::SSAValue& op);

    [[nodiscard]] std::optional<ssa::SSAValue> foldAssign(bool foldNumber, const ssa::SSAInstrAssign* asg);
    [[nodiscard]] std::optional<ssa::SSAValue> foldUnaryOp(const ssa::SSAValue& operand, ast::UnaryOperation unaryOp);
    [[nodiscard]] std::optional<ssa::SSAValue> foldBinaryOp(bool foldNumber, const ssa::SSAValue& L, const ssa::SSAValue& R, ast::BinaryOperation binaryOp);
    [[nodiscard]] std::optional<ssa::SSAValue> foldSwizzleOp(const ssa::SSAValue& operand, const std::string& swizzle);
    [[nodiscard]] std::optional<ssa::SSAValue> foldAccessOp(const ssa::SSAValue& operand, const ssa::SSAValue& index);
    [[nodiscard]] std::optional<ssa::SSAValue> foldVectorOp(const std::vector<ssa::SSAValue>& operands);
    [[nodiscard]] std::optional<ssa::SSAValue> foldCastOp(const ssa::SSAValue& operand, const type::Type& targetType);

    std::unordered_map<std::string, ssa::SSAValue> mConstants;

    [[nodiscard]] static bool extractBool(const ssa::SSAValue& vv, bool& out);
    [[nodiscard]] static bool extractInteger(const ssa::SSAValue& vv, Integer& out);
    [[nodiscard]] static bool extractNumber(const ssa::SSAValue& vv, Number& out);
    [[nodiscard]] static bool extractString(const ssa::SSAValue& vv, std::string& out);
    [[nodiscard]] static bool extractVecN(const ssa::SSAValue& vv, std::vector<Number>& out);
};

} // namespace PExpr::opt
