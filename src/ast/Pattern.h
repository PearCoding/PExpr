#pragma once

#include "parser/Location.h"
#include "type/Definitions.h"
#include "type/Type.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace PExpr::ast {

/// Forward declaration
class Pattern;

/// Represents a single element in a destructuring pattern, which can be either:
/// 1. A simple identifier binding: "a", "mut b:num"
/// 2. A nested pattern: "[x, y]"
class PatternElement {
public:
    using Variant = std::variant<Ptr<type::VariableDef>, Ptr<Pattern>>;

    inline PatternElement(const parser::Location& loc, Variant&& var)
        : mLocation(loc)
        , mVariant(std::move(var))
    {
    }

    [[nodiscard]] inline const parser::Location& location() const { return mLocation; }
    [[nodiscard]] inline bool isSimpleBinding() const { return std::holds_alternative<Ptr<type::VariableDef>>(mVariant); }
    [[nodiscard]] inline bool isNestedPattern() const { return std::holds_alternative<Ptr<Pattern>>(mVariant); }

    [[nodiscard]] inline Ptr<type::VariableDef> simpleBinding() const { return std::get<Ptr<type::VariableDef>>(mVariant); }
    [[nodiscard]] inline Ptr<Pattern> nestedPattern() const { return std::get<Ptr<Pattern>>(mVariant); }

    /// Helper to create a simple binding element
    static inline PatternElement makeSimple(const parser::Location& loc, const Ptr<type::VariableDef>& variable)
    {
        PEXPR_ASSERT(variable, "Expected a valid variable def pointer for pattern");
        return PatternElement(loc, Variant(variable));
    }

    /// Helper to create a nested pattern element
    static inline PatternElement makeNested(const parser::Location& loc, const Ptr<Pattern>& pattern)
    {
        PEXPR_ASSERT(pattern, "Expected a valid nested pattern pointer for pattern");
        return PatternElement(loc, Variant(pattern));
    }

    inline void setAsSimpleBinding(const Ptr<type::VariableDef>& var) { mVariant = Variant(var); }
    inline void setAsNestedPattern(const Ptr<Pattern>& pat) { mVariant = Variant(pat); }

private:
    parser::Location mLocation;
    Variant mVariant;
};

/// Represents a destructuring pattern like [a:vec2, b, mut c:num] or [[a, b], c]
class Pattern {
public:
    using ElementList = std::vector<PatternElement>;

    inline Pattern(const parser::Location& loc, const ElementList& elements)
        : mLocation(loc)
        , mElements(elements)
    {
    }

    inline Pattern(const parser::Location& loc, ElementList&& elements)
        : mLocation(loc)
        , mElements(std::move(elements))
    {
    }

    [[nodiscard]] inline const parser::Location& location() const { return mLocation; }
    [[nodiscard]] inline const ElementList& elements() const { return mElements; }
    [[nodiscard]] inline ElementList& elements() { return mElements; }
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
    parser::Location mLocation;
    ElementList mElements;
};

} // namespace PExpr::ast
