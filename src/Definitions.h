#pragma once

#include "Enums.h"
#include "Location.h"

#include <vector>

namespace PExpr {

/// A general purpose variable. The actual value is defined externally.
class VariableDef {
public:
    /// Construct a definition for a variable with a given name and type.
    inline VariableDef(const std::string& name, ElementaryType type, bool isMutable)
        : mName(name)
        , mType(type)
        , mIsMutable(isMutable)
    {
        PEXPR_ASSERT(type != ElementaryType::Unspecified, "Expected a specified type for an external definition");
    }

    /// The identifier the variable is named with.
    [[nodiscard]] inline const std::string& name() const { return mName; }
    /// The type of the variable.
    [[nodiscard]] inline ElementaryType type() const { return mType; }

    [[nodiscard]] inline bool isMutable() const { return mIsMutable; }

    [[nodiscard]] auto operator<=>(const VariableDef&) const = default;

private:
    std::string mName;
    ElementaryType mType;
    bool mIsMutable;
};

/// A general purpose function definition with a fixed signature.
class FunctionDef {
public:
    /// Construct a function definition with a given name, return type and parameter types.
    inline FunctionDef(const std::string& name, const std::string& mangledName, const std::vector<ElementaryType>& params, const std::vector<std::string>& paramNames, ElementaryType retType, bool isExtern)
        : mName(name)
        , mMangledName(mangledName)
        , mReturnType(retType)
        , mParameterTypes(params)
        , mParameterNames(paramNames)
        , mIsExtern(isExtern)
    {
        PEXPR_ASSERT(params.size() == paramNames.size(), "Expected parameter types and parameter names to be of same size");
        // Allow unspecified return type for non-extern functions (to support recursion).
        if (isExtern)
            PEXPR_ASSERT(retType != ElementaryType::Unspecified, "Expected a specified type for an external definition");
    }

    /// Construct a function definition with a given name, return type and parameter types.
    inline FunctionDef(const std::string& name, const std::string& mangledName, std::vector<ElementaryType>&& params, std::vector<std::string>&& paramNames, ElementaryType retType, bool isExtern)
        : mName(name)
        , mMangledName(mangledName)
        , mReturnType(retType)
        , mParameterTypes(std::move(params))
        , mParameterNames(std::move(paramNames))
        , mIsExtern(isExtern)
    {
        PEXPR_ASSERT(params.size() == paramNames.size(), "Expected parameter types and parameter names to be of same size");
        // Allow unspecified return type for non-extern functions (to support recursion).
        if (isExtern)
            PEXPR_ASSERT(retType != ElementaryType::Unspecified, "Expected a specified type for an external definition");
    }

    /// The identifier the function is named with.
    [[nodiscard]] inline const std::string& name() const { return mName; }
    /// Unique name computed internally.
    [[nodiscard]] inline const std::string& mangledName() const { return mMangledName; }

    /// The type of the return value.
    [[nodiscard]] inline ElementaryType returnType() const { return mReturnType; }
    /// The all parameter types the function has to be called with.
    [[nodiscard]] inline const std::vector<ElementaryType>& parameterTypes() const { return mParameterTypes; }

    /// Parameter names (if available). May be empty for externally-registered functions.
    [[nodiscard]] inline const std::vector<std::string>& parameterNames() const { return mParameterNames; }

    [[nodiscard]] inline bool isExtern() const { return mIsExtern; }

private:
    std::string mName;
    std::string mMangledName;
    ElementaryType mReturnType;
    std::vector<ElementaryType> mParameterTypes;
    std::vector<std::string> mParameterNames;
    bool mIsExtern;
};

} // namespace PExpr

namespace std {
template <>
class hash<PExpr::VariableDef> {
public:
    std::size_t operator()(const PExpr::VariableDef& def) const
    {
        const auto h1 = std::hash<std::string>{}(def.name());
        const auto h2 = std::hash<uint32_t>{}((uint32_t)def.type());
        const auto h3 = std::hash<bool>{}(def.isMutable());

        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};
} // namespace std