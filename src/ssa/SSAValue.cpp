#include "SSAValue.h"

#include <sstream>

namespace PExpr::ssa {
std::string SSAValue::baseName() const
{
    PEXPR_ASSERT(!isConstant(), "Only named have a base name");

    return std::get<0>(split());
}

int SSAValue::version() const
{
    PEXPR_ASSERT(!isConstant(), "Only named have a version");

    return std::get<1>(split());
}

[[nodiscard]] std::tuple<std::string, int> SSAValue::split() const
{
    PEXPR_ASSERT(!isConstant(), "Only named have a version and base name");

    const auto thisName = name();
    if (const auto pos = thisName.rfind('.'); pos != std::string::npos) {
        const std::string baseName = thisName.substr(0, pos);
        const int version          = std::stoi(thisName.substr(pos + 1));
        return { baseName, version };
    }

    return { thisName, 0 };
}

static size_t hashValueVariant(const type::Type& type, const ValueVariant& value)
{
    switch (type.kind()) {
    case type::TypeKind::Boolean:
        return std::hash<bool>{}(std::get<bool>(value));
    case type::TypeKind::Integer:
        return std::hash<Integer>{}(std::get<Integer>(value));
    case type::TypeKind::Number:
        return std::hash<Number>{}(std::get<Number>(value));
    case type::TypeKind::String:
        return std::hash<std::string>{}(std::get<std::string>(value));
    case type::TypeKind::Tuple: {
        const Tuple& tuple     = std::get<Tuple>(value);
        size_t h               = 0;
        const auto& components = type.components();
        for (size_t i = 0; i < components.size(); ++i)
            h = h * 31 + hashValueVariant(components[i], tuple->elements.at(i));
        return h;
    }
    default:
        PEXPR_ASSERT(false, "Non exhaustive type in hashValueVariant");
        return 0;
    }
}

static bool checkValueVariant(const ValueVariant& valueA, const ValueVariant& valueB)
{
    if (valueA.index() != valueB.index())
        return false;

    if (std::holds_alternative<bool>(valueA)) {
        return std::get<bool>(valueA) == std::get<bool>(valueB);
    } else if (std::holds_alternative<Integer>(valueA)) {
        return std::get<Integer>(valueA) == std::get<Integer>(valueB);
    } else if (std::holds_alternative<Number>(valueA)) {
        return std::get<Number>(valueA) == std::get<Number>(valueB);
    } else if (std::holds_alternative<std::string>(valueA)) {
        return std::get<std::string>(valueA) == std::get<std::string>(valueB);
    } else if (std::holds_alternative<Tuple>(valueA)) {
        const auto& tupleA = *std::get<Tuple>(valueA);
        const auto& tupleB = *std::get<Tuple>(valueB);

        if (tupleA.elements.size() != tupleB.elements.size())
            return false;

        for (size_t i = 0; i < tupleA.elements.size(); ++i) {
            if (!checkValueVariant(tupleA.elements[i], tupleB.elements.at(i)))
                return false;
        }
        return true;
    } else {
        PEXPR_ASSERT(false, "Non exhaustive type in hashValueVariant");
        return false;
    }
}

size_t SSAValue::hash(bool includeName) const
{
    size_t h = std::hash<int>{}(static_cast<bool>(isConstant()));
    if (includeName && !isConstant())
        h = h * 31 + std::hash<std::string>{}(name());
    h = h * 31 + type().hash();

    if (isConstant())
        h = h * 31 + hashValueVariant(type(), mValue);
    return h;
}

bool SSAValue::operator==(const SSAValue& other) const
{
    if (isConstant() != other.isConstant() || type() != other.type())
        return false;

    if (isConstant()) {
        return checkValueVariant(mValue, other.mValue);
    } else {
        // For Named/Temp values, compare names
        return name() == other.name();
    }
}
} // namespace PExpr::ssa