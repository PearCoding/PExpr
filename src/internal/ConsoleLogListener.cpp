#include "ConsoleLogListener.h"

namespace PExpr::internal {

constexpr const char* const ANSI_BLACK         = "\u001b[0;30m";
constexpr const char* const ANSI_RED           = "\u001b[0;31m";
constexpr const char* const ANSI_GREEN         = "\u001b[0;32m";
constexpr const char* const ANSI_BROWN         = "\u001b[0;33m";
constexpr const char* const ANSI_BLUE          = "\u001b[0;34m";
constexpr const char* const ANSI_MAGENTA       = "\u001b[0;35m";
constexpr const char* const ANSI_CYAN          = "\u001b[0;36m";
constexpr const char* const ANSI_GRAY          = "\u001b[0;37m";
constexpr const char* const ANSI_DARK_GRAY     = "\u001b[1;30m";
constexpr const char* const ANSI_LIGHT_RED     = "\u001b[1;31m";
constexpr const char* const ANSI_LIGHT_GREEN   = "\u001b[1;32m";
constexpr const char* const ANSI_YELLOW        = "\u001b[1;33m";
constexpr const char* const ANSI_LIGHT_BLUE    = "\u001b[1;34m";
constexpr const char* const ANSI_LIGHT_MAGENTA = "\u001b[1;35m";
constexpr const char* const ANSI_LIGHT_CYAN    = "\u001b[1;36m";
constexpr const char* const ANSI_WHITE         = "\u001b[1;37m";
constexpr const char* const ANSI_RESET         = "\u001b[0m";

ConsoleLogListener::ConsoleLogListener(bool useAnsi)
    : LogListener()
    , mUseAnsi(useAnsi)
{
}

void ConsoleLogListener::startEntry(LogLevel level)
{
    std::cout << "[";
    if (mUseAnsi) {
        switch (level) {
        case LogLevel::Debug:
            std::cout << ANSI_DARK_GRAY;
            break;
        case LogLevel::Info:
            break;
        case LogLevel::Warning:
            std::cout << ANSI_YELLOW;
            break;
        case LogLevel::Error:
            std::cout << ANSI_LIGHT_RED;
            break;
        case LogLevel::Fatal:
            std::cout << ANSI_RED;
            break;
        }
    }

    std::cout << Logger::levelString(level);

    if (mUseAnsi) {
        std::cout << ANSI_RESET;
    }

    std::cout << "] ";
}

void ConsoleLogListener::writeEntry(char c)
{
    std::cout.put(c);
}
} // namespace PExpr::internal
