#pragma once

#include "PExpr.h"

#include <string>
#include <unordered_map>

namespace PExpr::rvm {

/// String table for string literals
class RVMStringTable {
public:
    uint32_t addString(const std::string& str);
    [[nodiscard]] const std::string& getString(uint32_t id) const;
    [[nodiscard]] bool contains(const std::string& str) const;
    [[nodiscard]] std::optional<uint32_t> getId(const std::string& str) const;
    [[nodiscard]] size_t size() const { return mStrings.size(); }

private:
    std::vector<std::string> mStrings;
    std::unordered_map<std::string, uint32_t> mStringToId;
};
} // namespace PExpr::rvm
