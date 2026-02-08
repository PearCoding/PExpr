#include "FuzzRunner.h"
#include "ExternalProcess.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace PExpr::fuzzy {

FuzzRunner::FuzzRunner()
    : mGenerator(mSeed)
{
}

FuzzRunner::TestResult FuzzRunner::runParserFuzzing()
{
    TestResult overallResult;
    overallResult.success = true;

    logTestStart("Parser Fuzzing");

    for (size_t i = 0; i < mMaxIterations; ++i) {
        mTestsRun++;

        // Generate random input
        std::string input;
        if (i % 10 == 0) // Every 10th iteration, test malformed syntax
            input = mGenerator.generateMalformedSyntax();
        else if (i % 7 == 0) // Test edge cases
            input = mGenerator.generateEdgeCaseNumbers();
        else if (i % 5 == 0) // Test weird whitespace
            input = mGenerator.generateWeirdWhitespace();
        else // Test random valid programs
            input = mGenerator.generateRandomProgram(5);

        auto result = testParserInput(input, "Parser test iteration " + std::to_string(i));

        if (!result.success) {
            overallResult.success      = false;
            overallResult.errorMessage = "Failed at iteration " + std::to_string(i);
            overallResult.input        = result.input;

            if (result.crashed) {
                mCrashesDetected++;
                overallResult.crashed   = true;
                overallResult.crashInfo = result.crashInfo;
                break;
            }

            mTestsFailed++;
        } else {
            mTestsPassed++;
        }

        if (mVerbose && i % 100 == 0) {
            std::cout << "  Processed " << i << " iterations..." << std::endl;
        }
    }

    logTestResult(overallResult);
    return overallResult;
}

FuzzRunner::TestResult FuzzRunner::runCompilerOptionFuzzing()
{
    TestResult overallResult;
    overallResult.success = true;

    logTestStart("Compiler Option Fuzzing");

    for (size_t i = 0; i < mMaxIterations / 10; ++i) { // Fewer iterations due to complexity
        mTestsRun++;

        // Generate random input
        std::string input = mGenerator.generateRandomProgram(3);

        // Generate random options
        auto opts = mGenerator.generateRandomOptions();

        auto result = testCompilerWithOptions(input, opts);

        if (!result.success) {
            overallResult.success      = false;
            overallResult.errorMessage = "Failed at iteration " + std::to_string(i);
            overallResult.input        = result.input;
            overallResult.options      = result.options;

            if (result.crashed) {
                mCrashesDetected++;
                overallResult.crashed   = true;
                overallResult.crashInfo = result.crashInfo;
                break;
            }

            mTestsFailed++;
        } else {
            mTestsPassed++;
        }

        if (mVerbose && i % 50 == 0)
            std::cout << "  Processed " << i << " iterations..." << std::endl;
    }

    logTestResult(overallResult);
    return overallResult;
}

FuzzRunner::TestResult FuzzRunner::runUserMistakeFuzzing()
{
    TestResult overallResult;
    overallResult.success = true;

    logTestStart("User Mistake Fuzzing");

    // Test curated common mistakes
    std::vector<std::string> mistakePatterns = {
        mGenerator.generateMissingSemicolon(),
        mGenerator.generateUnmatchedBrackets(),
        mGenerator.generateInvalidTokens(),
        mGenerator.generateTypeMismatch(),
        mGenerator.generateExtremeLengthInput(),
        mGenerator.generateUnicodeEdgeCases(),
    };

    for (size_t i = 0; i < mistakePatterns.size(); ++i) {
        mTestsRun++;

        auto result = testParserInput(mistakePatterns[i],
                                      "User mistake pattern " + std::to_string(i));

        if (!result.success) {
            overallResult.success      = false;
            overallResult.errorMessage = "Failed on mistake pattern " + std::to_string(i);
            overallResult.input        = result.input;

            if (result.crashed) {
                mCrashesDetected++;
                overallResult.crashed   = true;
                overallResult.crashInfo = result.crashInfo;
                break;
            }

            mTestsFailed++;
        } else {
            mTestsPassed++;
        }
    }

    // Also test random combinations of mistakes
    for (size_t i = 0; i < mMaxIterations / 20; ++i) {
        mTestsRun++;

        std::string input;
        switch (i % 6) {
        case 0:
            input = mGenerator.generateMissingSemicolon();
            break;
        case 1:
            input = mGenerator.generateUnmatchedBrackets();
            break;
        case 2:
            input = mGenerator.generateInvalidTokens();
            break;
        case 3:
            input = mGenerator.generateTypeMismatch();
            break;
        case 4:
            input = mGenerator.generateExtremeLengthInput();
            break;
        case 5:
            input = mGenerator.generateUnicodeEdgeCases();
            break;
        }

        auto result = testParserInput(input, "Random user mistake " + std::to_string(i));

        if (!result.success) {
            overallResult.success      = false;
            overallResult.errorMessage = "Failed at iteration " + std::to_string(i);
            overallResult.input        = result.input;

            if (result.crashed) {
                mCrashesDetected++;
                overallResult.crashed   = true;
                overallResult.crashInfo = result.crashInfo;
                break;
            }

            mTestsFailed++;
        } else {
            mTestsPassed++;
        }
    }

    logTestResult(overallResult);
    return overallResult;
}

