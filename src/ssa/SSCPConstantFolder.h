#pragma once

#include "SSAMapper.h"

#include <optional>
#include <unordered_map>
#include <memory>

namespace PExpr::ssa {

class SSCPConstantFolder {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;
    
    SSCPConstantFolder(std::unordered_map<std::string, SSAValue>& constants);
    
    std::optional<SSAValue> foldAssign(const SSAInstrAssign* asg);
    std::optional<SSAValue> foldUnaryOp(const SSAValue& operand, UnaryOperation unaryOp);
    std::optional<SSAValue> foldBinaryOp(const SSAValue& L, const SSAValue& R, BinaryOperation binaryOp);
    std::optional<SSAValue> foldAccessOp(const SSAValue& operand, const std::string& swizzle);
    std::optional<SSAValue> foldVectorOp(const std::vector<SSAValue>& operands);
    std::optional<SSAValue> foldCastOp(const SSAValue& operand, ElementaryType targetType);
    
    bool replaceOperandIfConst(std::vector<SSAValue>& ops);
    bool replaceOperandIfConst(InstructionList& instructions);
    
private:
    std::unordered_map<std::string, SSAValue>& mConstants;
    
    static bool extractBool(const SSAValue& vv, bool& out);
    static bool extractInteger(const SSAValue& vv, Integer& out);
    static bool extractNumber(const SSAValue& vv, Number& out);
    static bool extractString(const SSAValue& vv, std::string& out);
    static bool extractVec2(const SSAValue& vv, Vec2& out);
    static bool extractVec3(const SSAValue& vv, Vec3& out);
    static bool extractVec4(const SSAValue& vv, Vec4& out);
};

} // namespace PExpr::ssa
