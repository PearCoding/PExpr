#include <catch2/catch_test_macros.hpp>
#include "FuzzRunner.h"
#include <iostream>
#include <string>

using namespace PExpr::fuzzy;

TEST_CASE("Fuzzy testing: Parser input validation", "[fuzzy][parser]")
{
    FuzzRunner runner;
    runner.setMaxIterations(100);  // Reduced for faster testing
    runner.setSeed(12345);
    runner.setVerbose(true);
    
    auto result = runner.runParserFuzzing();
    
    // The test passes if we didn't crash
    REQUIRE_FALSE(result.crashed);
    
    // We expect some failures due to malformed syntax, but no crashes
    if (result.crashed) {
        std::cerr << "CRASH DETECTED in parser fuzzing!" << std::endl;
        std::cerr << "Input that caused crash: " << result.input << std::endl;
        std::cerr << "Crash info: " << result.crashInfo << std::endl;
    }
}

TEST_CASE("Fuzzy testing: Compiler option combinations", "[fuzzy][compiler][options]")
{
    FuzzRunner runner;
    runner.setMaxIterations(50);  // Reduced for faster testing
    runner.setSeed(54321);
    runner.setVerbose(true);
    
    auto result = runner.runCompilerOptionFuzzing();
    
    // The test passes if we didn't crash
    REQUIRE_FALSE(result.crashed);
    
    if (result.crashed) {
        std::cerr << "CRASH DETECTED in compiler option fuzzing!" << std::endl;
        std::cerr << "Input that caused crash: " << result.input << std::endl;
        std::cerr << "Options: ";
        for (const auto& opt : result.options) {
            std::cerr << opt << " ";
        }
        std::cerr << std::endl;
        std::cerr << "Crash info: " << result.crashInfo << std::endl;
    }
}

TEST_CASE("Fuzzy testing: Common user mistakes", "[fuzzy][user][mistakes]")
{
    FuzzRunner runner;
    runner.setMaxIterations(20);  // Reduced for faster testing
    runner.setSeed(98765);
    runner.setVerbose(true);
    
    auto result = runner.runUserMistakeFuzzing();
    
    // The test passes if we didn't crash
    REQUIRE_FALSE(result.crashed);
    
    if (result.crashed) {
        std::cerr << "CRASH DETECTED in user mistake fuzzing!" << std::endl;
        std::cerr << "Input that caused crash: " << result.input << std::endl;
        std::cerr << "Crash info: " << result.crashInfo << std::endl;
    }
}

TEST_CASE("Fuzzy testing: Integration and full compilation", "[fuzzy][integration]")
{
    FuzzRunner runner;
    runner.setMaxIterations(30);  // Reduced for faster testing
    runner.setSeed(13579);
    runner.setVerbose(true);
    
    auto result = runner.runIntegrationFuzzing();
    
    // The test passes if we didn't crash
    REQUIRE_FALSE(result.crashed);
    
    if (result.crashed) {
        std::cerr << "CRASH DETECTED in integration fuzzing!" << std::endl;
        std::cerr << "Input that caused crash: " << result.input << std::endl;
        std::cerr << "Options: ";
        for (const auto& opt : result.options) {
            std::cerr << opt << " ";
        }
        std::cerr << std::endl;
        std::cerr << "Crash info: " << result.crashInfo << std::endl;
    }
}

TEST_CASE("Fuzzy testing: Comprehensive test suite", "[fuzzy][comprehensive]")
{
    SECTION("Run all fuzzy tests with different seeds")
    {
        std::vector<uint64_t> seeds = {42, 123456, 999999, 7777777};
        
        for (const auto& seed : seeds) {
            FuzzRunner runner;
            runner.setMaxIterations(50);  // Small number for comprehensive test
            runner.setSeed(seed);
            runner.setVerbose(false);  // Less verbose for batch testing
            
            auto results = runner.runAllTests();
            
            // Check that no crashes occurred
            for (const auto& result : results) {
                REQUIRE_FALSE(result.crashed);
                
                if (result.crashed) {
                    std::cerr << "CRASH DETECTED with seed " << seed << "!" << std::endl;
                    std::cerr << "Input: " << result.input << std::endl;
                    std::cerr << "Crash info: " << result.crashInfo << std::endl;
                }
            }
        }
    }
}

TEST_CASE("Fuzzy testing: Edge case reproduction", "[fuzzy][reproduction]")
{
    SECTION("Reproduce specific edge cases")
    {
        FuzzRunner runner;
        
        // Test specific problematic patterns
        std::vector<std::string> edgeCases = {
            // Empty input
            "",
            // Just whitespace
            "   \t\n  ",
            // Single token
            "1",
            // Unclosed string
            "\"hello",
            // Invalid number
            "1.2.3",
            // Nested brackets
            "[[[extern]]]",
            // Extreme nesting
            "(((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((())))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))",
        };
        
        for (const auto& input : edgeCases) {
            auto result = runner.testParserInput(input, "Edge case: " + input);
            
            // Should not crash
            REQUIRE_FALSE(result.crashed);
            
            if (result.crashed) {
                std::cerr << "CRASH on edge case: " << input << std::endl;
                std::cerr << "Crash info: " << result.crashInfo << std::endl;
            }
        }
    }
    
    SECTION("Test specific option combinations")
    {
        FuzzRunner runner;
        FuzzGenerator generator(12345);
        
        // Generate some specific option combinations to test
        for (int i = 0; i < 10; ++i) {
            auto opts = generator.generateRandomOptions();
            
            // Create contradictory options
            if (i == 0) {
                opts.skipOptimization = true;
                opts.optimizationLevel = 3;  // Contradictory
            } else if (i == 1) {
                opts.emitAST = true;
                opts.emitRVM = true;  // Might be contradictory
            }
            
            // Test with a simple program
            std::string input = "let x = 1; x";
            auto result = runner.testCompilerWithOptions(input, opts);
            
            // Should not crash
            REQUIRE_FALSE(result.crashed);
            
            if (result.crashed) {
                std::cerr << "CRASH on option combination " << i << std::endl;
                std::cerr << "Options: ";
                for (const auto& opt : result.options) {
                    std::cerr << opt << " ";
                }
                std::cerr << std::endl;
                std::cerr << "Crash info: " << result.crashInfo << std::endl;
            }
        }
    }
}