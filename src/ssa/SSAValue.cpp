#include "SSAValue.h"

#include <sstream>

namespace PExpr::ssa {
std::string SSAValue::baseName() const
{
    PEXPR_ASSERT(Kind != SSAValue::Kind::Constant, "Only named and temporary values have a base name");
    PEXPR_ASSERT(!Name.empty(), "The name should never be empty!");

    if (const auto pos = Name.rfind('.'); pos != std::string::npos)
        return Name.substr(0, pos);
    return Name;
}

// SSAValue hash implementation
size_t SSAValue::hash() const
{
    size_t h = std::hash<int>{}(static_cast<int>(Kind));
    h        = h * 31 + std::hash<std::string>{}(Name);
    h        = h * 31 + std::hash<int>{}(static_cast<int>(Type));

    if (Kind == Kind::Constant) {
        // Hash the constant value based on type
        switch (Type) {
        case ElementaryType::Boolean:
            if (const bool* b = std::get_if<bool>(&Value))
                h = h * 31 + std::hash<bool>{}(*b);
            break;
        case ElementaryType::Integer:
            if (const Integer* i = std::get_if<Integer>(&Value))
                h = h * 31 + std::hash<Integer>{}(*i);
            break;
        case ElementaryType::Number:
            if (const Number* n = std::get_if<Number>(&Value))
                h = h * 31 + std::hash<Number>{}(*n);
            break;
        case ElementaryType::String:
            if (const std::string* s = std::get_if<std::string>(&Value))
                h = h * 31 + std::hash<std::string>{}(*s);
            break;
        default:
            if (Type >= ElementaryType::Vec1) {
                if (const VecN* v = std::get_if<VecN>(&Value)) {
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
    if (Kind != other.Kind || Type != other.Type)
        return false;

    if (Kind == Kind::Constant) {
        // Compare constant values
        if (Type != other.Type)
            return false;

        switch (Type) {
        case ElementaryType::Boolean:
            return std::get<bool>(Value) == std::get<bool>(other.Value);
        case ElementaryType::Integer:
            return std::get<Integer>(Value) == std::get<Integer>(other.Value);
        case ElementaryType::Number:
            return std::get<Number>(Value) == std::get<Number>(other.Value);
        case ElementaryType::String:
            return std::get<std::string>(Value) == std::get<std::string>(other.Value);
        default:
            if (Type >= ElementaryType::Vec1) {
                return std::get<VecN>(Value) == std::get<VecN>(other.Value);
            }
            return false;
        }
    } else {
        // For Named/Temp values, compare names
        return Name == other.Name;
    }
}
} // namespace PExpr::ssa