#pragma once

#include "pexpr_config.h"

#include <iomanip>
#include <sstream>

namespace PExpr {
/// Location within the given expression stream.
class Location {
public:
    inline explicit Location(size_t pos)
        : mPosition(pos)
    {
    }

    inline size_t position() const { return mPosition; }
    inline Location& operator++()
    {
        mPosition++;
        return *this;
    }

    inline std::string asPrefix() const
    {
        std::stringstream stream;
        stream << "("
               << std::setw(3)
               << mPosition
               << ")";
        return stream.str();
    }

private:
    size_t mPosition;
};

inline Location operator+(const Location& loc, size_t i)
{
    return Location(loc.position() + i);
}

inline Location operator+(size_t i, const Location& loc)
{
    return loc + i;
}

inline Location operator-(const Location& loc, size_t i)
{
    return Location(loc.position() - i);
}

inline std::ostream& operator<<(std::ostream& os, const Location& loc)
{
    os << "("
       << loc.position()
       << ")";
    return os;
}
} // namespace PExpr