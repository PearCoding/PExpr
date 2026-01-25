#include "SSCPConstantFolder.h"
#include "Enums.h"
#include "SSAMapper.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace PExpr::ssa {

bool SSCPConstantFolder::extractBool(const SSAValue& vv, bool& out)
{
    if (const bool* b = vv.valueAsIf<bool>()) {
        out = *b;
        return true;
    }
    return false;
}

bool SSCPConstantFolder::extractInteger(const SSAValue& vv, Integer& out)
{
    if (const Integer* i = vv.valueAsIf<Integer>()) {
        out = static_cast<Integer>(*i);
        return true;
    }
    return false;
}

bool SSCPConstantFolder::extractNumber(const SSAValue& vv, Number& out)
{
    if (const Number* n = vv.valueAsIf<Number>()) {
        out = static_cast<Number>(*n);
        return true;
    }
    if (const Integer* i = vv.valueAsIf<Integer>()) {
        out = static_cast<Number>(*i);
        return true;
    }
    return false;
}

bool SSCPConstantFolder::extractString(const SSAValue& vv, std::string& out)
{
    if (const std::string* s = vv.valueAsIf<std::string>()) {
        out = *s;
        return true;
    }
    return false;
}

bool SSCPConstantFolder::extractVecN(const SSAValue& vv, VecN& out)
{
    if (const VecN* n = vv.valueAsIf<VecN>()) {
        out = static_cast<VecN>(*n);
        return true;
    }
    return false;
}

std::optional<SSAValue> SSCPConstantFolder::foldUnaryOp(const SSAValue& operand, UnaryOperation unaryOp)
{
    switch (unaryOp) {
    case UnaryOperation::Neg: {
        // We can always safely negate without worrying about precision

        if (Integer iv; extractInteger(operand, iv))
            return SSAValue::Constant(static_cast<Integer>(-iv));
        else if (Number dv; extractNumber(operand, dv))
            return SSAValue::Constant(static_cast<Number>(-dv));
        else if (VecN v; extractVecN(operand, v)) {
            VecN t;
            t.reserve(v.size());
            for (auto e : v)
                t.push_back(-e);
            return SSAValue::Constant(t);
        }
        break;
    }
    case UnaryOperation::Pos:
        // +x -> x
        return operand;
    case UnaryOperation::Not:
        if (bool bv; extractBool(operand, bv))
            return SSAValue::Constant(!bv);
        break;
    default:
        break;
    }
    return std::nullopt;
}

