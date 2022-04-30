#pragma once

#include "Enums.h"
#include "pexpr_config.h"

namespace PExpr {
class VariableContainer {
public:
    inline bool hasVariable(const std::string& name) const
    {
        return mMap.count(name) > 0;
    }

    inline void registerVariable(const std::string& name, ElementaryType type)
    {
        PEXPR_ASSERT(!name.empty(), "Expected a vaild variable name");
        PEXPR_ASSERT(type != ElementaryType::Unspecified, "Can not register variable of unspecified type");
        mMap[name] = type;
    }

    inline ElementaryType checkVariable(const std::string& name) const
    {
        if (hasVariable(name))
            return mMap.at(name);
        else
            return ElementaryType::Unspecified;
    }

private:
    std::unordered_map<std::string, ElementaryType> mMap;
};
} // namespace PExpr