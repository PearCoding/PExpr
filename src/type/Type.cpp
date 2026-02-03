#include "Type.h"
#include <sstream>

namespace PExpr::type {

std::string Type::toString() const
{
    switch (mKind) {
    case TypeKind::Error:
        return "error";
    case TypeKind::Unspecified:
        return "unspecified";
    case TypeKind::Boolean:
        return "bool";
    case TypeKind::Integer:
        return "int";
    case TypeKind::Number:
        return "num";
    case TypeKind::String:
        return "str";
    case TypeKind::Void:
        return "void";
    case TypeKind::Tuple: {
        std::stringstream ss;
        if (isVector()) {
            ss << "vec" << mComponents.size();
        } else {
            ss << "[";
            for (size_t i = 0; i < mComponents.size(); ++i) {
                if (i)
                    ss << ", ";
                ss << mComponents[i].toString();
            }
            ss << "]";
        }
        return ss.str();
    }
    default:
        PEXPR_ASSERT(false, "Unknown TypeKind");
        return "unknown";
    }
}

Type Type::FromVariant(const ValueVariant& value)
{
    if (std::holds_alternative<bool>(value)) {
        return Type(TypeKind::Boolean);
    } else if (std::holds_alternative<Integer>(value)) {
        return Type(TypeKind::Integer);
    } else if (std::holds_alternative<Number>(value)) {
        return Type(TypeKind::Number);
    } else if (std::holds_alternative<std::string>(value)) {
        return Type(TypeKind::String);
    } else if (std::holds_alternative<Tuple>(value)) {
        const auto& t = *std::get<Tuple>(value);
        std::vector<Type> innerTypes;
        innerTypes.reserve(t.elements.size());
        for (size_t i = 0; i < t.elements.size(); ++i)
            innerTypes.push_back(FromVariant(t.elements[i]));
        return Type(innerTypes);
    } else {
        PEXPR_ASSERT(false, "Invalid ValueVariant");
        return Type::Error();
    }
}

} // namespace PExpr