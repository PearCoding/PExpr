#pragma once

#include "Enums.h"
#include "Location.h"
#include "Parameter.h"

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
        PEXPR_ASSERT(type != ElementaryType::Unspecified, "Expected a valid type for a variable definition");
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
    /// Construct a function definition with a given name and a ParameterList.
    inline FunctionDef(const std::string& name, const std::string& mangledName, const ParameterList& params, ElementaryType retType, bool isExtern)
        : mName(name)
        , mMangledName(mangledName)
        , mReturnType(retType)
        , mParameters(params)
        , mIsExtern(isExtern)
    {
        if (isExtern)
            PEXPR_ASSERT(retType != ElementaryType::Unspecified, "Expected a specified type for an external definition");
    }

    /// Construct a function definition with a given name and a ParameterList (rvalue)
    inline FunctionDef(const std::string& name, const std::string& mangledName, ParameterList&& params, ElementaryType retType, bool isExtern)
        : mName(name)
        , mMangledName(mangledName)
        , mReturnType(retType)
        , mParameters(std::move(params))
        , mIsExtern(isExtern)
    {
        if (isExtern)
            PEXPR_ASSERT(retType != ElementaryType::Unspecified, "Expected a specified type for an external definition");
    }

    /// The identifier the function is named with.
    [[nodiscard]] inline const std::string& name() const { return mName; }
    /// Unique name computed internally.
    [[nodiscard]] inline const std::string& mangledName() const { return mMangledName; }

    /// The type of the return value.
    [[nodiscard]] inline ElementaryType returnType() const { return mReturnType; }

    /// List of parameters
    [[nodiscard]] inline const ParameterList& parameters() const { return mParameters; }

    [[nodiscard]] inline bool isExtern() const { return mIsExtern; }

private:
    std::string mName;
    std::string mMangledName;
    ElementaryType mReturnType;
    ParameterList mParameters;
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