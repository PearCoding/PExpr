#pragma once

#include "ast/Closure.h"
#include <span>

namespace PExpr::type {
namespace internal {
/// Encode elementary types into compact characters for mangling.
inline std::string encodeElemType(const Type& t)
{
    switch (t.kind()) {
    case TypeKind::Boolean:
        return "b";
    case TypeKind::Integer:
        return "i";
    case TypeKind::Number:
        return "n";
    case TypeKind::String:
        return "s";
    case TypeKind::Tuple: {
        std::stringstream stream;
        stream << "T" << t.size();
        for (const auto& c : t.components())
            stream << encodeElemType(c);
        return stream.str();
    }
    default:
        return "u"; // unspecified / unknown
    }
}

inline std::string appendClosure(const ast::Closure* currentClosure)
{
    std::string suffix;
    std::vector<std::string> parts;
    for (const auto* c = currentClosure; c->parent() != nullptr /* Stop at the global closure */; c = c->parent()) {
        const auto& l = c->location();
        std::stringstream ss;
        ss << "L" << l.line() << "C" << l.column();
        parts.push_back(ss.str());
    }
    for (auto it = parts.rbegin(); it != parts.rend(); ++it)
        suffix += "_" + *it;
    return suffix;
}

} // namespace internal

/// Build mangled name from declared parameter types (no return type).
inline std::string makeMangledNameFromTypes(const std::string& name, std::span<const Type> params, const ast::Closure* currentClosure)
{
    std::string mangled = "_Z";
    mangled += std::to_string(name.size()) + name;

    // parameter encoding prefix
    mangled += "_P";
    for (auto t : params)
        mangled += internal::encodeElemType(t);

    // include closure chain locations (outermost first)
    // but skip the global closure (which is identified by having no parent)
    if (currentClosure)
        mangled += internal::appendClosure(currentClosure);
    return mangled;
}

inline std::string makeMangledNameFromTypes(const std::string& name, std::span<const Ptr<VariableDef>> params, const ast::Closure* currentClosure)
{
    std::string mangled = "_Z";
    mangled += std::to_string(name.size()) + name;

    // parameter encoding prefix
    mangled += "_P";
    for (auto p : params)
        mangled += internal::encodeElemType(p->type());

    // include closure chain locations (outermost first)
    // but skip the global closure (which is identified by having no parent)
    if (currentClosure)
        mangled += internal::appendClosure(currentClosure);
    return mangled;
}
} // namespace PExpr::type