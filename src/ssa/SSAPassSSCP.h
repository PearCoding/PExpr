#pragma once

#include "SSAMapper.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace PExpr::ssa {

/// Sparse Conditional Constant Propagation (SSCP) pass for the SSA IR.
///
/// This pass performs:
///  - Constant propagation: replace uses of values known to be constants
///    with SSAValue::Kind::Constant values.
///  - Constant folding (basic): fold simple unary/binary operations when
///    operands are constant.
///  - Dead code elimination: remove instructions whose results are unused
///    and have no side-effects
///
/// The pass is intentionally conservative: it only folds simple literal
/// operations. It does not attempt advanced algebraic simplifications or
/// cross-function interprocedural propagation.
class SSAPassSSCP {
public:
    SSAPassSSCP() = default;

    /// Run the pass on a program. Modifies the program in-place.
    void run(SSAProgram& program);

private:
    // Helpers
    std::optional<SSAValue> foldAssign(const SSAInstrAssign* asg);
    bool instrHasSideEffects(const SSAInstr* instr) const;

    // map from SSA value name -> constant value (string representation + type)
    std::unordered_map<std::string, SSAValue> mConstants;

    // usage counts for SSA named/temp values
    std::unordered_map<std::string, int> mUseCount;

    // set of instructions to remove (dead)
    std::unordered_set<const SSAInstr*> mDeadInstrs;

    // set of function names that are considered to have side-effects (externals and those calling externals)
    std::unordered_set<std::string> mSideEffectFunctions;
};

} // namespace PExpr::ssa
