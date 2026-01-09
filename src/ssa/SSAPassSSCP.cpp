#include "SSAPassSSCP.h"
#include "SSAMapper.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace PExpr::ssa {

bool SSAPassSSCP::instrHasSideEffects(const SSAInstr* instr) const
{
    if (!instr)
        return false;
    // Calls have side-effects only if the callee is known to be side-effecting.
    if (auto c = dynamic_cast<const SSAInstrCall*>(instr)) {
        if (mSideEffectFunctions.find(c->FunctionName) != mSideEffectFunctions.end())
            return true;
        return false;
    }
    if (dynamic_cast<const SSAInstrReturn*>(instr))
        return true; // returns must be preserved
    // other instructions are assumed side-effect free
    return false;
}

static void replaceOperandIfConst(std::vector<SSAValue>& ops, const std::unordered_map<std::string, SSAValue>& consts)
{
    for (auto& o : ops) {
        if (o.Kind == SSAValue::Kind::Constant)
            continue;
        auto it = consts.find(o.Name);
        if (it != consts.end())
            o = it->second;
    }
}

static void countUsesInInstr(const SSAInstr* instr, std::unordered_map<std::string, int>& counts)
{
    if (!instr)
        return;
    if (auto a = dynamic_cast<const SSAInstrAssign*>(instr)) {
        for (const auto& op : a->Operands) {
            if (op.Kind != SSAValue::Kind::Constant)
                ++counts[op.Name];
        }
    } else if (auto c = dynamic_cast<const SSAInstrCall*>(instr)) {
        for (const auto& arg : c->Arguments) {
            if (arg.Kind != SSAValue::Kind::Constant)
                ++counts[arg.Name];
        }
    } else if (auto r = dynamic_cast<const SSAInstrReturn*>(instr)) {
        if (r->Value.Kind != SSAValue::Kind::Constant)
            ++counts[r->Value.Name];
    } else if (auto p = dynamic_cast<const SSAInstrPhi*>(instr)) {
        for (const auto& s : p->Sources) {
            if (s.Kind != SSAValue::Kind::Constant)
                ++counts[s.Name];
        }
    }
}

