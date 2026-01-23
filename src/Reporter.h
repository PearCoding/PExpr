#pragma once

#include "Location.h"
#include "Logger.h"

#include <cstdarg>
#include <mutex>
#include <string>
#include <vector>

namespace PExpr {

enum ReportType {
    RT_ERROR = 0x1, ///< All type of errors. These can not be disabled individually.

    RT_WARNING_TRAILING_SEMICOLON      = 0x100, ///< Warn about a trailing semicolon in expressions.
    RT_WARNING_IMPLICIT_CAST           = 0x200, ///< Warn about all implicit casts
    RT_WARNING_IMPLICIT_CAST_INT       = 0x400, ///< Warn about implicit casts from `int` to `num`.
    RT_WARNING_PURE_INTERNAL_FUNCTIONS = 0x800, ///< Warn when internal functions are marked as pure (which they are by default)

    RT_WARNING_DEFAULT = RT_WARNING_TRAILING_SEMICOLON | RT_WARNING_PURE_INTERNAL_FUNCTIONS,
    RT_WARNING_ALL     = RT_WARNING_DEFAULT | RT_WARNING_IMPLICIT_CAST,
};

struct ReporterEntry {
    LogLevel Level;
    Location Loc;
    std::string Message;
    ReportType Type;
};

/// Reporter collects diagnostics (errors, warnings)
class Reporter {
public:
    Reporter();
    ~Reporter();

    Reporter(const Reporter&)            = delete;
    Reporter& operator=(const Reporter&) = delete;

    // Configuration
    inline void setOutputMask(uint32_t mask) { mOutputMask = (mask | RT_ERROR); }
    [[nodiscard]] inline uint32_t outputMask() const { return mOutputMask; }

    inline void setQuiet(bool b) { mQuiet = b; }
    [[nodiscard]] inline bool isQuiet() const { return mQuiet; }

    // Core API: store and forward a diagnostic
    void report(LogLevel level, ReportType type, const Location& loc, const std::string& message);
    void error(const Location& loc, const std::string& message);
    void warning(ReportType type, const Location& loc, const std::string& message);

    // printf-style APIs
    void reportf(LogLevel level, ReportType type, const Location& loc, const char* fmt, ...);
    void errorf(const Location& loc, const char* fmt, ...);
    void warningf(ReportType type, const Location& loc, const char* fmt, ...);

    // Query
    [[nodiscard]] size_t errorCount() const;
    [[nodiscard]] size_t warningCount() const;
    [[nodiscard]] std::vector<ReporterEntry> entries() const;
    void clear();

private:
    // Helper to format varargs
    std::string vformat(const char* fmt, va_list args) const;

    mutable std::mutex mMutex;
    std::vector<ReporterEntry> mEntries;
    uint32_t mOutputMask;
    bool mQuiet;
    size_t mErrorCount;
    size_t mWarningCount;
};

} // namespace PExpr
