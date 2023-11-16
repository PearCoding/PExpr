#pragma once

#include "PExpr_Config.h"

#include <iomanip>
#include <sstream>

namespace PExpr {
/// Location within the given expression stream.
class Location {
public:
    inline explicit Location(size_t pos)
        : mColumn(pos)
        , mLine(1)
    {
    }

    inline size_t line() const { return mLine; }
    inline size_t column() const { return mColumn; }
    inline Location& operator++()
    {
        mColumn++;
        return *this;
    }

    inline void newLine()
    {
        mLine++;
        mColumn = 0;
    }

private:
    size_t mColumn;
    size_t mLine;
};

inline Location operator+(const Location& loc, size_t i)
{
    return Location(loc.column() + i);
}

inline Location operator+(size_t i, const Location& loc)
{
    return loc + i;
}

inline Location operator-(const Location& loc, size_t i)
{
    return Location(loc.column() - i);
}

inline std::ostream& operator<<(std::ostream& os, const Location& loc)
{
    os << "(:" << loc.line() << ":" << loc.column() << ")";
    return os;
}
} // namespace PExpr