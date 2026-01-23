#pragma once

#include "Closure.h"
#include "SSAContext.h"
#include "SSAStructs.h"

namespace PExpr::ssa {

/// Object-based SSA IR representation and mapper.
/// The IR is intentionally small and extensible:
/// - SSAValue represents a named (or temporary) value.
/// - SSAInstr is the polymorphic base for instructions.
/// - SSAFunction represents a function body (named).
/// - SSAProgram contains the top-level (main) body and any nested functions.
///
/// The SSAMapper traverses a Closure AST and produces an SSAProgram made of
/// objects which can later be serialized to text or binary.
class SSAMapper {
public:
    SSAMapper();

    /// Map a closure to an SSAProgram.
    [[nodiscard]] SSAProgram map(const Ptr<Closure>& closure);

private:
    [[nodiscard]] SSAProgram mapClosure(const Ptr<Closure>& closure);
    void mapStatement(SSAProgram& program, const Ptr<Statement>& stmt);
    [[nodiscard]] SSAValue mapExpression(SSAProgram& program, const Ptr<Expression>& expr);

    // Inline a mapped closure body into the current program by replacing any
    // SSAInstrReturn instructions with assignments to a fresh temporary variable.
    // Returns the SSAValue representing the last returned value (or a nil constant).
    SSAValue inlineClosureBody(SSAProgram& program, const std::vector<std::shared_ptr<SSAInstr>>& body);

    // Internal helpers
    SSAContext mContext; // SSA context for variable name generation
    std::unordered_map<const void*, SSAValue> mExprValues;
};

} // namespace PExpr::ssa
