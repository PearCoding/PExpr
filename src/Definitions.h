#pragma once

#include "Enums.h"
#include "Location.h"

#include <vector>

namespace PExpr {

/// A general purpose variable. The actual value is defined externally.
class VariableDef {
public:
    /// Construct a definition for a variable with a given name and type.
    inline VariableDef(const std::string& name, ElementaryType type)
        : mName(name)
        , mType(type)
    {
        PEXPR_ASSERT(type != ElementaryType::Unspecified, "Expected a specified type for an external definition");
    }

    /// The identifier the variable is named with.
    inline const std::string& name() const { return mName; }
    /// The type of the variable.
    inline ElementaryType type() const { return mType; }

private:
    std::string mName;
    ElementaryType mType;
};

/// A general purpose function definition with a fixed signature.
class FunctionDef {
public:
    /// Construct a function definition with a given name, return type and parameter types.
    inline FunctionDef(const std::string& name, ElementaryType retType, const std::vector<ElementaryType>& params)
        : mName(name)
        , mReturnType(retType)
        , mParameters(params)
    {
        PEXPR_ASSERT(retType != ElementaryType::Unspecified, "Expected a specified type for an external definition");
    }

    /// The identifier the function is named with.
    inline const std::string& name() const { return mName; }
    /// The type of the return value.
    inline ElementaryType returnType() const { return mReturnType; }
    /// The all parameter types the function has to be called with.
    inline const std::vector<ElementaryType>& parameters() const { return mParameters; }

private:
    std::string mName;
    ElementaryType mReturnType;
    std::vector<ElementaryType> mParameters;
};

} // namespace PExpr
