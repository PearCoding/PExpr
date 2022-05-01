#pragma once

#include "PExpr_Config.h"

#include <mutex>
#include <streambuf>
#include <string_view>
#include <vector>

namespace PExpr {
/// Available verbosity levels.
enum class LogLevel {
    Debug = 0,
    Info,
    Warning,
    Error,
    Fatal
};

class LogListener;

namespace internal {
class ConsoleLogListener;
}

/// Simple logging class.
class Logger {
public:
    /// Buffer used by the logging class.
    class StreamBuf final : public std::streambuf {
    public:
        inline StreamBuf(Logger& logger, bool ignore)
            : std::streambuf()
            , mLogger(logger)
            , mIgnore(ignore)
        {
        }
        std::streambuf::int_type overflow(std::streambuf::int_type c);

    private:
        Logger& mLogger;
        bool mIgnore;
    };

    /// A closure used to make calls threadsafe. Never capture the ostream, else threadsafety is not guaranteed.
    class LogClosure {
    public:
        inline explicit LogClosure(Logger& logger)
            : mLogger(logger)
            , mStarted(false)
        {
        }

        inline ~LogClosure()
        {
            if (mStarted)
                mLogger.mMutex.unlock();
        }

        inline std::ostream& log(LogLevel level)
        {
            if ((int)level >= (int)mLogger.verbosity()) {
                // Only lock if verbosity is high enough,
                // else nothing will be output anyway
                mStarted = true;
                mLogger.mMutex.lock();
            }
            return mLogger.startEntry(level);
        }

    private:
        Logger& mLogger;
        bool mStarted;
    };

    Logger();
    ~Logger() = default;

    /// String representation of the given log level.
    static std::string_view levelString(LogLevel l);

    /// Add a custom listener.
    void addListener(const std::shared_ptr<LogListener>& listener);
    /// Remove a custom listener.
    void removeListener(const std::shared_ptr<LogListener>& listener);

    /// Set level of verbosity.
    /// Only message of greater or same level will be forwarded to the available listeners.
    inline void setVerbosity(LogLevel level) { mVerbosity = level; }
    /// Current level of verbosity.
    /// Only message of greater or same level will be forwarded to the available listeners.
    inline LogLevel verbosity() const { return mVerbosity; }

    /// If true, the internal output to the console will be prohibited.
    void setQuiet(bool b);
    /// If true, the internal output to the console will be prohibited.
    inline bool isQuiet() const { return mQuiet; }

    /// If true, the internal console will use ASCII colors.
    void enableAnsiTerminal(bool b);
    /// If true, the internal console will use ASCII colors.
    bool isUsingAnsiTerminal() const;

    std::ostream& startEntry(LogLevel level);

    static inline Logger& instance()
    {
        static Logger this_log;
        return this_log;
    }

    static inline std::ostream& log(LogLevel level)
    {
        return instance().startEntry(level);
    }

    static inline LogClosure threadsafe()
    {
        return LogClosure(instance());
    }

private:
    std::vector<std::shared_ptr<LogListener>> mListener;
    std::shared_ptr<internal::ConsoleLogListener> mConsoleLogListener;

    LogLevel mVerbosity;
    bool mQuiet;

    StreamBuf mEmptyStreamBuf;
    std::ostream mEmptyStream;

    StreamBuf mStreamBuf;
    std::ostream mStream;
    std::mutex mMutex;
};
} // namespace PExpr

/// Global logger instance.
#define PEXPR_LOGGER (PExpr::Logger::instance())
/// Unsafe logging function. Not threadsafe.
#define PEXPR_LOG_UNSAFE(l) (PExpr::Logger::log((l)))
/// Threadsafe logging function.
#define PEXPR_LOG(l) (PExpr::Logger::threadsafe().log((l)))