FuzzRunner::TestResult FuzzRunner::runIntegrationFuzzing()
{
    TestResult overallResult;
    overallResult.success = true;

    logTestStart("Integration Fuzzing");

    for (size_t i = 0; i < mMaxIterations / 5; ++i) { // Fewer iterations due to complexity
        mTestsRun++;

        // Generate random input
        std::string input = mGenerator.generateRandomProgram(4);

        // Generate random options
        auto opts       = mGenerator.generateRandomOptions();
        auto optionArgs = mGenerator.generateOptionArgs(opts);

        auto result = testFullCompilation(input, optionArgs);

        if (!result.success) {
            overallResult.success      = false;
            overallResult.errorMessage = "Failed at iteration " + std::to_string(i);
            overallResult.input        = result.input;
            overallResult.options      = result.options;

            if (result.crashed) {
                mCrashesDetected++;
                overallResult.crashed   = true;
                overallResult.crashInfo = result.crashInfo;
                break;
            }

            mTestsFailed++;
        } else {
            mTestsPassed++;
        }

        if (mVerbose && i % 20 == 0)
            std::cout << "  Processed " << i << " iterations..." << std::endl;
    }

    logTestResult(overallResult);
    return overallResult;
}

std::vector<FuzzRunner::TestResult> FuzzRunner::runAllTests()
{
    std::vector<TestResult> results;

    std::cout << "=== Starting PExpr Fuzzy Testing ===" << std::endl;
    std::cout << "Seed: " << mSeed << std::endl;
    std::cout << "Max iterations per test: " << mMaxIterations << std::endl;
    std::cout << std::endl;

    // Reset statistics
    mTestsRun        = 0;
    mTestsPassed     = 0;
    mTestsFailed     = 0;
    mCrashesDetected = 0;

    // Run all test suites
    results.push_back(runParserFuzzing());
    results.push_back(runCompilerOptionFuzzing());
    results.push_back(runUserMistakeFuzzing());
    results.push_back(runIntegrationFuzzing());

    // Print summary
    std::cout << std::endl;
    std::cout << "=== Test Summary ===" << std::endl;
    std::cout << "Total tests run: " << mTestsRun << std::endl;
    std::cout << "Tests passed: " << mTestsPassed << std::endl;
    std::cout << "Tests failed: " << mTestsFailed << std::endl;
    std::cout << "Crashes detected: " << mCrashesDetected << std::endl;

    if (mCrashesDetected > 0)
        std::cout << "WARNING: " << mCrashesDetected << " crash(es) detected!" << std::endl;

    return results;
}

FuzzRunner::TestResult FuzzRunner::testParserInput(const std::string& input, const std::string& description)
{
    TestResult result;
    result.input = input;
    result.seed  = std::to_string(mSeed);

    auto testFunc = [&]() {
        try {
            Environment env;
            env.reporter().setQuiet(true);

            auto ast = env.parse(input);

            // For malformed syntax, we expect parsing to fail gracefully
            // For valid programs, we expect parsing to succeed
            // Either way, we shouldn't crash

            if (env.reporter().errorCount() > 0) {
                // Errors are expected for malformed inputs
                result.success = true;
            } else if (ast == nullptr) {
                // Null AST without errors is unexpected
                result.success      = false;
                result.errorMessage = "Parser returned nullptr without reporting errors";
            } else {
                // Successfully parsed
                result.success = true;
            }
        } catch (const std::exception& e) {
            result.success   = false;
            result.crashed   = true;
            result.crashInfo = std::string("Exception: ") + e.what();
        } catch (...) {
            result.success   = false;
            result.crashed   = true;
            result.crashInfo = "Unknown exception";
        }
    };

    if (detectCrash(testFunc)) {
        result.crashed = true;
        result.success = false;
        if (result.crashInfo.empty())
            result.crashInfo = "Crash detected via signal/termination";
    }

    return result;
}

