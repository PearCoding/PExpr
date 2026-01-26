#pragma once

#include "Enums.h"
#include "Location.h"
#include "Type.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace PExpr {

/// Forward declaration
class Pattern;

/// Represents a single element in a destructuring pattern, which can be either:
/// 1. A simple identifier binding: "a", "mut b:num"
/// 2. A nested pattern: "[x, y]"
class PatternElement {
public:
    /// Variant holding either a simple binding or a nested pattern
    struct SimpleBinding {
        std::string name;
        Type declaredType; // TypeKind::Unspecified if no explicit type
        bool isMutable;

        SimpleBinding(const std::string& n, const Type& type = Type(TypeKind::Unspecified), bool mutable_ = false)
            : name(n)
            , declaredType(type)
            , isMutable(mutable_)
        {
        }
    };

    using Variant = std::variant<SimpleBinding, std::shared_ptr<Pattern>>;

    inline PatternElement(const Location& loc, Variant&& var)
        : mLocation(loc)
        , mVariant(std::move(var))
    {
    }

    [[nodiscard]] inline const Location& location() const { return mLocation; }
    [[nodiscard]] inline bool isSimpleBinding() const { return std::holds_alternative<SimpleBinding>(mVariant); }
    [[nodiscard]] inline bool isNestedPattern() const { return std::holds_alternative<std::shared_ptr<Pattern>>(mVariant); }

    [[nodiscard]] inline const SimpleBinding& simpleBinding() const { return std::get<SimpleBinding>(mVariant); }
    [[nodiscard]] inline const std::shared_ptr<Pattern>& nestedPattern() const { return std::get<std::shared_ptr<Pattern>>(mVariant); }

    /// Helper to create a simple binding element
    static inline PatternElement makeSimple(const Location& loc, const std::string& name, const Type& type = Type(TypeKind::Unspecified), bool isMutable = false)
    {
        return PatternElement(loc, Variant(SimpleBinding(name, type, isMutable)));
    }

    /// Helper to create a nested pattern element
    static inline PatternElement makeNested(const Location& loc, const std::shared_ptr<Pattern>& pattern)
    {
        return PatternElement(loc, Variant(pattern));
    }

private:
    Location mLocation;
    Variant mVariant;
};

/// Represents a destructuring pattern like [a:vec2, b, mut c:num] or [[a, b], c]
class Pattern {
public:
    using ElementList = std::vector<PatternElement>;

    inline Pattern(const Location& loc, const ElementList& elements)
        : mLocation(loc)
        , mElements(elements)
    {
    }

    inline Pattern(const Location& loc, ElementList&& elements)
        : mLocation(loc)
        , mElements(std::move(elements))
    {
    }

    [[nodiscard]] inline const Location& location() const { return mLocation; }
    [[nodiscard]] inline const ElementList& elements() const { return mElements; }
    [[nodiscard]] inline size_t size() const { return mElements.size(); }

    /// Count total number of simple bindings in this pattern (recursively)
    [[nodiscard]] size_t countBindings() const
    {
        size_t count = 0;
        for (const auto& elem : mElements) {
            if (elem.isSimpleBinding())
                ++count;
            else
                count += elem.nestedPattern()->countBindings();
        }
        return count;
    }

private:
    Location mLocation;
    ElementList mElements;
};

} // namespace PExpr
