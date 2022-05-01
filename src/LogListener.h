#pragma once

#include "Logger.h"
#include <string>

namespace PExpr {

/// A log listener which can be registred to the current logger.
class LogListener {
public:
    LogListener()          = default;
    virtual ~LogListener() = default;

    virtual void startEntry(LogLevel level) = 0;
    virtual void writeEntry(char c)         = 0;
};
} // namespace PExpr