std::optional<SSAValue> SSCPConstantFolder::foldBinaryOp(bool foldNumber, const SSAValue& L, const SSAValue& R, BinaryOperation binaryOp)
{
    switch (binaryOp) {
    case BinaryOperation::And:
    case BinaryOperation::Or: {
        if (bool lv, rv; extractBool(L, lv) && extractBool(R, rv)) {
            bool res = (binaryOp == BinaryOperation::And) ? (lv && rv) : (lv || rv);
            return SSAValue::Constant(res);
        }
        break;
    }
    case BinaryOperation::Add:
    case BinaryOperation::Sub:
    case BinaryOperation::Mul:
    case BinaryOperation::Div:
    case BinaryOperation::Mod:
    case BinaryOperation::Pow: {
        if (!foldNumber)
            return std::nullopt;

        Integer li, ri;
        Number ld, rd;
        bool Lint = extractInteger(L, li);
        bool Rint = extractInteger(R, ri);
        bool Lnum = extractNumber(L, ld);
        bool Rnum = extractNumber(R, rd);

        if ((Lint || Lnum) && (Rint || Rnum)) {
            bool bothInt = Lint && Rint;
            if (bothInt) {
                if (binaryOp == BinaryOperation::Add) {
                    Integer r = li + ri;
                    return SSAValue::Constant(static_cast<Integer>(r));
                }
                if (binaryOp == BinaryOperation::Sub) {
                    Integer r = li - ri;
                    return SSAValue::Constant(static_cast<Integer>(r));
                }
                if (binaryOp == BinaryOperation::Mul) {
                    Integer r = li * ri;
                    return SSAValue::Constant(static_cast<Integer>(r));
                }
                if (binaryOp == BinaryOperation::Div) {
                    if (ri != 0) {
                        Integer r = li / ri;
                        return SSAValue::Constant(static_cast<Integer>(r));
                    }
                }
                if (binaryOp == BinaryOperation::Mod) {
                    if (ri != 0) {
                        Integer r = li % ri;
                        return SSAValue::Constant(static_cast<Integer>(r));
                    }
                }
                if (binaryOp == BinaryOperation::Pow) {
                    Number rr = std::pow(static_cast<Number>(li), static_cast<Number>(ri));
                    return SSAValue::Constant(static_cast<Number>(rr));
                }
            } else {
                Number lv = Lnum ? ld : static_cast<Number>(li);
                Number rv = Rnum ? rd : static_cast<Number>(ri);
                if (binaryOp == BinaryOperation::Add) {
                    Number r = lv + rv;
                    return SSAValue::Constant(static_cast<Number>(r));
                }
                if (binaryOp == BinaryOperation::Sub) {
                    Number r = lv - rv;
                    return SSAValue::Constant(static_cast<Number>(r));
                }
                if (binaryOp == BinaryOperation::Mul) {
                    Number r = lv * rv;
                    return SSAValue::Constant(static_cast<Number>(r));
                }
                if (binaryOp == BinaryOperation::Div) {
                    if (rv != Number(0.0)) {
                        Number r = lv / rv;
                        return SSAValue::Constant(static_cast<Number>(r));
                    }
                }
                if (binaryOp == BinaryOperation::Pow) {
                    Number r = std::pow(lv, rv);
                    return SSAValue::Constant(static_cast<Number>(r));
                }
                if (binaryOp == BinaryOperation::Mod) {
                    if (rv != Number(0.0)) {
                        Number r = std::fmod(lv, rv);
                        return SSAValue::Constant(static_cast<Number>(r));
                    }
                }
            }
        }

        // Vector arithmetic folding
        if (L.type() == R.type() && ::PExpr::isArray(L.type())) {
            // Helper lambda to perform component-wise operation on arrays
            auto applyToArrays = [&](auto&& opFunc) -> std::optional<SSAValue> {
                if (VecN lv; extractVecN(L, lv)) {
                    if (VecN rv; extractVecN(R, rv)) {
                        if (lv.size() != rv.size())
                            return std::nullopt;
                        VecN result;
                        result.reserve(lv.size());
                        for (size_t i = 0; i < lv.size(); ++i)
                            result.push_back(opFunc(lv[i], rv[i]));
                        return SSAValue::Constant(result);
                    }
                }
                return std::nullopt;
            };

            // Helper lambda to check if all components are non-zero for division/mod operations
            auto allComponentsNonZero = [](const auto& arr) {
                for (const auto& val : arr)
                    if (val == Number(0.0))
                        return false;
                return true;
            };

            switch (binaryOp) {
            case BinaryOperation::Add:
                return applyToArrays([](Number a, Number b) { return a + b; });
            case BinaryOperation::Sub:
                return applyToArrays([](Number a, Number b) { return a - b; });
            case BinaryOperation::Mul:
                return applyToArrays([](Number a, Number b) { return a * b; });
            case BinaryOperation::Div:
                if (VecN rv; extractVecN(R, rv)) {
                    if (allComponentsNonZero(rv))
                        return applyToArrays([](Number a, Number b) { return a / b; });
                }
                break;
            case BinaryOperation::Mod:
                if (VecN rv; extractVecN(R, rv)) {
                    if (allComponentsNonZero(rv))
                        return applyToArrays([](Number a, Number b) { return std::fmod(a, b); });
                }
                break;
            case BinaryOperation::Pow:
                return applyToArrays([](Number a, Number b) { return std::pow(a, b); });
            default:
                break;
            }
        }

        // Vector-scalar arithmetic folding (vector * scalar, vector / scalar, vector % scalar, vector ^ scalar)
        // Note: Add and Sub are not allowed between vectors and scalars
        if (::PExpr::isArithmetic(L.type()) && ::PExpr::isArithmetic(R.type())) {
            const bool LIsArray = ::PExpr::isArray(L.type());
            const bool RIsArray = ::PExpr::isArray(R.type());

            if (LIsArray != RIsArray) { // One is vector, one is scalar
                const SSAValue& vecOp    = LIsArray ? L : R;
                const SSAValue& scalarOp = LIsArray ? R : L;

                // Extract scalar value
                Number scalarVal;
                if (extractNumber(scalarOp, scalarVal)) {
                    // Helper lambda to apply scalar operation to vector
                    auto applyScalarToVector = [&](auto&& opFunc) -> std::optional<SSAValue> {
                        if (VecN vec; extractVecN(vecOp, vec)) {
                            VecN result;
                            result.reserve(vec.size());
                            for (size_t i = 0; i < vec.size(); ++i)
                                result[i] = opFunc(vec[i], scalarVal);
                            return SSAValue::Constant(result);
                        }
                        return std::nullopt;
                    };

                    switch (binaryOp) {
                    case BinaryOperation::Mul:
                        // Both vector * scalar and scalar * vector are commutative
                        return applyScalarToVector([](Number a, Number b) { return a * b; });
                    case BinaryOperation::Div:
                        // Only vector / scalar is allowed, not scalar / vector
                        if (LIsArray) { // vector / scalar
                            if (scalarVal != Number(0.0)) {
                                return applyScalarToVector([](Number a, Number b) { return a / b; });
                            }
                        }
                        break;
                    case BinaryOperation::Mod:
                        // Only vector % scalar is allowed, not scalar % vector
                        if (LIsArray) { // vector % scalar
                            if (scalarVal != Number(0.0)) {
                                return applyScalarToVector([](Number a, Number b) { return std::fmod(a, b); });
                            }
                        }
                        break;
                    case BinaryOperation::Pow:
                        // Both vector ^ scalar and scalar ^ vector are allowed
                        return applyScalarToVector([](Number a, Number b) { return std::pow(a, b); });
                    default:
                        // Add and Sub are not allowed between vectors and scalars
                        break;
                    }
                }
            }
        }
        break;
    }
    case BinaryOperation::Equal:
    case BinaryOperation::NotEqual:
    case BinaryOperation::Less:
    case BinaryOperation::Greater:
    case BinaryOperation::LessEqual:
    case BinaryOperation::GreaterEqual: {
        if (foldNumber) {
            if (Number lv, rv; extractNumber(L, lv) && extractNumber(R, rv)) {
                bool res = false;
                switch (binaryOp) {
                case BinaryOperation::Equal:
                    res = (lv == rv);
                    break;
                case BinaryOperation::NotEqual:
                    res = (lv != rv);
                    break;
                case BinaryOperation::Less:
                    res = (lv < rv);
                    break;
                case BinaryOperation::Greater:
                    res = (lv > rv);
                    break;
                case BinaryOperation::LessEqual:
                    res = (lv <= rv);
                    break;
                case BinaryOperation::GreaterEqual:
                    res = (lv >= rv);
                    break;
                default:
                    break;
                }

                return SSAValue::Constant(res);
            }
        }

        if (binaryOp == BinaryOperation::Equal || binaryOp == BinaryOperation::NotEqual) {
            // try boolean
            if (bool lb, rb; extractBool(L, lb) && extractBool(R, rb)) {
                bool res = (lb == rb);
                if (binaryOp == BinaryOperation::NotEqual)
                    res = !res;
                return SSAValue::Constant(res);
            }

            // try string equality
            if (std::string ls, rs; extractString(L, ls) && extractString(R, rs)) {
                bool res = (ls == rs);
                if (binaryOp == BinaryOperation::Equal)
                    return SSAValue::Constant(res);
                if (binaryOp == BinaryOperation::NotEqual)
                    return SSAValue::Constant(!res);
            }
            // other comparisons don't apply to booleans or strings here
        }
        break;
    }
    default:
        break;
    }
    return std::nullopt;
}

