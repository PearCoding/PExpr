#pragma once

#include "Closure.h"
#include "Enums.h"

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
class SSAValue {
public:
    enum class Kind { Named,
                      Temp,
                      Constant };

    SSAValue() = default;
    SSAValue(Kind k, std::string n, ElementaryType type)
        : Kind(k)
        , Name(std::move(n))
        , Type(type)
    {
    }

    Kind Kind = Kind::Named;
    std::string Name;
    PExpr::ElementaryType Type = PExpr::ElementaryType::Unspecified;

    std::string toString(bool suffixType = true) const;
};

struct SSAInstr {
    virtual ~SSAInstr()              = default;
    virtual std::string dump() const = 0;
};

struct SSAInstrAssign : public SSAInstr {
    enum class OpKind { Assign,
                        Unary,
                        Binary,
                        CallOp,
                        Access,
                        Nop,
                        Phi,
                        Literal };

    SSAValue Target;
    OpKind Operator;
    std::string OperatorName; // operator-specific name (e.g. "+", "neg", "xyzw" for swizzle)
    std::vector<SSAValue> Operands;

    std::string dump() const override;
};

struct SSAInstrCall : public SSAInstr {
    SSAValue Target;
    std::string FunctionName;
    std::vector<SSAValue> Arguments;

    std::string dump() const override;
};

struct SSAInstrReturn : public SSAInstr {
    SSAValue Value;
    std::string dump() const override;
};

struct SSAInstrPhi : public SSAInstr {
    SSAValue Target;
    std::vector<SSAValue> Sources;
    std::string dump() const override;
};

struct SSAFunction {
    std::string Name;
    std::vector<std::string> Parameters;
    std::vector<std::shared_ptr<SSAInstr>> Body;
    PExpr::ElementaryType ReturnType = PExpr::ElementaryType::Unspecified;

    std::string dump() const;
};

struct SSAProgram {
    std::vector<std::shared_ptr<SSAInstr>> Body;
    std::vector<SSAFunction> Functions;

    std::string dump() const;
};

/// SSAMapper builds an SSAProgram from a Closure AST.
class SSAMapper {
public:
    SSAMapper();

    /// Map a closure to an SSAProgram.
    SSAProgram map(const Ptr<Closure>& closure);

private:
    std::string fresh(const std::string& base);

    void mapClosure(const Ptr<Closure>& closure);
    void mapStatement(const Ptr<Statement>& stmt);
    SSAValue mapExpression(const Ptr<Expression>& expr);

    // Program under construction
    SSAProgram mProgram;

    // Internal helpers
    std::unordered_map<std::string, int> mCounters;
    std::unordered_map<const void*, SSAValue> mExprValues;
};

} // namespace PExpr::ssa
