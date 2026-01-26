#pragma once

#include "Type.h"

#include <functional>

namespace PExpr::ssa {
class SSAValue {
public:
    SSAValue() = default;
    SSAValue(bool isConstant, const Type& type, const ValueVariant& v)
        : mIsConstant(isConstant)
        , mType(type)
        , mValue(v)
    {
    }

    [[nodiscard]] std::string baseName() const;
    [[nodiscard]] inline std::string name() const
    {
        PEXPR_ASSERT(!isConstant(), "Only non-constant values have a name");
        return valueAs<std::string>();
    }

    [[nodiscard]] inline bool isConstant() const { return mIsConstant; }
    [[nodiscard]] inline const Type& type() const { return mType; }

    /// Compute a hash for this value
    [[nodiscard]] size_t hash(bool includeName = true) const;

    /// Check if two values are equivalent (same kind, type, and value/name)
    [[nodiscard]] bool operator==(const SSAValue& other) const;
    [[nodiscard]] bool operator!=(const SSAValue& other) const { return !(*this == other); }

    template <typename T>
    [[nodiscard]] inline const T& valueAs() const { return std::get<T>(mValue); }
    template <typename T>
    [[nodiscard]] inline const T* valueAsIf() const { return std::get_if<T>(&mValue); }
    [[nodiscard]] inline const auto& rawValue() const { return mValue; }

    [[nodiscard]] inline static SSAValue Named(const std::string& name, const Type& type) { return SSAValue(false, type, name); }

    [[nodiscard]] inline static SSAValue Constant(bool b) { return SSAValue(true, Type(TypeKind::Boolean), b); }

    [[nodiscard]] inline static SSAValue Constant(Integer v) { return SSAValue(true, Type(TypeKind::Integer), v); }

    [[nodiscard]] inline static SSAValue Constant(Number v) { return SSAValue(true, Type(TypeKind::Number), v); }

    [[nodiscard]] inline static SSAValue Constant(const std::string& str) { return SSAValue(true, Type(TypeKind::String), str); }

    [[nodiscard]] inline static SSAValue Constant(const std::vector<Number>& v)
    {
        std::vector<Type> innerTypes;
        Tuple tuple = Tuple(new TupleVariant());

        innerTypes.reserve(v.size());
        tuple->elements.reserve(v.size());
        for (size_t i = 0; i < v.size(); ++i) {
            innerTypes.push_back(Type(TypeKind::Number));
            tuple->elements.push_back(v[i]);
        }

        return SSAValue(true, Type(innerTypes), tuple);
    }

private:
    bool mIsConstant    = false;
    Type mType          = Type(TypeKind::Unspecified);
    ValueVariant mValue = "";
};
} // namespace PExpr::ssa