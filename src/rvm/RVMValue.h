#pragma once

#include "RVMTypes.h"
#include "type/Type.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace PExpr::rvm {
/// RVM value type - represents a value that can be stored in a register
class RVMValue {
public:
    enum class Kind {
        CONSTANT,
        REGISTER,
        STRING_REF, // Reference to string table entry
    };

    RVMValue() = default;

    // Register value
    static RVMValue Register(RegId reg, const type::Type& type);

    // Constant values
    static RVMValue Constant(bool b);
    static RVMValue Constant(Integer v);
    static RVMValue Constant(Number v);

    // String reference
    static RVMValue StringRef(uint32_t strId);

    [[nodiscard]] Kind kind() const { return mKind; }
    [[nodiscard]] const type::Type& type() const { return mType; }

    [[nodiscard]] bool isConstant() const { return mKind == Kind::CONSTANT; }
    [[nodiscard]] bool isRegister() const { return mKind == Kind::REGISTER; }
    [[nodiscard]] bool isStringRef() const { return mKind == Kind::STRING_REF; }

    // Accessors for different kinds
    [[nodiscard]] RegId regId() const;
    [[nodiscard]] const ValueVariant& constantValue() const;
    [[nodiscard]] uint32_t stringId() const;

    /// Compute a hash for this value
    [[nodiscard]] size_t hash() const;

    /// Check if two values are equivalent
    [[nodiscard]] bool operator==(const RVMValue& other) const;

private:
    RVMValue(Kind kind, const type::Type& type, const ValueVariant& value);
    RVMValue(Kind kind, const type::Type& type, uint32_t id);

    Kind mKind       = Kind::CONSTANT;
    type::Type mType = type::Type(type::TypeKind::Unspecified);

    // Value storage based on kind
    // For CONSTANT: stores ValueVariant
    // For REGISTER: stores RegId (uint32_t)
    // For STRING_REF: stores string table index (uint32_t)
    std::variant<ValueVariant, uint32_t> mStorage;
};

} // namespace PExpr::rvm

namespace std {
template <>
class hash<PExpr::rvm::RVMValue> {
public:
    std::size_t operator()(const PExpr::rvm::RVMValue& val) const
    {
        return val.hash();
    }
};
} // namespace std