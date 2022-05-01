#pragma once

#include "Definitions.h"
#include "Logger.h"

#include <algorithm>

namespace PExpr::internal {
class DefContainer {
public:
    using FunctionMap = std::unordered_multimap<std::string, FunctionDef>;

    inline bool hasVariable(const std::string& name) const
    {
        return mVarMap.count(name) > 0;
    }

    inline std::optional<VariableDef> checkVariable(const std::string& name) const
    {
        if (hasVariable(name))
            return mVarMap.at(name);
        else
            return {};
    }

    inline bool hasFunction(const std::string& name) const
    {
        return mFuncMap.count(name) > 0;
    }

    inline std::optional<std::pair<FunctionMap::const_iterator, FunctionMap::const_iterator>>
    checkFunction(const std::string& name) const
    {
        if (hasFunction(name))
            return mFuncMap.equal_range(name);
        else
            return {};
    }

    inline void registerDef(const VariableDef& def)
    {
        PEXPR_ASSERT(!def.name().empty(), "Expected a vaild variable name");
        mVarMap.emplace(def.name(), def);
    }

    inline void registerDef(const FunctionDef& def)
    {
        PEXPR_ASSERT(!def.name().empty(), "Expected a vaild function name");

        // Check if signature is unique
        auto pair = mFuncMap.equal_range(def.name());

        bool unique = true;
        for (auto it = pair.first; it != pair.second; ++it) {
            if (std::equal(def.parameters().begin(), def.parameters().end(), it->second.parameters().begin())) {
                unique = false;
                break;
            }
        }

        if (!unique)
            PEXPR_LOG(LogLevel::Error) << "Trying to add function '" << def.name() << "' with an already registred signature" << std::endl;
        else
            mFuncMap.emplace(def.name(), def);
    }

private:
    std::unordered_map<std::string, VariableDef> mVarMap;
    std::unordered_multimap<std::string, FunctionDef> mFuncMap;
};
} // namespace PExpr