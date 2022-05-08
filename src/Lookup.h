#pragma once

#include "Definitions.h"

#include <algorithm>
#include <functional>

namespace PExpr {
class VariableLookup {
public:
    inline VariableLookup(const Location& location, const std::string& name)
        : mName(name)
        , mLocation(location)
    {
    }

    /// The identifier the variable is named with.
    inline const std::string& name() const { return mName; }
    /// The location of the lookup.
    inline const Location& location() const { return mLocation; }

private:
    std::string mName;
    Location mLocation;
};
/// Callback returning definition of variable if found
using VariableLookupFunction = std::function<std::optional<VariableDef>(const VariableLookup&)>;

class FunctionLookup {
public:
    inline FunctionLookup(const Location& location, const std::string& name, const std::vector<ElementaryType>& params)
        : mName(name)
        , mLocation(location)
        , mParameters(params)
    {
    }

    /// The identifier the variable is named with.
    inline const std::string& name() const { return mName; }

    /// The location of the lookup.
    inline const Location& location() const { return mLocation; }

    /// The all parameter types the function has to be called with.
    inline const std::vector<ElementaryType>& parameters() const { return mParameters; }

    /// Return true if given set of parameters is compatible with the parameters in the lookup.
    inline bool matchParameter(const std::vector<ElementaryType>& params, bool exactOnly = false) const
    {
        // First check for exact matches
        if (std::equal(mParameters.begin(), mParameters.end(), params.begin()))
            return true;

        if (exactOnly)
            return false;

        // Second check (if failed) with conversions
        return std::equal(mParameters.begin(), mParameters.end(), params.begin(),
                          isConvertible);
    }

private:
    std::string mName;
    Location mLocation;
    std::vector<ElementaryType> mParameters;
};
/// Callback returning definition of function if exact match is found
using FunctionLookupFunction = std::function<std::optional<FunctionDef>(const FunctionLookup&)>;

} // namespace PExpr
