#pragma once

#include "Enums.h"
#include <vector>

namespace PExpr {
class VariableDef {
public:
    inline VariableDef(const std::string& name, ElementaryType type)
        : VariableDef(name, type, false)
    {
    }

    VariableDef(const VariableDef&) = default;
    VariableDef(VariableDef&&)      = default;

    VariableDef& operator=(const VariableDef&) = default;
    VariableDef& operator=(VariableDef&&) = default;

    inline const std::string& name() const { return mName; }
    inline ElementaryType type() const { return mType; }
    inline bool isConstant() const { return mIsConstant; }

protected:
    inline VariableDef(const std::string& name, ElementaryType type, bool isConstant)
        : mName(name)
        , mType(type)
        , mIsConstant(isConstant)
    {
        PEXPR_ASSERT(type != ElementaryType::Unspecified, "Expected a specified type for an external definition");
    }

private:
    std::string mName;
    ElementaryType mType;
    bool mIsConstant;
};

class ConstantDef : public VariableDef {
public:
    inline ConstantDef(const std::string& name, ElementaryType type, const ValueVariant& value)
        : VariableDef(name, type, true)
        , mValue(value)
    {
    }

    inline const std::string& name() const { return mName; }
    inline ElementaryType type() const { return mType; }

    inline const ValueVariant& value() const { return mValue; }

    inline bool asBool() const
    {
        PEXPR_ASSERT(type() == ElementaryType::Boolean, "Trying to get a constant which is not a boolean");
        return std::get<bool>(mValue);
    }

    inline Integer asInteger() const
    {
        PEXPR_ASSERT(type() == ElementaryType::Integer, "Trying to get a constant which is not a integer");
        return std::get<Integer>(mValue);
    }

    inline Number asNumber() const
    {
        PEXPR_ASSERT(type() == ElementaryType::Number, "Trying to get a constant which is not a number");
        return std::get<Number>(mValue);
    }

    inline std::string asString() const
    {
        PEXPR_ASSERT(type() == ElementaryType::String, "Trying to get a constant which is not a string");
        return std::get<std::string>(mValue);
    }

private:
    std::string mName;
    ElementaryType mType;
    ValueVariant mValue;
};

class FunctionDef {
public:
    inline FunctionDef(const std::string& name, ElementaryType retType, const std::vector<ElementaryType>& args)
        : mName(name)
        , mReturnType(retType)
        , mArguments(args)
    {
        PEXPR_ASSERT(retType != ElementaryType::Unspecified, "Expected a specified type for an external definition");
    }

    inline const std::string& name() const { return mName; }
    inline ElementaryType returnType() const { return mReturnType; }
    inline const std::vector<ElementaryType>& arguments() const { return mArguments; }

private:
    std::string mName;
    ElementaryType mReturnType;
    std::vector<ElementaryType> mArguments;
};
} // namespace PExpr