std::optional<SSAValue> SSCPConstantFolder::foldSwizzleOp(const SSAValue& operand, const std::string& swizzle)
{
    VecN ops;
    if (!extractVecN(operand, ops) || ops.size() == 0)
        return std::nullopt;

    VecN values;
    values.reserve(swizzle.size());

    for (const char c : swizzle) {
        if (c == 'x' || c == 'r')
            values.push_back(ops.at(0));

        if (c == 'y' || c == 'g') {
            if (ops.size() > 1)
                values.push_back(ops.at(1));
            else
                return std::nullopt;
        }

        if (c == 'z' || c == 'b') {
            if (ops.size() > 2)
                values.push_back(ops.at(2));
            else
                return std::nullopt;
        }

        if (c == 'w' || c == 'a') {
            if (ops.size() > 3)
                values.push_back(ops.at(3));
            else
                return std::nullopt;
        }
    }

    if (values.size() == 1)
        return SSAValue::Constant(values[0]);
    else
        return SSAValue::Constant(values);
}

std::optional<SSAValue> SSCPConstantFolder::foldAccessOp(const SSAValue& operand, const SSAValue& index)
{
    VecN ops;
    if (!extractVecN(operand, ops) || ops.size() == 0)
        return std::nullopt;

    Integer idx;
    if (!extractInteger(index, idx))
        return std::nullopt;

    if (ops.size() < (size_t)idx || idx < 0)
        return std::nullopt;

    return SSAValue::Constant(ops.at(idx));
}

