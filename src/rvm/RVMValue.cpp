#include "RVMValue.h"

namespace PExpr::rvm {

RVMValue RVMValue::Register(RegId reg, const type::Type& type)
{
    RVMValue result;
    result.mKind    = Kind::REGISTER;
    result.mType    = type;
    result.mStorage = uint32_t(reg);
    return result;
}

RVMValue RVMValue::Constant(bool b)
{
    RVMValue result;
    result.mKind    = Kind::CONSTANT;
    result.mType    = type::Type(type::TypeKind::Boolean);
    result.mStorage = ValueVariant(b);
    return result;
}

RVMValue RVMValue::Constant(Integer v)
{
    RVMValue result;
    result.mKind    = Kind::CONSTANT;
    result.mType    = type::Type(type::TypeKind::Integer);
    result.mStorage = ValueVariant(v);
    return result;
}

RVMValue RVMValue::Constant(Number v)
{
    RVMValue result;
    result.mKind    = Kind::CONSTANT;
    result.mType    = type::Type(type::TypeKind::Number);
    result.mStorage = ValueVariant(v);
    return result;
}

RVMValue RVMValue::StringRef(uint32_t strId)
{
    RVMValue result;
    result.mKind    = Kind::STRING_REF;
    result.mType    = type::Type(type::TypeKind::String);
    result.mStorage = uint32_t(strId);
    return result;
}

RegId RVMValue::regId() const
{
    PEXPR_ASSERT(mKind == Kind::REGISTER, "Not a register value");
    return std::get<uint32_t>(mStorage);
}

const ValueVariant& RVMValue::constantValue() const
{
    PEXPR_ASSERT(mKind == Kind::CONSTANT, "Not a constant value");
    return std::get<ValueVariant>(mStorage);
}

uint32_t RVMValue::stringId() const
{
    PEXPR_ASSERT(mKind == Kind::STRING_REF, "Not a string reference");
    return std::get<uint32_t>(mStorage);
}

namespace {
// Helper function to hash ValueVariant
size_t hashValueVariant(const ValueVariant& v)
{
    return std::visit([](auto&& arg) -> size_t {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            return std::hash<bool>{}(arg);
        } else if constexpr (std::is_same_v<T, Integer>) {
            return std::hash<Integer>{}(arg);
        } else if constexpr (std::is_same_v<T, Number>) {
            return std::hash<Number>{}(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return std::hash<std::string>{}(arg);
        } else if constexpr (std::is_same_v<T, Tuple>) {
            // For tuples, hash each element
            size_t h = 0;
            for (const auto& elem : arg->elements) {
                h = h * 31 + hashValueVariant(elem);
            }
            return h;
        }
        return 0;
    },
                      v);
}
} // namespace

size_t RVMValue::hash() const
{
    size_t h = std::hash<int>{}(static_cast<int>(mKind));
    h        = h * 31 + mType.hash();

    switch (mKind) {
    case Kind::CONSTANT:
        h = h * 31 + hashValueVariant(std::get<ValueVariant>(mStorage));
        break;
    case Kind::REGISTER:
        h = h * 31 + std::hash<uint32_t>{}(std::get<uint32_t>(mStorage));
        break;
    case Kind::STRING_REF:
        h = h * 31 + std::hash<uint32_t>{}(std::get<uint32_t>(mStorage));
        break;
    }

    return h;
}

bool RVMValue::operator==(const RVMValue& other) const
{
    if (mKind != other.mKind || mType != other.mType)
        return false;

    switch (mKind) {
    case Kind::CONSTANT:
        return std::get<ValueVariant>(mStorage) == std::get<ValueVariant>(other.mStorage);
    case Kind::REGISTER:
        return std::get<RegId>(mStorage) == std::get<RegId>(other.mStorage);
    case Kind::STRING_REF:
        return std::get<uint32_t>(mStorage) == std::get<uint32_t>(other.mStorage);
    default:
        return false;
    }
}

} // namespace PExpr::rvm
