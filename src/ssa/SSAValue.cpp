#include "SSAValue.h"

#include <sstream>

namespace PExpr::ssa {
std::string SSAValue::baseName() const
{
    PEXPR_ASSERT(!isConstant(), "Only named have a base name");

    const auto thisName = name();
    if (const auto pos = thisName.rfind('.'); pos != std::string::npos)
        return thisName.substr(0, pos);
    return thisName;
}

// SSAValue hash implementation
size_t SSAValue::hash(bool includeName) const
{
    size_t h = std::hash<int>{}(static_cast<bool>(isConstant()));
    if (includeName && !isConstant())
        h = h * 31 + std::hash<std::string>{}(name());
    h = h * 31 + std::hash<int>{}(static_cast<int>(type()));

    if (isConstant()) {
        // Hash the constant value based on type
        switch (type()) {
        case ElementaryType::Boolean:
            if (const bool* b = std::get_if<bool>(&mValue))
                h = h * 31 + std::hash<bool>{}(*b);
            break;
        case ElementaryType::Integer:
            if (const Integer* i = std::get_if<Integer>(&mValue))
                h = h * 31 + std::hash<Integer>{}(*i);
            break;
        case ElementaryType::Number:
            if (const Number* n = std::get_if<Number>(&mValue))
                h = h * 31 + std::hash<Number>{}(*n);
            break;
        case ElementaryType::String:
            if (const std::string* s = std::get_if<std::string>(&mValue))
                h = h * 31 + std::hash<std::string>{}(*s);
            break;
        default:
            if (type() >= ElementaryType::Vec1) {
                if (const VecN* v = std::get_if<VecN>(&mValue)) {
                    for (Number n : *v)
                        h = h * 31 + std::hash<Number>{}(n);
                }
            }
            break;
        }
    }
    return h;
}

bool SSAValue::operator==(const SSAValue& other) const
{
    if (isConstant() != other.isConstant() || type() != other.type())
        return false;

    if (isConstant()) {
        // Compare constant values
        switch (type()) {
        case ElementaryType::Boolean:
            return std::get<bool>(mValue) == std::get<bool>(other.mValue);
        case ElementaryType::Integer:
            return std::get<Integer>(mValue) == std::get<Integer>(other.mValue);
        case ElementaryType::Number:
            return std::get<Number>(mValue) == std::get<Number>(other.mValue);
        case ElementaryType::String:
            return std::get<std::string>(mValue) == std::get<std::string>(other.mValue);
        default:
            if (type() >= ElementaryType::Vec1) {
                return std::get<VecN>(mValue) == std::get<VecN>(other.mValue);
            }
            return false;
        }
    } else {
        // For Named/Temp values, compare names
        return name() == other.name();
    }
}
} // namespace PExpr::ssa