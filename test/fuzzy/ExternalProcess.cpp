#include "ExternalProcess.h"
#include "log/Logger.h"

#include <cstring>
#include <fstream>
#include <thread>

#ifdef PEXPR_OS_LINUX
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fstream>

#elif defined(PEXPR_OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <codecvt>
#include <locale>

#else
#error Process implementation missing
#endif

namespace PExpr {

#ifdef PEXPR_OS_LINUX
class ExternalProcessInternal {
public:
    const std::filesystem::path exePath;
    const std::vector<std::string> cmdParameters;

    pid_t pid;
    mutable int exit_code;

    inline ExternalProcessInternal(const std::filesystem::path& exe, const std::vector<std::string>& parameters)
        : exePath(exe)
        , cmdParameters(parameters)
        , pid(-1)
        , exit_code(-1)
    {
    }

    inline ~ExternalProcessInternal()
    {
    }

    inline bool start()
    {
        // Prepare for child
        const std::string path  = exePath.string();
        const char** parameters = new const char*[cmdParameters.size() + 2];

        parameters[0] = path.c_str();
        for (size_t i = 0; i < cmdParameters.size(); ++i)
            parameters[i + 1] = cmdParameters[i].c_str();
        parameters[cmdParameters.size() + 1] = nullptr;

        // Init process
        pid = fork();

        if (pid == 0) {
            // -> Child process
            if (execv(parameters[0], (char**)parameters) == -1)
                PEXPR_LOG_FATAL << "fork/exec of process failed: " << std::strerror(errno) << std::endl;

            std::exit(-1);

#if PEXPR_CC_MSC
            __assume(false);
#else // GCC, Clang
            __builtin_unreachable();
#endif
        } else {
            // -> Parent process
            delete[] parameters;

            // Check for error
            if (pid == -1) {
                PEXPR_LOG_ERROR << "Fork of process " << exePath << " failed: " << std::strerror(errno) << std::endl;
                return false;
            }
        }

        return true;
    }

    inline int exitCode() const
    {
        return exit_code;
    }

    inline void waitForFinish()
    {
        if (pid == -1)
            return;

        int status;
        if (waitpid(pid, &status, 0) < 0) {
            PEXPR_LOG_ERROR << "waitpid for " << exePath << " (" << pid << ") failed: " << std::strerror(errno) << std::endl;
            return;
        }

        if (WIFEXITED(status))
            exit_code = WEXITSTATUS(status);
    }
};

#elif defined(PEXPR_OS_WINDOWS)

static inline std::wstring s2ws(const std::string& str)
{
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.from_bytes(str);
}

class ExternalProcessInternal {
public:
    const std::filesystem::path exePath;
    const std::vector<std::string> cmdParameters;

    PROCESS_INFORMATION pi;

    inline ExternalProcessInternal(const std::filesystem::path& exe, const std::vector<std::string>& parameters)
        : exePath(exe)
        , cmdParameters(parameters)
    {
        pi.hProcess = INVALID_HANDLE_VALUE;
        pi.hThread  = INVALID_HANDLE_VALUE;
    }

    inline ~ExternalProcessInternal()
    {
        if (pi.hProcess != INVALID_HANDLE_VALUE)
            CloseHandle(pi.hProcess);

        if (pi.hThread != INVALID_HANDLE_VALUE)
            CloseHandle(pi.hThread);
    }

    inline bool start()
    {
        // Setup commandline
        std::wstringstream cmdLineStream;
        cmdLineStream << exePath << L" ";
        for (const auto& str : cmdParameters)
            cmdLineStream << s2ws(str) << L" ";
        std::wstring cmdLine = cmdLineStream.str();

        // Setup security stuff
        SECURITY_ATTRIBUTES saAttr;

        ZeroMemory(&saAttr, sizeof(saAttr));
        saAttr.nLength              = sizeof(saAttr);
        saAttr.bInheritHandle       = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        // Setup process
        STARTUPINFOW si;
        ZeroMemory(&si, sizeof(si));
        si.cb         = sizeof(si);
        si.hStdError  = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdInput  = NULL;
        si.dwFlags |= STARTF_USESTDHANDLES;

        ZeroMemory(&pi, sizeof(pi));

        // Start the child process.
        if (!CreateProcessW(exePath.native().data(),                    // Module name (use command line)
                            cmdLine.data(),                             // Command line
                            NULL,                                       // Process handle not inheritable
                            NULL,                                       // Thread handle not inheritable
                            TRUE,                                       // Set handle inheritance to TRUE
                            CREATE_NO_WINDOW | INHERIT_PARENT_AFFINITY, // Only keep the affinity and do not create a new console window if parent is closed
                            NULL,                                       // Use parent's environment block
                            NULL,                                       // Use parent's starting directory
                            &si,                                        // Pointer to STARTUPINFO structure
                            &pi)                                        // Pointer to PROCESS_INFORMATION structure
        ) {
            PEXPR_LOG_ERROR << "CreateProcess failed: " << std::system_category().message(GetLastError()) << std::endl;
            return false;
        }

        // The following handles are not needed
        if (pi.hThread != INVALID_HANDLE_VALUE) {
            CloseHandle(pi.hThread);
            pi.hThread = INVALID_HANDLE_VALUE;
        }

        return true;
    }

    inline int exitCode() const
    {
        DWORD exit_code;
        if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
            PEXPR_LOG_ERROR << "GetExitCodeProcess failed: " << std::system_category().message(GetLastError()) << std::endl;
            return -1;
        }
        return exit_code;
    }

    inline void waitForFinish()
    {
        if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
            PEXPR_LOG_ERROR << "WaitForSingleObject failed: " << std::system_category().message(GetLastError()) << std::endl;
    }
};
#endif

// -----------------------------------------------------------------------------------

ExternalProcess::ExternalProcess(const std::filesystem::path& exe, const std::vector<std::string>& parameters)
    : mInternal(new ExternalProcessInternal(exe, parameters))
{
}

ExternalProcess::~ExternalProcess()
{
}

int ExternalProcess::run()
{
    if (mInternal) {
        if (!mInternal->start())
            return 1;
        mInternal->waitForFinish();
        return mInternal->exitCode();
    }

    return 1;
}

int ExternalProcess::exitCode() const
{
    return mInternal ? mInternal->exitCode() : 0;
}
} // namespace PExpr