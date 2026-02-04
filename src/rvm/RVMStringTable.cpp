#include "RVMStringTable.h"

namespace PExpr::rvm {

uint32_t RVMStringTable::addString(const std::string& str)
{
    auto it = mStringToId.find(str);
    if (it != mStringToId.end()) {
        return it->second;
    }

    uint32_t id = static_cast<uint32_t>(mStrings.size());
    mStrings.push_back(str);
    mStringToId[str] = id;
    return id;
}

const std::string& RVMStringTable::getString(uint32_t id) const
{
    PEXPR_ASSERT(id < mStrings.size(), "Invalid string ID");
    return mStrings[id];
}

bool RVMStringTable::contains(const std::string& str) const
{
    return mStringToId.find(str) != mStringToId.end();
}

std::optional<uint32_t> RVMStringTable::getId(const std::string& str) const
{
    auto it = mStringToId.find(str);
    if (it != mStringToId.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace PExpr::rvm