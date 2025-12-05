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

    [[nodiscard]] std::string toString(bool suffixType = true) const;

    [[nodiscard]] inline static SSAValue Constant(bool b)
    {
        return SSAValue(Kind::Constant, b ? "true" : "false", ElementaryType::Boolean, b);
    }

    [[nodiscard]] inline static SSAValue Constant(Integer v)
    {
        return SSAValue(Kind::Constant, std::to_string(v), ElementaryType::Integer, v);
    }

    [[nodiscard]] inline static SSAValue Constant(Number v)
    {
        return SSAValue(Kind::Constant, std::to_string(v), ElementaryType::Number, v);
    }

    [[nodiscard]] inline static SSAValue Constant(const std::string& str)
    {
        return SSAValue(Kind::Constant, "\"" + str + "\"", ElementaryType::String, str);
    }

    [[nodiscard]] inline static SSAValue Constant(const Vec2& v)
    {
        return SSAValue(Kind::Constant, "[" + std::to_string(v[0]) + "," + std::to_string(v[1]) + "]", ElementaryType::Vec2, v);
    }

    [[nodiscard]] inline static SSAValue Constant(const Vec3& v)
    {
        return SSAValue(Kind::Constant, "[" + std::to_string(v[0]) + "," + std::to_string(v[1]) + "," + std::to_string(v[2]) + "]", ElementaryType::Vec2, v);
    }

    [[nodiscard]] inline static SSAValue Constant(const Vec4& v)
    {
        return SSAValue(Kind::Constant, "[" + std::to_string(v[0]) + "," + std::to_string(v[1]) + "," + std::to_string(v[2]) + "," + std::to_string(v[3]) + "]", ElementaryType::Vec2, v);
    }
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

struct SSAInstrPhi : public SSAInstr {
    SSAValue Target;
    std::vector<SSAValue> Sources;
    [[nodiscard]] std::string dump() const override;
};

struct SSAFunction {
    std::string Name;
    std::vector<std::string> Parameters;
    std::vector<std::shared_ptr<SSAInstr>> Body;
    std::vector<SSAFunction> InnerFunctions;
    ElementaryType ReturnType = ElementaryType::Unspecified;

    // Mark whether this function is external (declared but not defined).
    // External functions are considered to have side-effects.
    bool External = false;

    // Track accessed parent-level variables for later side-effect analysis.
    // These are populated by the parent SSAMapper when nested closures/functions
    // are mapped. Names are the plain variable names (without SSA version suffix).
    // Use unordered_set for faster lookup and to avoid duplicates.
    std::unordered_set<std::string> AccessedConstParents;
    std::unordered_set<std::string> AccessedMutableParents;

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
    [[nodiscard]] std::string fresh(const std::string& base);

    void mapClosure(const Ptr<Closure>& closure);
    void mapStatement(const Ptr<Statement>& stmt);
    [[nodiscard]] SSAValue mapExpression(const Ptr<Expression>& expr);
    [[nodiscard]] SSAValue handleCast(ElementaryType to, const SSAValue& from);

    // Detect captured parent-level variables referenced inside a nested function/closure.
    // Scans the provided SSA instruction list, classifies plain (non-versioned) named
    // references and appends them to the provided SSAFunction's AccessedConstParents /
    // AccessedMutableParents vectors based on this mapper's recorded local mutability.
    void detectCapturedParents(const std::vector<std::shared_ptr<SSAInstr>>& body, SSAFunction& func);

    // Program under construction
    SSAProgram mProgram;

    // Internal helpers
    std::unordered_map<std::string, int> mCounters;
    std::unordered_map<const void*, SSAValue> mExprValues;

    // Track mutability of local variable declarations in this mapper's scope.
    // Used by the parent mapper to determine whether a captured parent variable
    // is mutable or constant.
    std::unordered_map<std::string, bool> mLocalMutability;
};

} // namespace PExpr::ssa