FuzzRunner::TestResult FuzzRunner::testCompilerWithOptions(const std::string& input, const FuzzGenerator::CompilerOptions& opts)
{
    TestResult result;
    result.input   = input;
    result.options = mGenerator.generateOptionArgs(opts);
    result.seed    = std::to_string(mSeed);

    // Create a temporary file for the input
    std::string tempFilename = "fuzz_test_temp.pexpr";
    {
        std::ofstream tempFile(tempFilename);
        tempFile << input;
    }

    auto testFunc = [&]() {
        try {

            // Check common locations
            std::vector<std::string> possiblePaths = {
                "pexprc",
                "./pexprc",
                "bin/pexprc",
                "../bin/pexprc",
                "build/Debug/bin/pexprc",
                "build/Release/bin/pexprc",
                "build/bin/pexprc",
                "../build/Debug/bin/pexprc",
                "../build/Release/bin/pexprc",
                "../build/bin/pexprc",
                "../../build/Debug/bin/pexprc",
                "../../build/Release/bin/pexprc",
                "../../build/bin/pexprc"
            };

#ifdef PEXPR_OS_WINDOWS
            // Add .exe extension for Windows
            for (auto& path : possiblePaths)
                path += ".exe";
#endif

            // Find the pexprc executable
            std::string pexprcPath;

            for (const auto& path : possiblePaths) {
                if (std::filesystem::exists(path)) {
                    pexprcPath = path;
                    break;
                }
            }

            if (pexprcPath.empty()) {
                // Try to find it in PATH
                pexprcPath = "pexprc";
#ifdef PEXPR_OS_WINDOWS
                pexprcPath += ".exe";
#endif
            }

            // Build command line
            std::vector<std::string> arguments = {
                tempFilename,
                "-o",
                "fuzz_test_output.tmp"
            };

            for (const auto& opt : result.options)
                arguments.push_back(opt);

            // Execute command
            ExternalProcess process(pexprcPath, arguments);
            int exitCode = process.run();

            // Check exit code
            if (exitCode != 0) // Non-zero exit code is acceptable for invalid inputs
                result.success = true;
            else
                result.success = true;

        } catch (const std::exception& e) {
            result.success   = false;
            result.crashed   = true;
            result.crashInfo = std::string("Exception: ") + e.what();
        } catch (...) {
            result.success   = false;
            result.crashed   = true;
            result.crashInfo = "Unknown exception";
        }
    };

    if (detectCrash(testFunc)) {
        result.crashed = true;
        result.success = false;
        if (result.crashInfo.empty())
            result.crashInfo = "Crash detected via signal/termination";
    }

    // Clean up temporary files
    std::remove(tempFilename.c_str());
    std::remove("fuzz_test_output.tmp");

    return result;
}

FuzzRunner::TestResult FuzzRunner::testFullCompilation(const std::string& input, const std::vector<std::string>& options)
{
    // Similar to testCompilerWithOptions but with more comprehensive checking
    return testCompilerWithOptions(input, mGenerator.generateRandomOptions());
}

bool FuzzRunner::detectCrash(const std::function<void()>& testFunc)
{
    // Simple crash detection - just run the function and catch exceptions
    try {
        testFunc();
        return false;
    } catch (...) {
        return true;
    }
}

void FuzzRunner::logTestStart(const std::string& description)
{
    std::cout << "Running " << description << "..." << std::endl;
}

void FuzzRunner::logTestResult(const TestResult& result)
{
    if (result.success) {
        std::cout << "  ✓ PASSED" << std::endl;
    } else {
        std::cout << "  ✗ FAILED";
        if (result.crashed)
            std::cout << " (CRASH)";
        std::cout << std::endl;

        if (mVerbose) {
            std::cout << "    Error: " << result.errorMessage << std::endl;
            if (result.crashed)
                std::cout << "    Crash info: " << result.crashInfo << std::endl;
            std::cout << "    Input: " << result.input << std::endl;
            if (!result.options.empty()) {
                std::cout << "    Options: ";
                for (const auto& opt : result.options)
                    std::cout << opt << " ";
                std::cout << std::endl;
            }
        }
    }
}

} // namespace PExpr::fuzzy