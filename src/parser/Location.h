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

    [[nodiscard]] inline friend std::strong_ordering operator<=>(const Location& a, const Location& b)
    {
        if (const auto cmp = a.line() <=> b.line(); cmp != std::strong_ordering::equal)
            return cmp;

        return a.column() <=> b.column();
    }

private:
    size_t mColumn;
    size_t mLine;
};
} // namespace PExpr::parser

namespace std {
template <>
class hash<PExpr::parser::Location> {
public:
    std::size_t operator()(const PExpr::parser::Location& loc) const
    {
        const auto h1 = std::hash<size_t>{}(loc.line());
        const auto h2 = std::hash<size_t>{}(loc.column());
        return h1 ^ (h2 << 1);
    }
};
} // namespace std