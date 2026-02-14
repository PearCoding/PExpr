#pragma once

#include "Type.h"
#include "parser/Location.h"

#include <vector>

namespace PExpr::type {

/// A general purpose variable. The actual value is defined externally.
class VariableDef {
public:
    VariableDef() = default;

    /// Construct a definition for a variable with a given name and type.
    inline VariableDef(const std::string& name, const Type& type, bool isMutable, const parser::Location& loc)
        : mName(name)
        , mType(type)
        , mIsMutable(isMutable)
        , mLocation(loc)
    {
    }

    /// The identifier the variable is named with.
    [[nodiscard]] inline const std::string& name() const { return mName; }
    /// The type of the variable.
    [[nodiscard]] inline const Type& type() const { return mType; }
    inline void setType(const Type& type) { mType = type; }

    [[nodiscard]] inline bool isMutable() const { return mIsMutable; }

    /// Get the location where this variable was declared.
    [[nodiscard]] inline const parser::Location& location() const { return mLocation; }

    /// Returns a name which is unique in a translation unit
    [[nodiscard]] inline std::string uniqueName() const
    {
        std::stringstream stream;
        stream << name() << "_L" << mLocation.line() << "C" << mLocation.column();
        return stream.str();
    }

    [[nodiscard]] auto operator<=>(const VariableDef&) const = default;

private:
    std::string mName = "";
    Type mType;
    bool mIsMutable            = false;
    parser::Location mLocation = parser::Location(0);
};

/// Simple list of variable definitions
using ParameterList = std::vector<Ptr<VariableDef>>;

/// A general purpose function definition with a fixed signature.
class FunctionDef {
public:
    /// Construct a function definition with a given name and a ParameterList.
    inline FunctionDef(const std::string& name, const std::string& mangledName, const ParameterList& params, const Type& retType, bool isExtern, bool hasSideEffects, const parser::Location& loc)
        : mName(name)
        , mMangledName(mangledName)
        , mReturnType(retType)
        , mParameters(params)
        , mIsExtern(isExtern)
        , mHasSideEffects(hasSideEffects)
        , mLocation(loc)
    {
        PEXPR_ASSERT(!isExtern || retType.kind() != TypeKind::Unspecified, "Expected a specified type for an external definition");
        PEXPR_ASSERT(isExtern || !hasSideEffects, "Only external functions can be marked side-effect free");
    }

    /// Construct a function definition with a given name and a ParameterList (rvalue)
    inline FunctionDef(const std::string& name, const std::string& mangledName, ParameterList&& params, const Type& retType, bool isExtern, bool hasSideEffects, const parser::Location& loc)
        : mName(name)
        , mMangledName(mangledName)
        , mReturnType(retType)
        , mParameters(std::move(params))
        , mIsExtern(isExtern)
        , mHasSideEffects(hasSideEffects)
        , mLocation(loc)
    {
        PEXPR_ASSERT(!isExtern || retType.kind() != TypeKind::Unspecified, "Expected a specified type for an external definition");
        PEXPR_ASSERT(isExtern || !hasSideEffects, "Only external functions can be marked side-effect free");
    }

    /// The identifier the function is named with.
    [[nodiscard]] inline const std::string& name() const { return mName; }
    /// Unique name computed internally.
    [[nodiscard]] inline const std::string& mangledName() const { return mMangledName; }

    /// The type of the return value.
    [[nodiscard]] inline const Type& returnType() const { return mReturnType; }

    /// List of parameters
    [[nodiscard]] inline const ParameterList& parameters() const { return mParameters; }

    [[nodiscard]] inline bool isExtern() const { return mIsExtern; }
    [[nodiscard]] inline bool hasSideEffects() const { return isExtern() && mHasSideEffects; }

    /// Return the location where this function was declared.
    [[nodiscard]] inline const parser::Location& location() const { return mLocation; }

private:
    std::string mName;
    std::string mMangledName;
    Type mReturnType;
    ParameterList mParameters;
    bool mIsExtern;
    bool mHasSideEffects;
    parser::Location mLocation = parser::Location(0);
};

} // namespace PExpr::type

namespace std {
template <>
class hash<PExpr::type::VariableDef> {
public:
    std::size_t operator()(const PExpr::type::VariableDef& def) const
    {
        const auto h1 = std::hash<std::string>{}(def.name());
        const auto h2 = def.type().hash();
        const auto h3 = std::hash<bool>{}(def.isMutable());
        const auto h4 = std::hash<PExpr::parser::Location>{}(def.location());

        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};
} // namespace std
