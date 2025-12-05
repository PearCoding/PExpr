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

private:
    std::string mName;
    ElementaryType mType;
    bool mIsMutable;
};

/// A general purpose function definition with a fixed signature.
class FunctionDef {
public:
    /// Construct a function definition with a given name, return type and parameter types.
    inline FunctionDef(const std::string& name, const std::string& mangledName, const std::vector<ElementaryType>& params, ElementaryType retType, bool isExtern)
        : mName(name)
        , mMangledName(mangledName)
        , mReturnType(retType)
        , mParameters(params)
        , mIsExtern(isExtern)
    {
        // Allow unspecified return type for non-extern functions (to support recursion).
        if (isExtern)
            PEXPR_ASSERT(retType != ElementaryType::Unspecified, "Expected a specified type for an external definition");
    }

    /// Construct a function definition with a given name, return type and parameter types.
    inline FunctionDef(const std::string& name, const std::string& mangledName,std::vector<ElementaryType>&& params, ElementaryType retType, bool isExtern)
        : mName(name)
        , mMangledName(mangledName)
        , mReturnType(retType)
        , mParameters(std::move(params))
        , mIsExtern(isExtern)
    {
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
    [[nodiscard]] inline const std::vector<ElementaryType>& parameters() const { return mParameters; }

    [[nodiscard]] inline bool isExtern() const { return mIsExtern; }

private:
    std::string mName;
    std::string mMangledName;
    ElementaryType mReturnType;
    std::vector<ElementaryType> mParameters;
    bool mIsExtern;
};

} // namespace PExpr
