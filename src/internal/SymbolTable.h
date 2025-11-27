#pragma once

#include "../Logger.h"
#include "../Lookup.h"

namespace PExpr::internal {
class SymbolTable {
public:
    inline explicit SymbolTable(const SymbolTable* parent = nullptr)
        : mParent(parent)
    {
    }

    inline bool addVariable(const std::string& name, ElementaryType type, bool is_mutable)
    {
        if (const auto it = mVariables.find(name); it != mVariables.end()) {
            if (!it->second.IsMutable || it->second.Type != type)
                return false;
        }

        mVariables[name] = VariableEntry{ type, is_mutable };
        return true;
    }

    inline std::optional<VariableDef> lookupVariable(const Location& loc, const std::string& name) const
    {
        if (const auto it = mVariables.find(name); it != mVariables.end())
            return VariableDef(it->first, it->second.Type);

        return mParent ? mParent->lookupVariable(loc, name) : std::nullopt;
    }

    inline void addFunction(const std::string& name, const FunctionLookupFunction& func, bool is_extern)
    {
        mFunctions.emplace(name, FunctionEntry{ func, is_extern });
    }

    inline std::optional<FunctionDef> lookupFunction(const Location& loc, const std::string& name, const std::vector<ElementaryType>& params) const
    {
        const auto range = mFunctions.equal_range(name);
        for (auto it = range.first; it != range.second; ++it) {
            auto res = it->second.Callback(FunctionLookup(loc, name, params));
            if (res.has_value())
                return res;
        }

        return mParent ? mParent->lookupFunction(loc, name, params) : std::nullopt;
    }

    inline const SymbolTable* parent() const { return mParent; }
    inline void setParent(const SymbolTable* tbl) { mParent = tbl; }

private:
    const SymbolTable* mParent;
    struct VariableEntry {
        ElementaryType Type;
        bool IsMutable;
    };
    std::unordered_map<std::string, VariableEntry> mVariables;
    struct FunctionEntry {
        FunctionLookupFunction Callback;
        bool IsExtern;
    };
    std::unordered_multimap<std::string, FunctionEntry> mFunctions;
};
} // namespace PExpr::internal