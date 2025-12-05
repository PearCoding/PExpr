#pragma once

#include "Location.h"
#include "Logger.h"

#include <cstdarg>
#include <mutex>
#include <string>
#include <vector>

namespace PExpr {

struct ReporterEntry {
    LogLevel Level;
    Location Loc;
    std::string Message;
};

/// Reporter collects diagnostics (errors, warnings)
class Reporter {
public:
    Reporter();
    ~Reporter();

    Reporter(const Reporter&)            = delete;
    Reporter& operator=(const Reporter&) = delete;

    // Configuration
    void setWarningsEnabled(bool enabled);
    bool warningsEnabled() const;

    inline void setQuiet(bool b) { mQuiet = b; }
    [[nodiscard]] inline bool isQuiet() const { return mQuiet; }

    // Core API: store and forward a diagnostic (string)
    void report(LogLevel level, const Location& loc, const std::string& message);
    void error(const Location& loc, const std::string& message);
    void warning(const Location& loc, const std::string& message);

    // printf-style APIs (safer for many call-sites)
    void reportf(LogLevel level, const Location& loc, const char* fmt, ...);
    void errorf(const Location& loc, const char* fmt, ...);
    void warningf(const Location& loc, const char* fmt, ...);

    // Query
    size_t errorCount() const;
    size_t warningCount() const;
    std::vector<ReporterEntry> entries() const;
    void clear();

private:
    // Helper to format varargs
    std::string vformat(const char* fmt, va_list args) const;

    mutable std::mutex mMutex;
    std::vector<ReporterEntry> mEntries;
    bool mWarningsEnabled;
    bool mQuiet;
    size_t mErrorCount;
    size_t mWarningCount;
};

} // namespace PExpr
