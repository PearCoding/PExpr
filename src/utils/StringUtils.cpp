#include "StringUtils.h"

namespace PExpr::utils {

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