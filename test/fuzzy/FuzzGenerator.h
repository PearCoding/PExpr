#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace PExpr::fuzzy {

class FuzzGenerator {
public:
    FuzzGenerator(uint64_t seed = std::random_device{}());

    // Set the seed for reproducible tests
    void setSeed(uint64_t seed);

    // Generate random PExpr programs
    std::string generateRandomProgram(size_t maxDepth = 5);
    std::string generateRandomExpression(size_t maxDepth = 3);
    std::string generateRandomStatement(size_t maxDepth = 3);

    // Generate malformed/edge case inputs
    std::string generateMalformedSyntax();
    std::string generateEdgeCaseNumbers();
    std::string generateWeirdWhitespace();
    std::string generateUnicodeEdgeCases();
    std::string generateExtremeLengthInput();

    // Generate specific error patterns
    std::string generateMissingSemicolon();
    std::string generateUnmatchedBrackets();
    std::string generateInvalidTokens();
    std::string generateTypeMismatch();

    // Generate option combinations
    struct CompilerOptions {
        bool emitAST          = false;
        bool emitRVM          = false;
        bool readSSAIR        = false;
        bool skipOptimization = false;
        int optimizationLevel = 0;
        uint32_t warningFlags = 0;
        bool warningAsError   = false;

        // Individual optimization flags
        bool optConstantFolding         = false;
        bool optMathFolding             = false;
        bool optDeadCode                = false;
        bool optInlineFunctions         = false;
        bool optForceInlineFunctions    = false;
        bool optMathIdentities          = false;
        bool optTrigonometricIdentities = false;
        bool optCSE                     = false;
        bool optPRE                     = false;
        bool optDissolveTuples          = false;
    };

    CompilerOptions generateRandomOptions();
    std::vector<std::string> generateOptionArgs(const CompilerOptions& opts);

private:
    std::mt19937 mRNG;

    // Helper methods
    std::string randomIdentifier();
    std::string randomNumber();
    std::string randomStringLiteral();
    std::string randomType();
    std::string randomOperator();

    // Generation components
    std::string generateLetDeclaration(size_t depth);
    std::string generateFnDeclaration(size_t depth);
    std::string generateIfExpression(size_t depth);
    std::string generateBinaryExpression(size_t depth);
    std::string generateCallExpression(size_t depth);
    std::string generateVectorExpression(size_t depth);
    std::string generateTupleExpression(size_t depth);
    std::string generateAttributeList();

    // Probability distributions
    template <typename T>
    T randomChoice(const std::vector<T>& choices);

    bool randomBool(double probability = 0.5);
    int randomInt(int min, int max);
    double randomDouble(double min, double max);

    // Constants
    static const std::vector<std::string> KEYWORDS;
    static const std::vector<std::string> OPERATORS;
    static const std::vector<std::string> TYPES;
    static const std::vector<std::string> BUILTIN_FUNCTIONS;
};

} // namespace PExpr::fuzzy