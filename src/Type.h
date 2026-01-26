#pragma once

#include "Enums.h"
#include "PExpr_Config.h"

#include <string>
#include <variant>
#include <vector>

namespace PExpr {

/// Base type kind
enum class TypeKind {
    Error,
    Unspecified,
    Boolean,
    Integer,
    Number,
    String,
    Tuple,
};

/// Type representation that can be elementary or tuple
class Type {
public:
    /// Construct an elementary type
    inline explicit Type(TypeKind kind)
        : mKind(kind)
    {
        PEXPR_ASSERT(kind != TypeKind::Tuple, "Use Type::Tuple constructor for tuple types");
    }

    /// Construct a tuple type with component types
    inline explicit Type(std::vector<Type> components)
        : mKind(TypeKind::Tuple)
        , mComponents(std::move(components))
    {
        PEXPR_ASSERT(!mComponents.empty(), "Tuple must have at least one component");
    }

    /// Default constructor creates unspecified type
    inline Type()
        : Type(TypeKind::Unspecified)
    {
    }

    /// Construct tuple as a vector/array (homogeneous tuple of numbers)
    [[nodiscard]] inline static Type AsVector(size_t n)
    {
        std::vector<Type> innerTypes;
        innerTypes.reserve(n);
        for (size_t i = 0; i < n; ++i)
            innerTypes.push_back(Type(TypeKind::Number));
        return Type(std::move(innerTypes));
    }

    /// Construct type from a ValueVariant
    [[nodiscard]] static Type FromVariant(const ValueVariant& value);

    /// Get the kind of type
    [[nodiscard]] inline TypeKind kind() const { return mKind; }

    /// Check if this is an elementary type
    [[nodiscard]] inline bool isElementary() const { return mKind != TypeKind::Tuple; }

    /// Check if this is a tuple type
    [[nodiscard]] inline bool isTuple() const { return mKind == TypeKind::Tuple; }

    /// For tuple types, get component types
    [[nodiscard]] inline const std::vector<Type>& components() const
    {
        PEXPR_ASSERT(isTuple(), "Not a tuple type");
        return mComponents;
    }

    /// Get number of components (1 for elementary types)
    [[nodiscard]] inline size_t size() const
    {
        return isTuple() ? mComponents.size() : 1;
    }

    /// Check if this type is an array (homogeneous tuple of numbers)
    [[nodiscard]] inline bool isVector() const
    {
        if (isTuple()) {
            // Check if all components are numbers
            for (const auto& comp : mComponents) {
                if (comp.kind() != TypeKind::Number)
                    return false;
            }
            return true;
        }
        return false;
    }

    /// Check if this type is arithmetic (int, num, or array)
    [[nodiscard]] inline bool isArithmetic() const
    {
        if (isElementary())
            return mKind == TypeKind::Integer || mKind == TypeKind::Number;
        return isVector();
    }

    /// Equality comparison
    [[nodiscard]] bool operator==(const Type& other) const = default;
    [[nodiscard]] inline friend std::strong_ordering operator<=>(const Type& aType, const Type& bType)
    {
        if (!aType.isTuple())
            return aType.kind() <=> bType.kind();

        if (auto kindCmp = aType.kind() <=> bType.kind(); kindCmp != std::strong_ordering::equal)
            return kindCmp;

        if (auto sizeCmp = aType.size() <=> bType.size(); sizeCmp != std::strong_ordering::equal)
            return sizeCmp;

        for (size_t i = 0; i < aType.size(); ++i) {
            if (auto compCmp = aType.mComponents[i] <=> bType.mComponents[i]; compCmp != std::strong_ordering::equal)
                return compCmp;
        }

        return std::strong_ordering::equal;
    };

    [[nodiscard]] inline size_t hash() const
    {
        size_t hash = std::hash<TypeKind>{}(mKind);
        if (isTuple()) {
            for (const auto& c : mComponents)
                hash = hash * 31 + c.hash();
        }
        return hash;
    }

    /// Returns printable representation of the given type.
    [[nodiscard]] std::string toString() const;

private:
    TypeKind mKind;
    std::vector<Type> mComponents; // Only used when mKind == Tuple
};

/// Checks if a conversion from one type to another is possible.
inline bool isConvertible(const Type& from, const Type& to)
{
    if (from.kind() == TypeKind::Error || to.kind() == TypeKind::Error)
        return false;

    if (from.kind() == TypeKind::Unspecified || to.kind() == TypeKind::Unspecified)
        return false;

    if (from == to)
        return true;

    // We only allow implicit conversion from integer to float, but not back!
    if (from.isElementary() && to.isElementary()) {
        return from.kind() == TypeKind::Integer && to.kind() == TypeKind::Number;
    }

    // Tuple conversions: component-wise conversion
    if (from.isTuple() && to.isTuple()) {
        if (from.size() != to.size())
            return false;
        for (size_t i = 0; i < from.size(); ++i) {
            if (!isConvertible(from.components()[i], to.components()[i]))
                return false;
        }
        return true;
    }

    // Vector (homogeneous tuple of numbers) to vector conversion with same size
    if (from.isVector() && to.isVector() && from.size() == to.size())
        return true;

    return false;
}

/// Checks if an explicit conversion (cast) from one type to another is allowed.
inline bool isExplicitConvertible(const Type& from, const Type& to)
{
    // Implicit conversions are always allowed explicitly as well.
    if (isConvertible(from, to))
        return true;

    if (from.isElementary() && to.isElementary()) {
        // Allow explicit narrowing from num to int.
        return from.kind() == TypeKind::Number && to.kind() == TypeKind::Integer;
    }

    // Tuple casts: component-wise explicit conversion
    if (from.isTuple() && to.isTuple()) {
        if (from.size() != to.size())
            return false;
        for (size_t i = 0; i < from.size(); ++i) {
            if (!isExplicitConvertible(from.components()[i], to.components()[i]))
                return false;
        }
        return true;
    }

    // Vector to vector with same size (homogeneous tuple of numbers)
    if (from.isVector() && to.isVector() && from.size() == to.size())
        return true;

    return false;
}

/// Checks if a conversion from one type to another is possible.
inline bool isConvertible(const Type& from, TypeKind to) { return isConvertible(from, Type(to)); }

/// Checks if an explicit conversion (cast) from one type to another is allowed.
inline bool isExplicitConvertible(const Type& from, TypeKind to) { return isExplicitConvertible(from, Type(to)); }

} // namespace PExpr
