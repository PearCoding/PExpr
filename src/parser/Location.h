#pragma once

#include "PExpr.h"

#include <iomanip>
#include <sstream>

namespace PExpr::parser {
/// Location within the given expression stream.
class Location {
public:
    inline explicit Location(size_t pos)
        : mColumn(pos)
        , mLine(1)
    {
    }

    inline explicit Location(size_t line, size_t col)
        : mColumn(col)
        , mLine(line)
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

    inline friend Location operator+(const Location& loc, size_t i)
    {
        return Location(loc.line(), loc.column() + i);
    }

    inline friend Location operator+(size_t i, const Location& loc)
    {
        return loc + i;
    }

    inline friend std::ostream& operator<<(std::ostream& os, const Location& loc)
    {
        os << "(:" << loc.line() << ":" << loc.column() << ")";
        return os;
    }

private:
    size_t mColumn;
    size_t mLine;
};
} // namespace PExpr