std::optional<SSAValue> SSAPassSSCP::foldAssign(const SSAInstrAssign* asg)
{
    if (!asg)
        return std::nullopt;

    // Helper extractors
    auto getBool = [](const SSAValue& vv, bool& out) -> bool {
        if (const bool* b = std::get_if<bool>(&vv.Value)) {
            out = *b;
            return true;
        }
        return false;
    };
    auto getInteger = [](const SSAValue& vv, Integer& out) -> bool {
        if (const Integer* i = std::get_if<Integer>(&vv.Value)) {
            out = static_cast<Integer>(*i);
            return true;
        }
        return false;
    };
    auto getNumber = [](const SSAValue& vv, Number& out) -> bool {
        if (const Number* n = std::get_if<Number>(&vv.Value)) {
            out = static_cast<Number>(*n);
            return true;
        }
        if (const Integer* i = std::get_if<Integer>(&vv.Value)) {
            out = static_cast<Number>(*i);
            return true;
        }
        return false;
    };
    auto getString = [](const SSAValue& vv, std::string& out) -> bool {
        if (const std::string* s = std::get_if<std::string>(&vv.Value)) {
            out = *s;
            return true;
        }
        return false;
    };
    auto getVec2 = [](const SSAValue& vv, Vec2& out) -> bool {
        if (const Vec2* n = std::get_if<Vec2>(&vv.Value)) {
            out = static_cast<Vec2>(*n);
            return true;
        }
        return false;
    };
    auto getVec3 = [](const SSAValue& vv, Vec3& out) -> bool {
        if (const Vec3* n = std::get_if<Vec3>(&vv.Value)) {
            out = static_cast<Vec3>(*n);
            return true;
        }
        return false;
    };
    auto getVec4 = [](const SSAValue& vv, Vec4& out) -> bool {
        if (const Vec4* n = std::get_if<Vec4>(&vv.Value)) {
            out = static_cast<Vec4>(*n);
            return true;
        }
        return false;
    };

    // Collect resolved operand constants
    std::vector<SSAValue> ops;
    ops.reserve(asg->Operands.size());
    for (const auto& op : asg->Operands) {
        SSAValue resolved;
        if (op.Kind == SSAValue::Kind::Constant) {
            resolved = op;
        } else {
            auto it = mConstants.find(op.Name);
            if (it == mConstants.end()) {
                // not a constant
                return std::nullopt;
            }
            resolved = it->second;
        }
        ops.push_back(resolved);
    }

    // -- This section is only reached when all operands are constant!

    // Assignment
    if (asg->Operator == SSAInstrAssign::OpKind::Assign && ops.size() == 1)
        return ops.front();

    // Access xyzw
    if (asg->Operator == SSAInstrAssign::OpKind::Access && ops.size() == 1) {
        std::vector<Number> values;
        values.reserve(asg->Swizzle.size());

        if (Vec2 v; getVec2(ops.front(), v)) {
            for (const char c : asg->Swizzle) {
                if (c == 'x' || c == 'r')
                    values.push_back(v[0]);
                if (c == 'y' || c == 'g')
                    values.push_back(v[1]);
            }
        }

        if (Vec3 v; getVec3(ops.front(), v)) {
            for (const char c : asg->Swizzle) {
                if (c == 'x' || c == 'r')
                    values.push_back(v[0]);
                if (c == 'y' || c == 'g')
                    values.push_back(v[1]);
                if (c == 'z' || c == 'b')
                    values.push_back(v[2]);
            }
        }

        if (Vec4 v; getVec4(ops.front(), v)) {
            for (const char c : asg->Swizzle) {
                if (c == 'x' || c == 'r')
                    values.push_back(v[0]);
                if (c == 'y' || c == 'g')
                    values.push_back(v[1]);
                if (c == 'z' || c == 'b')
                    values.push_back(v[2]);
                if (c == 'w' || c == 'a')
                    values.push_back(v[3]);
            }
        }

        if (values.size() == 1)
            return SSAValue::Constant(values[0]);
        if (values.size() == 2)
            return SSAValue::Constant(Vec2{ values[0], values[1] });
        if (values.size() == 3)
            return SSAValue::Constant(Vec3{ values[0], values[1], values[2] });
        if (values.size() == 4)
            return SSAValue::Constant(Vec4{ values[0], values[1], values[2], values[3] });
    }

    // Vector [x,y,z,w]
    if (asg->Operator == SSAInstrAssign::OpKind::Vector) {
        std::vector<Number> values;
        values.reserve(ops.size());
        for (const auto& vv : ops) {
            Number v;
            if (!getNumber(vv, v))
                return std::nullopt;
            values.push_back(v);
        }

        if (values.size() == 2)
            return SSAValue::Constant(Vec2{ values[0], values[1] });
        if (values.size() == 3)
            return SSAValue::Constant(Vec3{ values[0], values[1], values[2] });
        if (values.size() == 4)
            return SSAValue::Constant(Vec4{ values[0], values[1], values[2], values[3] });
    }

    // Cast
    if (asg->Operator == SSAInstrAssign::OpKind::Cast && ops.size() == 1) {
        // Is it even useful?
        if (asg->Target.Type == asg->Operands.front().Type)
            return asg->Operands.front();

        if (asg->Target.Type == ElementaryType::Number) {
            // int -> num (implicit or explicit)
            if (Integer i; getInteger(asg->Operands.front(), i))
                return SSAValue::Constant(static_cast<Number>(i));
        }

        if (asg->Target.Type == ElementaryType::Integer) {
            // num -> int (explicit)
            if (Number v; getNumber(asg->Operands.front(), v))
                return SSAValue::Constant(static_cast<Integer>(v));
        }
    }

    // Unary fold
    if (asg->Operator == SSAInstrAssign::OpKind::Unary && ops.size() == 1) {
        const auto& o = ops[0];
        switch (asg->UnaryOp) {
        case UnaryOperation::Neg: {
            if (Integer iv; getInteger(o, iv))
                return SSAValue::Constant(static_cast<Integer>(-iv));
            else if (Number dv; getNumber(o, dv))
                return SSAValue::Constant(static_cast<Number>(-dv));
            else if (Vec2 v; getVec2(o, v))
                return SSAValue::Constant(Vec2{ -v[0], -v[1] });
            else if (Vec3 v; getVec3(o, v))
                return SSAValue::Constant(Vec3{ -v[0], -v[1], -v[2] });
            else if (Vec4 v; getVec4(o, v))
                return SSAValue::Constant(Vec4{ -v[0], -v[1], -v[2], -v[3] });
            break;
        }
        case UnaryOperation::Pos:
            // +x -> x
            return o;
        case UnaryOperation::Not:
            if (bool bv; getBool(o, bv))
                return SSAValue::Constant(!bv);
            break;
        default:
            break;
        }
    }

    // Binary fold
    if (asg->Operator == SSAInstrAssign::OpKind::Binary && ops.size() == 2) {
        const auto& L = ops[0];
        const auto& R = ops[1];
        switch (asg->BinaryOp) {
        case BinaryOperation::And:
        case BinaryOperation::Or: {
            if (bool lv, rv; getBool(L, lv) && getBool(R, rv)) {
                bool res = (asg->BinaryOp == BinaryOperation::And) ? (lv && rv) : (lv || rv);
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
            Integer li, ri;
            Number ld, rd;
            bool Lint = getInteger(L, li);
            bool Rint = getInteger(R, ri);
            bool Lnum = getNumber(L, ld);
            bool Rnum = getNumber(R, rd);

            if ((Lint || Lnum) && (Rint || Rnum)) {
                bool bothInt = Lint && Rint;
                if (bothInt) {
                    if (asg->BinaryOp == BinaryOperation::Add) {
                        Integer r = li + ri;
                        return SSAValue::Constant(static_cast<Integer>(r));
                    }
                    if (asg->BinaryOp == BinaryOperation::Sub) {
                        Integer r = li - ri;
                        return SSAValue::Constant(static_cast<Integer>(r));
                    }
                    if (asg->BinaryOp == BinaryOperation::Mul) {
                        Integer r = li * ri;
                        return SSAValue::Constant(static_cast<Integer>(r));
                    }
                    if (asg->BinaryOp == BinaryOperation::Div) {
                        if (ri != 0) {
                            Integer r = li / ri;
                            return SSAValue::Constant(static_cast<Integer>(r));
                        }
                    }
                    if (asg->BinaryOp == BinaryOperation::Mod) {
                        if (ri != 0) {
                            Integer r = li % ri;
                            return SSAValue::Constant(static_cast<Integer>(r));
                        }
                    }
                    if (asg->BinaryOp == BinaryOperation::Pow) {
                        Number rr = std::pow(static_cast<Number>(li), static_cast<Number>(ri));
                        return SSAValue::Constant(static_cast<Number>(rr));
                    }
                } else {
                    Number lv = Lnum ? ld : static_cast<Number>(li);
                    Number rv = Rnum ? rd : static_cast<Number>(ri);
                    if (asg->BinaryOp == BinaryOperation::Add) {
                        Number r = lv + rv;
                        return SSAValue::Constant(static_cast<Number>(r));
                    }
                    if (asg->BinaryOp == BinaryOperation::Sub) {
                        Number r = lv - rv;
                        return SSAValue::Constant(static_cast<Number>(r));
                    }
                    if (asg->BinaryOp == BinaryOperation::Mul) {
                        Number r = lv * rv;
                        return SSAValue::Constant(static_cast<Number>(r));
                    }
                    if (asg->BinaryOp == BinaryOperation::Div) {
                        if (rv != Number(0.0)) {
                            Number r = lv / rv;
                            return SSAValue::Constant(static_cast<Number>(r));
                        }
                    }
                    if (asg->BinaryOp == BinaryOperation::Pow) {
                        Number r = std::pow(lv, rv);
                        return SSAValue::Constant(static_cast<Number>(r));
                    }
                    if (asg->BinaryOp == BinaryOperation::Mod) {
                        if (rv != Number(0.0)) {
                            Number r = std::fmod(lv, rv);
                            return SSAValue::Constant(static_cast<Number>(r));
                        }
                    }
                }
            }

            // TODO: Vector version
            break;
        }
        case BinaryOperation::Equal:
        case BinaryOperation::NotEqual:
        case BinaryOperation::Less:
        case BinaryOperation::Greater:
        case BinaryOperation::LessEqual:
        case BinaryOperation::GreaterEqual: {
            if (Number lv, rv; getNumber(L, lv) && getNumber(R, rv)) {
                bool res = false;
                switch (asg->BinaryOp) {
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

            if (asg->BinaryOp == BinaryOperation::Equal || asg->BinaryOp == BinaryOperation::NotEqual) {
                // try boolean
                if (bool lb, rb; getBool(L, lb) && getBool(R, rb)) {
                    bool res = (lb == rb);
                    if (asg->BinaryOp == BinaryOperation::NotEqual)
                        res = !res;
                    return SSAValue::Constant(res);
                }

                // try string equality
                if (std::string ls, rs; getString(L, ls) && getString(R, rs)) {
                    bool res = (ls == rs);
                    if (asg->BinaryOp == BinaryOperation::Equal)
                        return SSAValue::Constant(res);
                    if (asg->BinaryOp == BinaryOperation::NotEqual)
                        return SSAValue::Constant(!res);
                }
                // other comparisons don't apply to booleans or strings here
            }
            break;
        }
        default:
            break;
        }
    }

    return std::nullopt;
}

void SSAPassSSCP::run(SSAProgram& program)
{
    // Build helper maps: known functions and their parameter sets.
    std::unordered_map<std::string, std::unordered_set<std::string>> funcParams;
    std::unordered_set<std::string> knownFunctions;
    mSideEffectFunctions.clear();

    for (const auto& f : program.Functions) {
        knownFunctions.insert(f.Name);
        std::unordered_set<std::string> params;
        for (const auto& p : f.Parameters)
            params.insert(p);
        funcParams[f.Name] = std::move(params);
        if (f.External)
            mSideEffectFunctions.insert(f.Name);
    }

    // Propagate side-effects:
    // - Any function that calls an unknown function (not in knownFunctions) is side-effecting.
    // - Any function that calls a side-effecting function is side-effecting.
    bool progress = true;
    while (progress) {
        progress = false;
        for (const auto& f : program.Functions) {
            if (mSideEffectFunctions.find(f.Name) != mSideEffectFunctions.end())
                continue;
            // inspect body
            for (const auto& instrPtr : f.Body) {
                if (!instrPtr)
                    continue;
                if (auto call = dynamic_cast<const SSAInstrCall*>(instrPtr.get())) {
                    if (knownFunctions.find(call->FunctionName) == knownFunctions.end() || mSideEffectFunctions.find(call->FunctionName) != mSideEffectFunctions.end()) {
                        mSideEffectFunctions.insert(f.Name);
                        progress = true;
                        break;
                    }
                }
            }
        }
    }

    // Now proceed with the usual SSCP iterations (replace operands, fold, DCE), but
    // consider calls side-effecting only if the callee is marked side-effecting.
    bool changed = true;
    while (changed) {
        changed = false;

        // 1) Replace operands with known constants where possible
        for (auto& instrPtr : program.Body) {
            if (!instrPtr)
                continue;
            if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
                replaceOperandIfConst(asg->Operands, mConstants);
            } else if (auto call = dynamic_cast<SSAInstrCall*>(instrPtr.get())) {
                replaceOperandIfConst(call->Arguments, mConstants);
            } else if (auto ret = dynamic_cast<SSAInstrReturn*>(instrPtr.get())) {
                if (ret->Value.Kind != SSAValue::Kind::Constant) {
                    auto it = mConstants.find(ret->Value.Name);
                    if (it != mConstants.end()) {
                        ret->Value = it->second;
                        changed    = true;
                    }
                }
            } else if (auto phi = dynamic_cast<SSAInstrPhi*>(instrPtr.get())) {
                replaceOperandIfConst(phi->Sources, mConstants);
            }
        }
        for (auto& func : program.Functions) {
            for (auto& instrPtr : func.Body) {
                if (!instrPtr)
                    continue;
                if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
                    replaceOperandIfConst(asg->Operands, mConstants);
                } else if (auto call = dynamic_cast<SSAInstrCall*>(instrPtr.get())) {
                    replaceOperandIfConst(call->Arguments, mConstants);
                } else if (auto ret = dynamic_cast<SSAInstrReturn*>(instrPtr.get())) {
                    if (ret->Value.Kind != SSAValue::Kind::Constant) {
                        auto it = mConstants.find(ret->Value.Name);
                        if (it != mConstants.end()) {
                            ret->Value = it->second;
                            changed    = true;
                        }
                    }
                } else if (auto phi = dynamic_cast<SSAInstrPhi*>(instrPtr.get())) {
                    replaceOperandIfConst(phi->Sources, mConstants);
                }
            }
        }

        // 2) Try to fold assignments into constants
        mUseCount.clear();

        for (auto& instrPtr : program.Body) {
            if (!instrPtr)
                continue;
            if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
                auto folded = foldAssign(asg);
                if (folded) {
                    if (!asg->Target.Name.empty()) {
                        mConstants[asg->Target.Name] = *folded;
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
        }

        for (auto& func : program.Functions) {
            for (auto& instrPtr : func.Body) {
                if (!instrPtr)
                    continue;
                if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
                    auto folded = foldAssign(asg);
                    if (folded) {
                        if (!asg->Target.Name.empty()) {
                            mConstants[asg->Target.Name] = *folded;
                            SSAInstrAssign lit;
                            lit.Target   = asg->Target;
                            lit.Operator = SSAInstrAssign::OpKind::Assign;
                            lit.Operands = { *folded };
                            *asg         = std::move(lit);
                            changed      = true;
                        }
                    }
                }
            }
        }

        // 3) Dead code elimination: compute use counts and remove dead assigns without side effects
        mUseCount.clear();
        for (const auto& instrPtr : program.Body)
            countUsesInInstr(instrPtr.get(), mUseCount);
        for (const auto& func : program.Functions) {
            for (const auto& instrPtr : func.Body)
                countUsesInInstr(instrPtr.get(), mUseCount);
        }

        std::vector<std::shared_ptr<SSAInstr>> newBody;
        newBody.reserve(program.Body.size());
        for (const auto& instrPtr : program.Body) {
            if (!instrPtr)
                continue;
            if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
                int uses = 0;
                auto it  = mUseCount.find(asg->Target.Name);
                if (it != mUseCount.end())
                    uses = it->second;
                if (uses == 0 && !instrHasSideEffects(instrPtr.get())) {
                    changed = true;
                    continue;
                }
            }
            newBody.push_back(instrPtr);
        }
        program.Body.swap(newBody);

        for (auto& func : program.Functions) {
            std::vector<std::shared_ptr<SSAInstr>> newF;
            newF.reserve(func.Body.size());
            for (const auto& instrPtr : func.Body) {
                if (!instrPtr)
                    continue;
                if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
                    int uses = 0;
                    auto it  = mUseCount.find(asg->Target.Name);
                    if (it != mUseCount.end())
                        uses = it->second;
                    if (uses == 0 && !instrHasSideEffects(instrPtr.get())) {
                        changed = true;
                        continue;
                    }
                }
                newF.push_back(instrPtr);
            }
            func.Body.swap(newF);
        }
    }
}

} // namespace PExpr::ssa
