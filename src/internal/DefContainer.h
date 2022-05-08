#pragma once

#include "../Lookup.h"
#include "../Logger.h"

namespace PExpr::internal {
class DefContainer {
public:
    inline void addVariableLookupFunction(const VariableLookupFunction& func)
    {
        mVars.emplace_back(func);
    }

    inline std::optional<VariableDef> lookupVariable(const Location& loc, const std::string& name) const
    {
        for (const auto& cb : mVars) {
            auto res = cb(VariableLookup(loc, name));
            if (res.has_value())
                return res;
        }
        return {};
    }

    inline void addFunctionLookupFunction(const FunctionLookupFunction& func)
    {
        mFuncs.emplace_back(func);
    }

    inline std::optional<FunctionDef> lookupFunction(const Location& loc, const std::string& name, const std::vector<ElementaryType>& params) const
    {
        for (const auto& cb : mFuncs) {
            auto res = cb(FunctionLookup(loc, name, params));
            if (res.has_value())
                return res;
        }
        return {};
    }

private:
    std::vector<VariableLookupFunction> mVars;
    std::vector<FunctionLookupFunction> mFuncs;
};
} // namespace PExpr::internal