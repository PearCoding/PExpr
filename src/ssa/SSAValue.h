#pragma once

#include "Enums.h"

#include <functional>

namespace PExpr::ssa {
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

    [[nodiscard]] std::string baseName() const;

    /// Compute a hash for this value
    [[nodiscard]] size_t hash() const;

    /// Check if two values are equivalent (same kind, type, and value/name)
    [[nodiscard]] bool operator==(const SSAValue& other) const;
    [[nodiscard]] bool operator!=(const SSAValue& other) const { return !(*this == other); }

    [[nodiscard]] inline static SSAValue Constant(bool b) { return SSAValue(Kind::Constant, {}, ElementaryType::Boolean, b); }

    [[nodiscard]] inline static SSAValue Constant(Integer v) { return SSAValue(Kind::Constant, {}, ElementaryType::Integer, v); }

    [[nodiscard]] inline static SSAValue Constant(Number v) { return SSAValue(Kind::Constant, {}, ElementaryType::Number, v); }

    [[nodiscard]] inline static SSAValue Constant(const std::string& str) { return SSAValue(Kind::Constant, {}, ElementaryType::String, str); }

    [[nodiscard]] inline static SSAValue Constant(const VecN& v) { return SSAValue(Kind::Constant, {}, (ElementaryType)((size_t)ElementaryType::Vec1 + v.size() - 1), v); }
};
} // namespace PExpr::ssa