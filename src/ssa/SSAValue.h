#pragma once

#include "Enums.h"

#include <functional>

namespace PExpr::ssa {
class SSAValue {
public:
    SSAValue() = default;
    SSAValue(bool isConstant, ElementaryType type, const ExtendedValueVariant& v)
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
    [[nodiscard]] inline ElementaryType type() const { return mType; }

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

    [[nodiscard]] inline static SSAValue Named(const std::string& name, ElementaryType type) { return SSAValue(false, type, name); }

    [[nodiscard]] inline static SSAValue Constant(bool b) { return SSAValue(true, ElementaryType::Boolean, b); }

    [[nodiscard]] inline static SSAValue Constant(Integer v) { return SSAValue(true, ElementaryType::Integer, v); }

    [[nodiscard]] inline static SSAValue Constant(Number v) { return SSAValue(true, ElementaryType::Number, v); }

    [[nodiscard]] inline static SSAValue Constant(const std::string& str) { return SSAValue(true, ElementaryType::String, str); }

    [[nodiscard]] inline static SSAValue Constant(const VecN& v) { return SSAValue(true, (ElementaryType)((size_t)ElementaryType::Vec1 + v.size() - 1), v); }

private:
    bool mIsConstant            = false;
    ElementaryType mType        = ElementaryType::Unspecified;
    ExtendedValueVariant mValue = "";
};
} // namespace PExpr::ssa