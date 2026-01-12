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
    } else if (auto b = dynamic_cast<const SSAInstrBranch*>(instr)) {
        if (b->Condition.Kind != SSAValue::Kind::Constant)
            ++counts[b->Condition.Name];
    } else if (auto p = dynamic_cast<const SSAInstrPhi*>(instr)) {
        for (const auto& s : p->Conditions) {
            if (s.Kind != SSAValue::Kind::Constant)
                ++counts[s.Name];
        }
        for (const auto& s : p->Branches) {
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

    // FIXME: Understand why we could not just do this??
    // Assignment
    // if (asg->Operator == SSAInstrAssign::OpKind::Assign && asg->Operands.size() == 1)
    //     return asg->Operands.front();

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

template <typename Func>
[[nodiscard]] inline bool handleCallbackWithChange(SSAFunction& func, Func clb)
{
    bool changed = false;
    for (auto& f : func.InnerFunctions) {
        if (clb(f.Body))
            changed = true;
    }

    if (clb(func.Body))
        changed = true;

    return changed;
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
        if (replaceOperandIfConst(program.Body))
            changed = true;
        for (auto& func : program.Functions) {
            if (replaceOperandIfConst(func.Body))
                changed = true;
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

        // 4) Remove empty branches
        if (removeEmptyBranches(program.Body))
            changed = true;
        for (auto& func : program.Functions) {
            if (handleCallbackWithChange(func, [this](auto& a) { return this->removeEmptyBranches(a); }))
                changed = true;
        }

        // 5) Handle unused labels
        if (removeObsoleteLabels(program.Body))
            changed = true;
        for (auto& func : program.Functions) {
            if (handleCallbackWithChange(func, [this](auto& a) { return this->removeObsoleteLabels(a); }))
                changed = true;
        }

        // 6) Collapse phi nodes
        if (collapsePhiNodes(program.Body))
            changed = true;
        for (auto& func : program.Functions) {
            if (handleCallbackWithChange(func, [this](auto& a) { return this->collapsePhiNodes(a); }))
                changed = true;
        }
    }
}

bool SSAPassSSCP::replaceOperandIfConst(std::vector<SSAValue>& ops)
{
    bool changed = false;
    for (auto& o : ops) {
        if (o.Kind == SSAValue::Kind::Constant)
            continue;
        auto it = mConstants.find(o.Name);
        if (it != mConstants.end()) {
            o       = it->second;
            changed = true;
        }
    }
    return changed;
}

bool SSAPassSSCP::replaceOperandIfConst(std::vector<std::shared_ptr<SSAInstr>>& instructions)
{
    bool changed = false;
    for (auto& instrPtr : instructions) {
        if (!instrPtr)
            continue;
        if (auto asg = dynamic_cast<SSAInstrAssign*>(instrPtr.get())) {
            if (replaceOperandIfConst(asg->Operands))
                changed = true;
        } else if (auto call = dynamic_cast<SSAInstrCall*>(instrPtr.get())) {
            if (replaceOperandIfConst(call->Arguments))
                changed = true;
        } else if (auto ret = dynamic_cast<SSAInstrReturn*>(instrPtr.get())) {
            if (ret->Value.Kind != SSAValue::Kind::Constant) {
                auto it = mConstants.find(ret->Value.Name);
                if (it != mConstants.end()) {
                    ret->Value = it->second;
                    changed    = true;
                }
            }
        } else if (auto phi = dynamic_cast<SSAInstrPhi*>(instrPtr.get())) {
            if (replaceOperandIfConst(phi->Conditions))
                changed = true;
            if (replaceOperandIfConst(phi->Branches))
                changed = true;
        } else if (auto br = dynamic_cast<SSAInstrBranch*>(instrPtr.get())) {
            if (br->Condition.Kind != SSAValue::Kind::Constant) {
                auto it = mConstants.find(br->Condition.Name);
                if (it != mConstants.end()) {
                    br->Condition = it->second;
                    changed       = true;
                }
            }
        }
    }
    return changed;
}

bool SSAPassSSCP::removeEmptyBranches(std::vector<std::shared_ptr<SSAInstr>>& instructions)
{
    for (size_t i = 0; i < instructions.size() - 1; ++i) {
        // Check for the following:
        //   lbl.1:
        //   goto lbl.2
        // and
        //   lbl.1:
        //   lbl.2:
        if (auto l = dynamic_cast<const SSAInstrLabel*>(instructions[i].get())) {
            // Is the following instruction a basic jump?
            if (auto g = dynamic_cast<const SSAInstrGoto*>(instructions[i + 1].get())) {
                // Replace all necessary stuff in this program
                for (size_t j = 0; j < instructions.size(); ++j) {
                    if (i == j)
                        continue;
                    // Check branches/gotos
                    if (auto br = dynamic_cast<SSAInstrBranch*>(instructions[j].get())) {
                        if (br->TargetLabel == l->Name)
                            br->TargetLabel = g->TargetLabel;
                    } else if (auto gt = dynamic_cast<SSAInstrGoto*>(instructions[j].get())) {
                        if (gt->TargetLabel == l->Name)
                            br->TargetLabel = g->TargetLabel;
                    }
                }

                // Remove the label and the goto
                instructions.erase(instructions.begin() + i, instructions.begin() + i + 2);
                return true;
            } else if (auto l2 = dynamic_cast<const SSAInstrLabel*>(instructions[i + 1].get())) {
                // Replace all necessary stuff in this program
                for (size_t j = 0; j < instructions.size(); ++j) {
                    if (i == j)
                        continue;
                    // Check branches/gotos
                    if (auto br = dynamic_cast<SSAInstrBranch*>(instructions[j].get())) {
                        if (br->TargetLabel == l->Name)
                            br->TargetLabel = l2->Name;
                    } else if (auto gt = dynamic_cast<SSAInstrGoto*>(instructions[j].get())) {
                        if (gt->TargetLabel == l->Name)
                            br->TargetLabel = l2->Name;
                    }
                }

                // Remove the first label
                instructions.erase(instructions.begin() + i);
                return true;
            }
        }

        // Check for the following:
        //   goto lbl.1
        //   lbl.1:
        if (auto g = dynamic_cast<const SSAInstrGoto*>(instructions[i].get())) {
            // Is the following instruction a label?
            if (auto l = dynamic_cast<const SSAInstrLabel*>(instructions[i + 1].get())) {
                // The goto follows strict the label
                if (g->TargetLabel == l->Name) {
                    // Delete the goto
                    instructions.erase(instructions.begin() + i);
                    return true;
                }
            }
        }

        // Check for the following:
        //   br x -> lbl.1
        //   lbl.1:
        if (auto br = dynamic_cast<const SSAInstrBranch*>(instructions[i].get())) {
            // Is the following instruction a label?
            if (auto l = dynamic_cast<const SSAInstrLabel*>(instructions[i + 1].get())) {
                // The goto follows strict the label
                if (br->TargetLabel == l->Name) {
                    // Delete the branching
                    instructions.erase(instructions.begin() + i);
                    return true;
                }
            }
        }
    }

    return false;
}

bool SSAPassSSCP::removeObsoleteLabels(std::vector<std::shared_ptr<SSAInstr>>& instructions)
{
    // Count the usage of the labels
    std::unordered_map<std::string, size_t> counter;
    for (auto& instrPtr : instructions) {
        if (!instrPtr)
            continue;
        if (auto l = dynamic_cast<const SSAInstrLabel*>(instrPtr.get())) {
            if (!counter.contains(l->Name))
                counter[l->Name] = 0;
        } else if (auto g = dynamic_cast<const SSAInstrGoto*>(instrPtr.get())) {
            if (auto it = counter.find(g->TargetLabel); it != counter.end())
                it->second += 1;
            else
                counter[g->TargetLabel] = 1;
        } else if (auto br = dynamic_cast<const SSAInstrBranch*>(instrPtr.get())) {
            if (auto it = counter.find(br->TargetLabel); it != counter.end())
                it->second += 1;
            else
                counter[br->TargetLabel] = 1;
        }
    }

    bool changed = false;
    // Remove labels without usage
    for (auto it = instructions.begin(); it != instructions.end(); ++it) {
        if (!*it)
            continue;

        if (auto l = dynamic_cast<const SSAInstrLabel*>(it->get())) {
            if (counter.at(l->Name) == 0) {
                it      = instructions.erase(it);
                changed = true;
                if (it != instructions.begin())
                    --it;
            }
        }
    }

    return changed;
}

bool SSAPassSSCP::collapsePhiNodes(std::vector<std::shared_ptr<SSAInstr>>& instructions)
{
    bool changed = false;
    for (auto& instrPtr : instructions) {
        if (!instrPtr)
            continue;

        if (auto phi = dynamic_cast<SSAInstrPhi*>(instrPtr.get())) {
            // Check if there are some obsolete branches?
            std::vector<size_t> removableBranches;
            for (size_t i = 0; i < phi->Conditions.size(); ++i) {
                const auto& cond = phi->Conditions[i];
                if (cond.Kind == SSAValue::Kind::Constant && cond.Type == ElementaryType::Boolean) {
                    const bool condVal = std::get<bool>(cond.Value);
                    if (!condVal)
                        removableBranches.push_back(i);
                }
            }

            if (!removableBranches.empty())
                changed = true;

            // Remove the obsolete ones from back to front (for the iterator to work)
            for (auto it = removableBranches.rbegin(); it != removableBranches.rend(); ++it) {
                phi->Branches.erase(phi->Branches.begin() + *it);
                phi->Conditions.erase(phi->Conditions.begin() + *it);
            }

            // Only the 'else' statement survived
            if (phi->Conditions.empty()) {
                SSAInstrAssign asg;
                asg.Target   = phi->Target;
                asg.Operator = SSAInstrAssign::OpKind::Assign;
                asg.Operands = { phi->Branches.at(0) };
                instrPtr     = std::make_shared<SSAInstrAssign>(std::move(asg));
                changed      = true;
                continue;
            }

            // Check if the first entry is truely 'true' -> remove phi and use that one
            const auto& firstCond = phi->Conditions.at(0);
            if (firstCond.Kind == SSAValue::Kind::Constant && firstCond.Type == ElementaryType::Boolean) {
                const bool condVal = std::get<bool>(firstCond.Value);
                PEXPR_ASSERT(condVal, "Expected a 'true' phi condition as all 'false' ones should be erased");
                SSAInstrAssign asg;
                asg.Target   = phi->Target;
                asg.Operator = SSAInstrAssign::OpKind::Assign;
                asg.Operands = { phi->Branches.at(0) };
                instrPtr     = std::make_shared<SSAInstrAssign>(std::move(asg));
                changed      = true;
            }
        }
    }

    return changed;
}
} // namespace PExpr::ssa
