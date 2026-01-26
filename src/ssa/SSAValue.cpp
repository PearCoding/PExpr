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
    h = h * 31 + type().hash();

    if (isConstant()) {
        // Hash the constant value based on type
        switch (type().kind()) {
        case TypeKind::Boolean:
            if (const bool* b = std::get_if<bool>(&mValue))
                h = h * 31 + std::hash<bool>{}(*b);
            break;
        case TypeKind::Integer:
            if (const Integer* i = std::get_if<Integer>(&mValue))
                h = h * 31 + std::hash<Integer>{}(*i);
            break;
        case TypeKind::Number:
            if (const Number* n = std::get_if<Number>(&mValue))
                h = h * 31 + std::hash<Number>{}(*n);
            break;
        case TypeKind::String:
            if (const std::string* s = std::get_if<std::string>(&mValue))
                h = h * 31 + std::hash<std::string>{}(*s);
            break;
        case TypeKind::Tuple: {
            // TODO
            // if (const Tuple* v = std::get_if<Tuple>(&mValue)) {
            //     for (Number n : (*v)->elements)
            //         h = h * 31 + std::hash<Number>{}(n);
            // }
        } break;

        default:
            PEXPR_ASSERT(false, "Non exhaustive constant type check");
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
        return mValue == other.mValue;
    } else {
        // For Named/Temp values, compare names
        return name() == other.name();
    }
}
} // namespace PExpr::ssa