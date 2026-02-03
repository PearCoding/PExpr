#pragma once

#include "Definitions.h"
#include "parser/Location.h"

#include <span>

namespace PExpr::type {
class SymbolTable {
private:
    inline explicit SymbolTable(const SymbolTable* parent)
        : mParent(parent)
    {
    }

public:
    inline SymbolTable()
        : mParent(nullptr)
    {
    }

    inline static SymbolTable Connect(const SymbolTable* parent) { return SymbolTable(parent); }

    inline void addDefaultTypeAliases()
    {
        addTypeAlias("bool", Type(TypeKind::Boolean));
        addTypeAlias("int", Type(TypeKind::Integer));
        addTypeAlias("num", Type(TypeKind::Number));
        addTypeAlias("str", Type(TypeKind::String));
        addTypeAlias("void", Type::Void());
        addTypeAlias("vec2", Type::AsVector(2));
        addTypeAlias("vec3", Type::AsVector(3));
        addTypeAlias("vec4", Type::AsVector(4));
    }

    //-------------------------------------------

    inline bool addVariable(const Ptr<VariableDef>& var)
    {
        PEXPR_ASSERT(var, "Expected a valid variable def pointer");
        if (mVariables.contains(var->name()))
            return false;

        mVariables.emplace(var->name(), var);
        return true;
    }

    [[nodiscard]] inline Ptr<VariableDef> lookupVariable(const parser::Location& loc, const std::string& name, const SymbolTable** tbl = nullptr) const
    {
        if (const auto it = mVariables.find(name); it != mVariables.end()) {
            if (tbl)
                *tbl = this;
            return it->second;
        }

        return mParent ? mParent->lookupVariable(loc, name, tbl) : nullptr;
    }

    /// Returns true if this table contains the variable. Does not check in the parent!
    [[nodiscard]] inline bool containsVariable(const Ptr<VariableDef>& var) const
    {
        PEXPR_ASSERT(var, "Expected a valid variable def pointer");
        return mVariables.contains(var->name());
    }

    //-------------------------------------------

    inline bool addTypeAlias(const std::string& name, const Type& type)
    {
        if (mTypeAliases.contains(name))
            return false;

        mTypeAliases.emplace(name, type);
        return true;
    }

    [[nodiscard]] inline std::optional<Type> lookupTypeAlias(const std::string& name) const
    {
        if (const auto it = mTypeAliases.find(name); it != mTypeAliases.end()) {
            return it->second;
        }

        return mParent ? mParent->lookupTypeAlias(name) : std::nullopt;
    }

    //-------------------------------------------

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
    inline bool replaceFunction(FunctionDef&& func)
    {
        const auto range = mFunctions.equal_range(func.name());
        for (auto it = range.first; it != range.second; ++it) {
            if (func.parameters().size() == it->second.parameters().size()
                && std::equal(func.parameters().begin(), func.parameters().end(), it->second.parameters().begin(),
                              [](const Ptr<VariableDef>& a, const Ptr<VariableDef>& b) { return a->type() == b->type(); })) {
                mFunctions.erase(it);
                mFunctions.emplace(func.name(), std::move(func));
                return true;
            }
        }
        // not found -> add
        mFunctions.emplace(func.name(), std::move(func));
        return true;
    }

    inline bool removeFunction(const FunctionDef& func)
    {
        const auto range = mFunctions.equal_range(func.name());
        for (auto it = range.first; it != range.second; ++it) {
            if (func.parameters().size() == it->second.parameters().size()
                && std::equal(func.parameters().begin(), func.parameters().end(), it->second.parameters().begin(),
                              [](const Ptr<VariableDef>& a, const Ptr<VariableDef>& b) { return a->type() == b->type(); })) {
                mFunctions.erase(it);
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] inline std::optional<FunctionDef> lookupFunction(const parser::Location& loc, const std::string& name, const std::vector<Type>& parameterTypes, bool strict = false) const
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

#if 0
    inline void printVariables()
    {
        for (const auto& v : mVariables)
            std::cout << v.first << std::endl;
    }
#endif

private:
    [[nodiscard]] inline std::unordered_multimap<std::string, FunctionDef>::const_iterator checkFunctionExists(const std::string& name, std::span<const Type> parameterTypes, bool strict) const
    {
        const auto range = mFunctions.equal_range(name);
        if (strict) {
            for (auto it = range.first; it != range.second; ++it) {
                if (parameterTypes.size() == it->second.parameters().size()) {
                    if (parameterTypes.size() == 0 || std::equal(parameterTypes.begin(), parameterTypes.end(), it->second.parameters().begin(), [](const Type& aType, const Ptr<VariableDef>& b) { return aType == b->type(); }))
                        return it;
                }
            }
        } else {
            for (auto it = range.first; it != range.second; ++it) {
                if (parameterTypes.size() == it->second.parameters().size()) {
                    if (parameterTypes.size() == 0 || std::equal(parameterTypes.begin(), parameterTypes.end(), it->second.parameters().begin(), [](const Type& aType, const Ptr<VariableDef>& b) { return isConvertible(aType, b->type()); }))
                        return it;
                }
            }
        }
        return mFunctions.end();
    }

    [[nodiscard]] inline std::unordered_multimap<std::string, FunctionDef>::const_iterator checkFunctionExists(const std::string& name, std::span<const Ptr<VariableDef>> parameters, bool strict) const
    {
        const auto range = mFunctions.equal_range(name);
        if (strict) {
            for (auto it = range.first; it != range.second; ++it) {
                if (parameters.size() == it->second.parameters().size()) {
                    if (parameters.size() == 0 || std::equal(parameters.begin(), parameters.end(), it->second.parameters().begin(), [](const Ptr<VariableDef>& a, const Ptr<VariableDef>& b) { return a->type() == b->type(); }))
                        return it;
                }
            }
        } else {
            for (auto it = range.first; it != range.second; ++it) {
                if (parameters.size() == it->second.parameters().size()) {
                    if (parameters.size() == 0 || std::equal(parameters.begin(), parameters.end(), it->second.parameters().begin(), [](const Ptr<VariableDef>& a, const Ptr<VariableDef>& b) { return isConvertible(a->type(), b->type()); }))
                        return it;
                }
            }
        }
        return mFunctions.end();
    }

    const SymbolTable* mParent;
    std::unordered_map<std::string, Ptr<VariableDef>> mVariables;
    std::unordered_multimap<std::string, FunctionDef> mFunctions;
    std::unordered_map<std::string, Type> mTypeAliases;
};
} // namespace PExpr::type
