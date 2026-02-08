#include "ConsoleLogListener.h"

#ifdef PEXPR_OS_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace PExpr::log {

[[maybe_unused]] constexpr std::string_view ANSI_BLACK         = "\u001b[0;30m";
[[maybe_unused]] constexpr std::string_view ANSI_RED           = "\u001b[0;31m";
[[maybe_unused]] constexpr std::string_view ANSI_GREEN         = "\u001b[0;32m";
[[maybe_unused]] constexpr std::string_view ANSI_BROWN         = "\u001b[0;33m";
[[maybe_unused]] constexpr std::string_view ANSI_BLUE          = "\u001b[0;34m";
[[maybe_unused]] constexpr std::string_view ANSI_MAGENTA       = "\u001b[0;35m";
[[maybe_unused]] constexpr std::string_view ANSI_CYAN          = "\u001b[0;36m";
[[maybe_unused]] constexpr std::string_view ANSI_GRAY          = "\u001b[0;37m";
[[maybe_unused]] constexpr std::string_view ANSI_DARK_GRAY     = "\u001b[1;30m";
[[maybe_unused]] constexpr std::string_view ANSI_LIGHT_RED     = "\u001b[1;31m";
[[maybe_unused]] constexpr std::string_view ANSI_LIGHT_GREEN   = "\u001b[1;32m";
[[maybe_unused]] constexpr std::string_view ANSI_YELLOW        = "\u001b[1;33m";
[[maybe_unused]] constexpr std::string_view ANSI_LIGHT_BLUE    = "\u001b[1;34m";
[[maybe_unused]] constexpr std::string_view ANSI_LIGHT_MAGENTA = "\u001b[1;35m";
[[maybe_unused]] constexpr std::string_view ANSI_LIGHT_CYAN    = "\u001b[1;36m";
[[maybe_unused]] constexpr std::string_view ANSI_WHITE         = "\u001b[1;37m";
[[maybe_unused]] constexpr std::string_view ANSI_RESET         = "\u001b[0m";

ConsoleLogListener::ConsoleLogListener(bool useAnsi)
    : LogListener()
    , mUseAnsi(useAnsi)
{
#ifdef PEXPR_OS_WINDOWS
    // Enable Unicode output on Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (mUseAnsi) {
        const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (handle == INVALID_HANDLE_VALUE)
            mUseAnsi = false; // Failed
        if (SetConsoleMode(handle, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0)
            mUseAnsi = false; // Failed to setup color output
    }
#endif
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

    if (mUseAnsi)
        std::cout << ANSI_RESET;

    std::cout << "] ";
    std::cout.flush();
}

void ConsoleLogListener::writeEntry(char c)
{
    std::cout.put(c);
}
} // namespace PExpr::log
