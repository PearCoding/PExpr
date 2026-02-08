#include "FuzzRunner.h"
#include "log/Logger.h"

#include <cstdlib>
#include <iostream>
#include <numeric>
#include <string>

using namespace PExpr::fuzzy;

int main(int argc, char** argv)
{
    // Default configuration
    size_t maxIterations = 100;
    uint64_t seed        = 12345;
    bool verbose         = true;
    bool runAllTests     = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl
                      << "Options:" << std::endl
                      << "  -h, --help            Show this help message" << std::endl
                      << "  -n, --iterations NUM  Set maximum iterations (default: 100)" << std::endl
                      << "  -s, --seed SEED       Set random seed (default: 12345)" << std::endl
                      << "  -v, --verbose         Enable verbose output" << std::endl
                      << "  -q, --quiet           Disable verbose output" << std::endl
                      << "  --all                 Run all test suites" << std::endl
                      << "  --parser              Run only parser fuzzing" << std::endl
                      << "  --options             Run only compiler option fuzzing" << std::endl
                      << "  --mistakes            Run only user mistake fuzzing" << std::endl
                      << "  --integration         Run only integration fuzzing" << std::endl;
            return EXIT_SUCCESS;
        } else if (arg == "-n" || arg == "--iterations") {
            if (i + 1 < argc)
                maxIterations = std::stoul(argv[++i]);
        } else if (arg == "-s" || arg == "--seed") {
            if (i + 1 < argc)
                seed = std::stoull(argv[++i]);
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            verbose = false;
        } else if (arg == "--all") {
            runAllTests = true;
        }
    }

    PEXPR_LOGGER.setVerbosity(verbose ? PExpr::log::LogLevel::Debug : PExpr::log::LogLevel::Info);

    PEXPR_LOG_INFO << "=== PExpr Fuzzy Testing ===" << std::endl
                   << "Seed: " << seed << std::endl
                   << "Max iterations: " << maxIterations << std::endl
                   << "Verbose: " << (verbose ? "yes" : "no") << std::endl;

    FuzzRunner runner;
    runner.setMaxIterations(maxIterations);
    runner.setSeed(seed);
    runner.setVerbose(verbose);

    const auto join = [](std::span<const std::string> array, const std::string& delim) {
        return std::accumulate(std::next(array.begin()), array.end(), array[0],
                               [delim](const std::string& a, const std::string& b) {
                                   return a + delim + b;
                               });
    };

    int exitCode = EXIT_SUCCESS;

    if (runAllTests || (argc == 1)) {
        // Run all test suites by default
        PEXPR_LOG_INFO << "Running all fuzzy test suites..." << std::endl;
        auto results = runner.runAllTests();

        // Check for crashes
        for (const auto& result : results) {
            if (result.crashed) {
                std::stringstream info;
                if (!result.options.empty())
                    info << "Options: " << join(result.options, ", ");

                PEXPR_LOG_ERROR << "Crash detected!" << std::endl
                                << "Input: " << result.input << std::endl
                                << "Crash info: " << result.crashInfo << std::endl
                                << info.str();

                exitCode = EXIT_FAILURE;
            }
        }

        if (exitCode == EXIT_SUCCESS)
            PEXPR_LOG_INFO << "All fuzzy tests completed successfully!" << std::endl;
    } else {
        // Run individual test suites based on command line arguments
        bool anyTestRun = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--parser") {
                anyTestRun = true;
                PEXPR_LOG_INFO << "Running parser fuzzing..." << std::endl;
                auto result = runner.runParserFuzzing();
                if (result.crashed) {
                    PEXPR_LOG_ERROR << "Crash detected in parser fuzzing!" << std::endl
                                    << "Input: " << result.input << std::endl
                                    << "Crash info: " << result.crashInfo << std::endl;
                    exitCode = EXIT_FAILURE;
                }
            } else if (arg == "--options") {
                anyTestRun = true;
                PEXPR_LOG_INFO << "Running compiler option fuzzing..." << std::endl;
                auto result = runner.runCompilerOptionFuzzing();
                if (result.crashed) {
                    PEXPR_LOG_ERROR << "Crash detected in compiler option fuzzing!" << std::endl
                                    << "Input: " << result.input << std::endl
                                    << "Options: " << join(result.options, ", ") << std::endl
                                    << "Crash info: " << result.crashInfo << std::endl;
                    exitCode = EXIT_FAILURE;
                }
            } else if (arg == "--mistakes") {
                anyTestRun = true;
                PEXPR_LOG_INFO << "Running user mistake fuzzing..." << std::endl;
                auto result = runner.runUserMistakeFuzzing();
                if (result.crashed) {
                    PEXPR_LOG_ERROR << "Crash detected in user mistake fuzzing!" << std::endl
                                    << "Input: " << result.input << std::endl
                                    << "Crash info: " << result.crashInfo << std::endl;
                    exitCode = EXIT_FAILURE;
                }
            } else if (arg == "--integration") {
                anyTestRun = true;
                PEXPR_LOG_INFO << "Running integration fuzzing..." << std::endl;
                auto result = runner.runIntegrationFuzzing();
                if (result.crashed) {
                    PEXPR_LOG_ERROR << "Crash detected in integration fuzzing!" << std::endl
                                    << "Input: " << result.input << std::endl
                                    << "Options: " << join(result.options, ", ") << std::endl
                                    << "Crash info: " << result.crashInfo << std::endl;
                    exitCode = EXIT_FAILURE;
                }
            }
        }

        if (!anyTestRun) {
            PEXPR_LOG_INFO << "No specific test specified. Running all tests..." << std::endl;
            auto results = runner.runAllTests();

            for (const auto& result : results) {
                if (result.crashed) {
                    PEXPR_LOG_ERROR << "Crash detected!" << std::endl
                                    << "Input: " << result.input << std::endl
                                    << "Crash info: " << result.crashInfo << std::endl;
                    exitCode = EXIT_FAILURE;
                }
            }
        }

        if (exitCode == EXIT_SUCCESS)
            PEXPR_LOG_INFO << "Fuzzy tests completed successfully!" << std::endl;
    }

    return exitCode;
}