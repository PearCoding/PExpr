#pragma once

#include "Closure.h"
#include <span>

namespace PExpr::internal {
/// Encode elementary types into compact characters for mangling.
inline char encodeElemType(ElementaryType t)
{
    switch (t) {
    case ElementaryType::Boolean:
        return 'b';
    case ElementaryType::Integer:
        return 'i';
    case ElementaryType::Number:
        return 'n';
    case ElementaryType::Vec2:
        return '2';
    case ElementaryType::Vec3:
        return '3';
    case ElementaryType::Vec4:
        return '4';
    case ElementaryType::String:
        return 's';
    default:
        return 'u'; // unspecified / unknown
    }
}

/// Build mangled name from declared parameter types (no return type).
inline std::string makeMangledNameFromTypes(const std::string& name, std::span<const ElementaryType> params, const Closure* currentClosure)
{
    std::string mangled = "_Z";
    mangled += std::to_string(name.size()) + name;

    // parameter encoding prefix
    mangled += "_P";
    for (auto t : params)
        mangled.push_back(encodeElemType(t));

    // include closure chain locations (outermost first)
    if (currentClosure) {
        std::vector<std::string> parts;
        for (const Closure* c = currentClosure; c != nullptr; c = c->parent()) {
            const Location& l = c->location();
            std::stringstream ss;
            ss << "L" << l.line() << "C" << l.column();
            parts.push_back(ss.str());
        }
        for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
            mangled += "_" + *it;
        }
    }
    return mangled;
}
} // namespace PExpr::internal