std::optional<SSAValue> SSCPConstantFolder::foldVectorOp(const std::vector<SSAValue>& operands)
{
    VecN values;
    values.reserve(operands.size());
    for (const auto& vv : operands) {
        Number v;
        if (!extractNumber(vv, v))
            return std::nullopt;
        values.push_back(v);
    }

    if (values.size() == 0)
        return std::nullopt;
    else
        return SSAValue::Constant(values);
}

std::optional<SSAValue> SSCPConstantFolder::foldCastOp(const SSAValue& operand, ElementaryType targetType)
{
    // Is it even useful?
    if (targetType == operand.type())
        return operand;

    if (targetType == ElementaryType::Number) {
        // int -> num (implicit or explicit)
        if (Integer i; extractInteger(operand, i))
            return SSAValue::Constant(static_cast<Number>(i));
    }

    if (targetType == ElementaryType::Integer) {
        // num -> int (explicit)
        if (Number v; extractNumber(operand, v))
            return SSAValue::Constant(static_cast<Integer>(v));
    }

    return std::nullopt;
}

std::optional<SSAValue> SSCPConstantFolder::foldAssign(bool foldNumber, const SSAInstrAssign* asg)
{
    if (!asg)
        return std::nullopt;

    // Collect resolved operand constants
    std::vector<SSAValue> ops;
    ops.reserve(asg->Operands.size());
    for (const auto& op : asg->Operands) {
        SSAValue resolved;
        if (op.isConstant()) {
            resolved = op;
        } else {
            const auto it = mConstants.find(op.name());
            if (it == mConstants.end()) // not a constant
                return std::nullopt;
            resolved = it->second;
        }
        ops.push_back(resolved);
    }

    // -- This section is only reached when all operands are constant!

    // Assignment
    if (asg->Operator == SSAInstrAssign::OpKind::Assign && ops.size() == 1)
        return ops.front();

    // Swizzle xyzw
    if (asg->Operator == SSAInstrAssign::OpKind::Swizzle && ops.size() == 1)
        return foldSwizzleOp(ops.front(), asg->Swizzle);

    // Access [i]
    if (asg->Operator == SSAInstrAssign::OpKind::Access && ops.size() == 2)
        return foldAccessOp(ops.at(0), ops.at(1));

    // Vector [x,y,z,w]
    if (asg->Operator == SSAInstrAssign::OpKind::Vector)
        return foldVectorOp(ops);

    // Cast
    if (asg->Operator == SSAInstrAssign::OpKind::Cast && ops.size() == 1)
        return foldCastOp(ops.front(), asg->Target.type());

    // Unary fold
    if (asg->Operator == SSAInstrAssign::OpKind::Unary && ops.size() == 1)
        return foldUnaryOp(ops[0], asg->UnaryOp);

    // Binary fold
    if (asg->Operator == SSAInstrAssign::OpKind::Binary && ops.size() == 2)
        return foldBinaryOp(foldNumber, ops[0], ops[1], asg->BinaryOp);

    return std::nullopt;
}

bool SSCPConstantFolder::replaceOperandIfConst(SSAValue& op)
{
    if (op.isConstant())
        return false;

    if (const auto it = mConstants.find(op.name()); it != mConstants.end()) {
        op = it->second;
        return true;
    }
    return false;
}

bool SSCPConstantFolder::replaceOperandIfConst(InstructionList& instructions)
{
    bool changed = false;
    for (auto& instrPtr : instructions) {
        if (!instrPtr)
            continue;
        instrPtr->forEachValue([&](SSAValue& val) {
            if (replaceOperandIfConst(val))
                changed = true;
        });
    }
    return changed;
}

bool SSCPConstantFolder::foldToConstants(bool foldNumber, InstructionList& body)
{
    bool changed = false;
    for (auto& instrPtr : body) {
        if (!instrPtr)
            continue;
        if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
            auto folded = foldAssign(foldNumber, asg);
            if (folded) {
                PEXPR_ASSERT(!asg->Target.isConstant() && !asg->Target.name().empty(), "foldAssign does not return a proper target value");
                mConstants[asg->Target.name()] = *folded;

                // mutate instruction to literal form
                SSAInstrAssign lit;
                lit.Target   = asg->Target;
                lit.Operator = SSAInstrAssign::OpKind::Assign;
                lit.Operands = { *folded };
                *asg         = std::move(lit);
                changed      = true;
            }
        }
    }
    return changed;
}

} // namespace PExpr::ssa
