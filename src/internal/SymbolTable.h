#pragma once

#include "Logger.h"
#include "Lookup.h"
#include "Parameter.h"

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
        if (mVariables.contains(var.name()))
            return false;

        mVariables.emplace(var.name(), var);
        return true;
    }

    inline bool addVariable(VariableDef&& var)
    {
        if (mVariables.contains(var.name()))
            return false;

        const std::string name = var.name();
        mVariables.emplace(name, std::move(var));
        return true;
    }

    inline std::optional<VariableDef> lookupVariable(const Location& loc, const std::string& name, const SymbolTable** tbl = nullptr) const
    {
        if (const auto it = mVariables.find(name); it != mVariables.end()) {
            if (tbl)
                *tbl = this;
            return it->second;
        }

        return mParent ? mParent->lookupVariable(loc, name, tbl) : std::nullopt;
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

    /// Replace an existing function definition (matching name + parameters) or add if not present.
    inline bool replaceFunction(const FunctionDef& func)
    {
        const auto range = mFunctions.equal_range(func.name());
        for (auto it = range.first; it != range.second; ++it) {
            if (func.parameters().size() == it->second.parameters().size()
                && std::equal(func.parameters().begin(), func.parameters().end(), it->second.parameters().begin(),
                              [](const Parameter& a, const Parameter& b) { return a.Type == b.Type; })) {
                // replace this overload
                mFunctions.erase(it);
                mFunctions.emplace(func.name(), func);
                return true;
            }
        }
        // not found -> add
        mFunctions.emplace(func.name(), func);
        return true;
    }

    inline bool replaceFunction(FunctionDef&& func)
    {
        const auto range = mFunctions.equal_range(func.name());
        for (auto it = range.first; it != range.second; ++it) {
            if (func.parameters().size() == it->second.parameters().size()
                && std::equal(func.parameters().begin(), func.parameters().end(), it->second.parameters().begin(),
                              [](const Parameter& a, const Parameter& b) { return a.Type == b.Type; })) {
                mFunctions.erase(it);
                mFunctions.emplace(func.name(), std::move(func));
                return true;
            }
        }
        // not found -> add
        mFunctions.emplace(func.name(), std::move(func));
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
    [[nodiscard]] inline std::unordered_multimap<std::string, FunctionDef>::const_iterator checkFunctionExists(const std::string& name, std::span<const ElementaryType> parameterTypes, bool strict) const
    {
        const auto range = mFunctions.equal_range(name);
        if (strict) {
            for (auto it = range.first; it != range.second; ++it) {
                if (parameterTypes.size() == it->second.parameters().size()) {
                    if (parameterTypes.size() == 0 || std::equal(parameterTypes.begin(), parameterTypes.end(), it->second.parameters().begin(), [](ElementaryType aType, const Parameter& b) { return aType == b.Type; }))
                        return it;
                }
            }
        } else {
            for (auto it = range.first; it != range.second; ++it) {
                if (parameterTypes.size() == it->second.parameters().size()) {
                    if (parameterTypes.size() == 0 || std::equal(parameterTypes.begin(), parameterTypes.end(), it->second.parameters().begin(), [](ElementaryType aType, const Parameter& b) { return isConvertible(aType, b.Type); }))
                        return it;
                }
            }
        }
        return mFunctions.end();
    }

    [[nodiscard]] inline std::unordered_multimap<std::string, FunctionDef>::const_iterator checkFunctionExists(const std::string& name, std::span<const Parameter> parameters, bool strict) const
    {
        const auto range = mFunctions.equal_range(name);
        if (strict) {
            for (auto it = range.first; it != range.second; ++it) {
                if (parameters.size() == it->second.parameters().size()) {
                    if (parameters.size() == 0 || std::equal(parameters.begin(), parameters.end(), it->second.parameters().begin(), [](const Parameter& a, const Parameter& b) { return a.Type == b.Type; }))
                        return it;
                }
            }
        } else {
            for (auto it = range.first; it != range.second; ++it) {
                if (parameters.size() == it->second.parameters().size()) {
                    if (parameters.size() == 0 || std::equal(parameters.begin(), parameters.end(), it->second.parameters().begin(), [](const Parameter& a, const Parameter& b) { return isConvertible(a.Type, b.Type); }))
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
