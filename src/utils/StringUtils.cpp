#include "StringUtils.h"

#include <array>
#include <charconv>

namespace PExpr::utils {

std::string formatNumber(Number value)
{
    // std::to_chars with no format produces the shortest representation that
    // round-trips exactly, so precision is not lost on serialize/deserialize.
    std::array<char, 32> buffer;
    auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (ec != std::errc())
        return std::to_string(value); // fallback (should not happen for finite doubles)
    return std::string(buffer.data(), ptr);
}

std::string escapeString(const std::string& str)
{
    std::string result;
    result.reserve(str.size());

    for (char c : str) {
        switch (c) {
        case '\"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += c;
            break;
        }
    }

    return result;
}

std::string unescapeString(const std::string& str)
{
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
            case '\"':
                result += '\"';
                ++i;
                break;
            case '\\':
                result += '\\';
                ++i;
                break;
            case 'n':
                result += '\n';
                ++i;
                break;
            case 'r':
                result += '\r';
                ++i;
                break;
            case 't':
                result += '\t';
                ++i;
                break;
            default:
                result += str[i];
                break;
            }
        } else {
            result += str[i];
        }
    }

    return result;
}
} // namespace PExpr::utils