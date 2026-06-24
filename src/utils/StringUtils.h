#pragma once

#include "PExpr.h"

namespace PExpr::utils {
/// Escape special characters in a string for serialization
[[nodiscard]] std::string escapeString(const std::string& str);

/// Unescape special characters in a string after deserialization
[[nodiscard]] std::string unescapeString(const std::string& str);

/// Format a Number as the shortest decimal string that round-trips back to the
/// same value (used by the IR serializers so deserialize(serialize(x)) == x).
[[nodiscard]] std::string formatNumber(Number value);
} // namespace PExpr::utils