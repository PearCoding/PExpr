#include "Reporter.h"
#include "Logger.h"

namespace PExpr {

Reporter::Reporter()
    : mOutputMask(RT_WARNING_DEFAULT | RT_ERROR)
    , mErrorCount(0)
    , mWarningCount(0)
{
}

Reporter::~Reporter() = default;

std::string Reporter::vformat(const char* fmt, va_list args) const
{
    // Try formatting into a fixed buffer first
    va_list tmp;
    va_copy(tmp, args);
    int needed = std::vsnprintf(nullptr, 0, fmt, tmp);
    va_end(tmp);
    if (needed < 0)
        throw std::runtime_error("vformat encoding error");

    std::string buf;
    buf.resize((size_t)needed + 1);
    va_copy(tmp, args);
    std::vsnprintf(&buf[0], buf.size(), fmt, tmp);
    va_end(tmp);
    buf.pop_back(); // remove trailing null
    return buf;
}

void Reporter::report(LogLevel level, ReportType type, const Location& loc, const std::string& message)
{
    {
        std::lock_guard<std::mutex> l(mMutex);
        mEntries.push_back(ReporterEntry{ level, loc, message, type });
        if (level == LogLevel::Error)
            ++mErrorCount;
        else if (level == LogLevel::Warning && (mOutputMask & type) == type)
            ++mWarningCount;
    }

    if (isQuiet())
        return;

    // Forward to console using existing Logger, but respect warnings flag.
    switch (level) {
    case LogLevel::Error:
        PEXPR_LOG(LogLevel::Error) << loc << ": " << message << std::endl;
        break;
    case LogLevel::Warning:
        if ((mOutputMask & type) == type)
            PEXPR_LOG(LogLevel::Warning) << loc << ": " << message << std::endl;
        break;
    case LogLevel::Info:
        PEXPR_LOG(LogLevel::Info) << loc << ": " << message << std::endl;
        break;
    case LogLevel::Debug:
        PEXPR_LOG(LogLevel::Debug) << loc << ": " << message << std::endl;
        break;
    case LogLevel::Fatal:
        PEXPR_LOG(LogLevel::Fatal) << loc << ": " << message << std::endl;
        break;
    }
}

void Reporter::reportf(LogLevel level, ReportType type, const Location& loc, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::string msg;
    try {
        msg = vformat(fmt, args);
    } catch (...) {
        va_end(args);
        throw;
    }
    va_end(args);
    report(level, type, loc, msg);
}

void Reporter::errorf(const Location& loc, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::string msg;
    try {
        msg = vformat(fmt, args);
    } catch (...) {
        va_end(args);
        throw;
    }
    va_end(args);
    report(LogLevel::Error, RT_ERROR, loc, msg);
}

void Reporter::warningf(ReportType type, const Location& loc, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::string msg;
    try {
        msg = vformat(fmt, args);
    } catch (...) {
        va_end(args);
        throw;
    }
    va_end(args);
    report(LogLevel::Warning, type, loc, msg);
}

void Reporter::error(const Location& loc, const std::string& message)
{
    report(LogLevel::Error, RT_ERROR, loc, message);
}

void Reporter::warning(ReportType type, const Location& loc, const std::string& message)
{
    report(LogLevel::Warning, type, loc, message);
}

size_t Reporter::errorCount() const
{
    std::lock_guard<std::mutex> l(mMutex);
    return mErrorCount;
}

size_t Reporter::warningCount() const
{
    std::lock_guard<std::mutex> l(mMutex);
    return mWarningCount;
}

std::vector<ReporterEntry> Reporter::entries() const
{
    std::lock_guard<std::mutex> l(mMutex);
    return mEntries;
}

void Reporter::clear()
{
    std::lock_guard<std::mutex> l(mMutex);
    mEntries.clear();
    mErrorCount   = 0;
    mWarningCount = 0;
}

} // namespace PExpr
