#pragma once

#include "Logger.h"
#include "Lookup.h"

#include <span>

namespace PExpr::internal {
class SymbolTable {
public:
    inline explicit SymbolTable(const SymbolTable* parent = nullptr)
        : mParent(parent)
    {
    }

    inline bool addVariable(const VariableDef& var)
    {
        if (const auto it = mVariables.find(var.name()); it != mVariables.end())
            return false;

        mVariables.emplace(var.name(), var);
        return true;
    }

    inline bool addVariable(VariableDef&& var)
    {
        if (const auto it = mVariables.find(var.name()); it != mVariables.end())
            return false;

        const std::string name = var.name();
        mVariables.emplace(name, std::move(var));
        return true;
    }

    inline std::optional<VariableDef> lookupVariable(const Location& loc, const std::string& name) const
    {
        if (const auto it = mVariables.find(name); it != mVariables.end())
            return it->second;

        return mParent ? mParent->lookupVariable(loc, name) : std::nullopt;
    }

    inline bool addFunction(const FunctionDef& func)
    {
        // Check if a function with the same name and parameters exists
        if (checkFunctionExists(func.name(), func.parameters(), true) != mFunctions.end())
            return false;

        mFunctions.emplace(func.name(), func);
        return true;
    }

    inline bool addFunction(FunctionDef&& func)
    {
        // Check if a function with the same name and parameters exists
        if (checkFunctionExists(func.name(), func.parameters(), true) != mFunctions.end())
            return false;

        const std::string name = func.name();
        mFunctions.emplace(name, std::move(func));
        return true;
    }

    inline std::optional<FunctionDef> lookupFunction(const Location& loc, const std::string& name, const std::vector<ElementaryType>& parameterTypes, bool strict = false) const
    {
        // Check for correct parameters (be strict!)
        if (const auto it = checkFunctionExists(name, parameterTypes, true); it != mFunctions.end())
            return it->second;

        // Check the parent, but be strict
        if (mParent) {
            if (const auto res = mParent->lookupFunction(loc, name, parameterTypes, true); res.has_value())
                return res;
        }

        if (!strict) {
            // Check for correct parameters (implicit conversion from Integer to Number is allowed)
            if (const auto it = checkFunctionExists(name, parameterTypes, false); it != mFunctions.end())
                return it->second;
            return mParent ? mParent->lookupFunction(loc, name, parameterTypes, false) : std::nullopt;
        }

        return std::nullopt;
    }

    inline const SymbolTable* parent() const { return mParent; }
    inline void setParent(const SymbolTable* tbl) { mParent = tbl; }

private:
    [[nodiscard]] inline std::unordered_multimap<std::string, FunctionDef>::const_iterator checkFunctionExists(const std::string& name, const std::span<const ElementaryType>& parameterTypes, bool strict) const
    {
        const auto range = mFunctions.equal_range(name);
        if (strict) {
            for (auto it = range.first; it != range.second; ++it) {
                const auto& params = it->second.parameters();
                if (parameterTypes.size() == params.size()) {
                    if (parameterTypes.size() == 0 || std::equal(parameterTypes.begin(), parameterTypes.end(), params.begin()))
                        return it;
                }
            }
        } else {
            for (auto it = range.first; it != range.second; ++it) {
                const auto& params = it->second.parameters();
                if (parameterTypes.size() == params.size()) {
                    if (parameterTypes.size() == 0 || std::equal(parameterTypes.begin(), parameterTypes.end(), params.begin(), isConvertible))
                        return it;
                }
            }
        }
        return mFunctions.end();
    }

    const SymbolTable* mParent;
    std::unordered_map<std::string, VariableDef> mVariables;
    std::unordered_multimap<std::string, FunctionDef> mFunctions;
};
} // namespace PExpr::internal