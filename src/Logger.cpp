#include "Logger.h"
#include "internal/ConsoleLogListener.h"

#include <iostream>

namespace PExpr {
Logger::Logger()
    : mConsoleLogListener(std::make_shared<internal::ConsoleLogListener>(
#ifdef PEXPR_OS_LINUX
        true
#else
        false
#endif
        ))
#ifdef PEXPR_DEBUG
    , mVerbosity(LogLevel::Debug)
#else
    , mVerbosity(LogLevel::Info)
#endif
    , mQuiet(false)
    , mEmptyStreamBuf(*this, true)
    , mEmptyStream(&mEmptyStreamBuf)
    , mStreamBuf(*this, false)
    , mStream(&mStreamBuf)
{
    addListener(mConsoleLogListener);
}

static std::string_view levelStr[] = {
    "Debug  ",
    "Info   ",
    "Warning",
    "Error  ",
    "Fatal  "
};

std::string_view Logger::levelString(LogLevel l)
{
    return levelStr[(int)l];
}

void Logger::setQuiet(bool b)
{
    if (mQuiet == b)
        return;

    if (!b)
        addListener(mConsoleLogListener);
    else
        removeListener(mConsoleLogListener);
    mQuiet = b;
}

void Logger::enableAnsiTerminal(bool b)
{
    mConsoleLogListener->enableAnsi(b);
}

bool Logger::isUsingAnsiTerminal() const
{
    return mConsoleLogListener->isUsingAnsi();
}

void Logger::addListener(const std::shared_ptr<LogListener>& listener)
{
    mListener.push_back(listener);
}

void Logger::removeListener(const std::shared_ptr<LogListener>& listener)
{
    mListener.erase(std::remove(mListener.begin(), mListener.end(), listener), mListener.end());
}

std::ostream& Logger::startEntry(LogLevel level)
{
    if ((int)level < (int)verbosity())
        return mEmptyStream;

    for (const auto& listener : mListener)
        listener->startEntry(level);

    return mStream;
}

std::streambuf::int_type Logger::StreamBuf::overflow(std::streambuf::int_type c)
{
    if (mIgnore)
        return 0;

    for (const auto& listener : mLogger.mListener)
        listener->writeEntry((char)c);

    return 0;
}
} // namespace PExpr