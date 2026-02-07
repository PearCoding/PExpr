#pragma once

#include "Environment.h"
#include "FuzzGenerator.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace PExpr::fuzzy {

class FuzzRunner {
public:
    struct TestResult {
        bool success = true;
        std::string errorMessage;
        std::string input;
        std::vector<std::string> options;
        std::string seed;

        // For crashes
        bool crashed = false;
        std::string crashInfo;
    };

    FuzzRunner();

    // Configuration
    void setMaxIterations(size_t iterations) { mMaxIterations = iterations; }
    void setSeed(uint64_t seed) { mSeed = seed; }
    void setVerbose(bool verbose) { mVerbose = verbose; }

    // Run various fuzzing tests
    TestResult runParserFuzzing();
    TestResult runCompilerOptionFuzzing();
    TestResult runUserMistakeFuzzing();
    TestResult runIntegrationFuzzing();

    // Comprehensive test suite
    std::vector<TestResult> runAllTests();

    // Public test methods for specific testing
    TestResult testParserInput(const std::string& input, const std::string& description);
    TestResult testCompilerWithOptions(const std::string& input, const FuzzGenerator::CompilerOptions& opts);
    TestResult testFullCompilation(const std::string& input, const std::vector<std::string>& options);

private:
    // Internal helper methods

    // Crash detection
    bool detectCrash(const std::function<void()>& testFunc);

    // Reporting
    void logTestStart(const std::string& description);
    void logTestResult(const TestResult& result);

    size_t mMaxIterations = 1000;
    uint64_t mSeed        = 123456789;
    bool mVerbose         = false;
    FuzzGenerator mGenerator;

    // Statistics
    size_t mTestsRun        = 0;
    size_t mTestsPassed     = 0;
    size_t mTestsFailed     = 0;
    size_t mCrashesDetected = 0;
};

} // namespace PExpr::fuzzy