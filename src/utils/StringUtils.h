#pragma once

#include "PExpr.h"

namespace PExpr::utils {
/// Escape special characters in a string for serialization
[[nodiscard]] std::string escapeString(const std::string& str);

/// Unescape special characters in a string after deserialization
[[nodiscard]] std::string unescapeString(const std::string& str);
} // namespace PExpr::utils