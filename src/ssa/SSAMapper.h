#pragma once

#include "Closure.h"
#include "Enums.h"

#include <unordered_set>

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
    SSAValue(Kind k, std::string n, ElementaryType type, const ExtendedValueVariant& v)
        : Kind(k)
        , Name(std::move(n))
        , Type(type)
        , Value(v)
    {
    }

    Kind Kind = Kind::Named;
    std::string Name;
    ElementaryType Type = ElementaryType::Unspecified;
    ExtendedValueVariant Value;

    [[nodiscard]] std::string toString(bool showType = true) const;
    [[nodiscard]] std::string baseName() const;

    [[nodiscard]] inline static SSAValue Constant(bool b) { return SSAValue(Kind::Constant, {}, ElementaryType::Boolean, b); }

    [[nodiscard]] inline static SSAValue Constant(Integer v) { return SSAValue(Kind::Constant, {}, ElementaryType::Integer, v); }

    [[nodiscard]] inline static SSAValue Constant(Number v) { return SSAValue(Kind::Constant, {}, ElementaryType::Number, v); }

    [[nodiscard]] inline static SSAValue Constant(const std::string& str) { return SSAValue(Kind::Constant, {}, ElementaryType::String, str); }

    [[nodiscard]] inline static SSAValue Constant(const Vec2& v) { return SSAValue(Kind::Constant, {}, ElementaryType::Vec2, v); }

    [[nodiscard]] inline static SSAValue Constant(const Vec3& v) { return SSAValue(Kind::Constant, {}, ElementaryType::Vec3, v); }

    [[nodiscard]] inline static SSAValue Constant(const Vec4& v) { return SSAValue(Kind::Constant, {}, ElementaryType::Vec4, v); }
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
                        Vector,
                        Cast,
                        Nop,
                        Phi };

    SSAValue Target;
    OpKind Operator;

    // OpKind specific data
    UnaryOperation UnaryOp   = UnaryOperation::Pos;
    BinaryOperation BinaryOp = BinaryOperation::Add;
    std::string Swizzle;
    std::vector<SSAValue> Operands;

    [[nodiscard]] std::string dump() const override;
};

struct SSAInstrCall : public SSAInstr {
    SSAValue Target;
    std::string FunctionName;       ///< Mangled unique name
    std::string PublicFunctionName; ///< User given name
    std::vector<SSAValue> Arguments;

    [[nodiscard]] std::string dump() const override;
};

struct SSAInstrReturn : public SSAInstr {
    SSAValue Value;
    [[nodiscard]] std::string dump() const override;
};

// Label instruction to mark basic blocks in the SSA body. Labels are useful
// for representing control-flow boundaries (e.g., branch entry points) and
// are emitted when inlining branch/closure bodies.
struct SSAInstrLabel : public SSAInstr {
    std::string Name;
    [[nodiscard]] std::string dump() const override;
};

// Conditional branch instruction: if Condition is true jump to TargetLabel.
struct SSAInstrBranch : public SSAInstr {
    SSAValue Condition;
    std::string TargetLabel;
    [[nodiscard]] std::string dump() const override;
};

// Unconditional jump to a label.
struct SSAInstrGoto : public SSAInstr {
    std::string TargetLabel;
    [[nodiscard]] std::string dump() const override;
};

struct SSAInstrPhi : public SSAInstr {
    SSAValue Target;
    std::vector<SSAValue> Conditions;
    std::vector<SSAValue> Branches; // One more than Conditions due to 'else' case
    [[nodiscard]] std::string dump() const override;
};

struct SSAFunction {
    std::string Name;
    std::vector<std::string> Parameters;
    std::vector<std::shared_ptr<SSAInstr>> Body;
    ElementaryType ReturnType = ElementaryType::Unspecified;

    // Mark whether this function is external (declared but not defined).
    bool External = false;
    // Mark whether this function has side-effects. Only external functions can have side-effects
    bool HasSideEffect = false;

    [[nodiscard]] std::string dump() const;
};

struct SSAProgram {
    std::vector<std::shared_ptr<SSAInstr>> Body;
    std::vector<SSAFunction> Functions;

    [[nodiscard]] std::string dump() const;
};

/// SSAMapper builds an SSAProgram from a Closure AST.
class SSAMapper {
public:
    SSAMapper();

    /// Map a closure to an SSAProgram.
    [[nodiscard]] SSAProgram map(const Ptr<Closure>& closure);

private:
    [[nodiscard]] std::string fresh(const std::string& base, bool updateScope = false);
    void pushScope();
    void popScope();
    int getCurrentVersion(const std::string& base) const;
    std::unordered_map<std::string, int>& currentScope();
    const std::unordered_map<std::string, int>& currentScope() const;

    [[nodiscard]] SSAProgram mapClosure(const Ptr<Closure>& closure);
    void mapStatement(SSAProgram& program, const Ptr<Statement>& stmt);
    [[nodiscard]] SSAValue mapExpression(SSAProgram& program, const Ptr<Expression>& expr);

    // Inline a mapped closure body into the current program by replacing any
    // SSAInstrReturn instructions with assignments to a fresh temporary variable.
    // Returns the SSAValue representing the last returned value (or a nil constant).
    SSAValue inlineClosureBody(SSAProgram& program, const std::vector<std::shared_ptr<SSAInstr>>& body);

    // Internal helpers
    std::unordered_map<std::string, int> mCounters;
    std::vector<std::unordered_map<std::string, int>> mScopeStack;
    std::unordered_map<const void*, SSAValue> mExprValues;
};

} // namespace PExpr::ssa
