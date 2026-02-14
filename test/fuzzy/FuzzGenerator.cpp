#include "FuzzGenerator.h"
#include "utils/Reporter.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace PExpr::fuzzy {

// Static constants
const std::vector<std::string> FuzzGenerator::KEYWORDS = {
    "let", "mut", "fn", "if", "elif", "else", "using", "extern", "pure", "true", "false"
};

const std::vector<std::string> FuzzGenerator::OPERATORS = {
    "+", "-", "*", "/", "%", "^", "==", "!=", "<", ">", "<=", ">=", "&&", "||"
};

const std::vector<std::string> FuzzGenerator::TYPES = {
    "int", "num", "bool", "vec2", "vec3", "vec4", "string"
};

const std::vector<std::string> FuzzGenerator::BUILTIN_FUNCTIONS = {
    "sin", "cos", "tan", "sqrt", "abs", "pow", "min", "max", "length", "normalize"
};

FuzzGenerator::FuzzGenerator(uint64_t seed)
    : mRNG(seed)
{
}

void FuzzGenerator::setSeed(uint64_t seed)
{
    mRNG.seed(seed);
}

std::string FuzzGenerator::generateRandomProgram(size_t maxDepth)
{
    std::stringstream ss;
    int numStatements = randomInt(1, 10);

    // Add some type aliases
    if (randomBool(0.3))
        ss << "using " << randomIdentifier() << " = " << randomType() << ";\n";

    // Add some external functions
    if (randomBool(0.2)) {
        if (randomBool(0.6))
            ss << "[[extern]]";
        else
            ss << "[[extern, pure]]";

        ss << " fn " << randomIdentifier() << "(x:" << randomType() << ") -> " << randomType() << ";\n";
    }

    // Generate statements
    for (int i = 0; i < numStatements; ++i) {
        if (randomBool(0.7))
            ss << generateLetDeclaration(maxDepth - 1) << ";\n";
        else if (randomBool(0.5))
            ss << generateFnDeclaration(maxDepth - 1) << ";\n";
        else
            ss << generateRandomExpression(maxDepth - 1) << ";\n";
    }

    // Final expression
    ss << generateRandomExpression(std::min<size_t>(maxDepth, 3));

    return ss.str();
}

std::string FuzzGenerator::generateRandomExpression(size_t maxDepth)
{
    if (maxDepth == 0) {
        // Base case: literals or identifiers
        switch (randomInt(0, 3)) {
        case 0:
            return randomNumber();
        case 1:
            return randomBool() ? "true" : "false";
        case 2:
            return randomStringLiteral();
        case 3:
            return randomIdentifier();
        default:
            return "0";
        }
    }

    switch (randomInt(0, 7)) {
    case 0:
        return generateBinaryExpression(maxDepth - 1);
    case 1:
        return generateCallExpression(maxDepth - 1);
    case 2:
        return generateIfExpression(maxDepth - 1);
    case 3:
        return "(" + generateRandomExpression(maxDepth - 1) + ")";
    case 4:
        return randomIdentifier() + "." + randomChoice<std::string>({ "x", "y", "z", "w", "xy", "xyz", "yx" });
    case 5:
        return generateVectorExpression(maxDepth - 1);
    case 6:
        return generateTupleExpression(maxDepth - 1);
    case 7:
        return randomNumber();
    default:
        return "0";
    }
}

std::string FuzzGenerator::generateRandomStatement(size_t maxDepth)
{
    if (randomBool(0.6))
        return generateLetDeclaration(maxDepth) + ";";
    else if (randomBool(0.7))
        return generateFnDeclaration(maxDepth) + ";";
    else
        return generateRandomExpression(maxDepth) + ";";
}

std::string FuzzGenerator::generateMalformedSyntax()
{
    switch (randomInt(0, 9)) {
    case 0:
        return "let x = ;"; // Missing expression
    case 1:
        return "fn f() = ;"; // Missing body
    case 2:
        return "if true {"; // Missing closing brace
    case 3:
        return "let x = 1 + ;"; // Binary op with missing right operand
    case 4:
        return "(1 + 2"; // Unclosed parentheses
    case 5:
        return "let x = 1 2 3;"; // Missing operators
    case 6:
        return "[[extern fn f() -> int;"; // Missing bracket
    case 7:
        return "let mut x = 1 let y = 2;"; // Missing semicolon
    case 8:
        return "fn f(x:int -> int = x;"; // Missing closing paren
    case 9:
        return "[x, y] = [1, 2"; // Missing closing bracket
    default:
        return "";
    }
}

std::string FuzzGenerator::generateEdgeCaseNumbers()
{
    switch (randomInt(0, 8)) {
    case 0:
        return "1e308"; // Near max double
    case 1:
        return "1e-308"; // Near min double
    case 2:
        return "0.0";
    case 3:
        return "-0.0";
    case 4:
        return "inf";
    case 5:
        return "-inf";
    case 6:
        return "nan";
    case 7:
        return "1.7976931348623157e+308"; // Max double
    case 8:
        return "2.2250738585072014e-308"; // Min positive double
    default:
        return "0";
    }
}

std::string FuzzGenerator::generateWeirdWhitespace()
{
    std::string input = "let";

    // Insert various whitespace characters
    std::vector<std::string> whitespace = { " ", "  ", "\t", "\n", "\r", "\v", "\f" };

    for (int i = 0; i < randomInt(3, 10); ++i) {
        input += randomChoice(whitespace);
        if (i % 3 == 0)
            input += "x";
        else if (i % 3 == 1)
            input += "=";
        else
            input += "1";
    }

    input += ";";
    return input;
}

std::string FuzzGenerator::generateUnicodeEdgeCases()
{
    // Various Unicode edge cases
    switch (randomInt(0, 5)) {
    case 0:
        return "let π = 3.14159; π"; // Greek letter
    case 1:
        return "let 变量 = 42; 变量"; // Chinese characters
    case 2:
        return "let 🚀 = 100; 🚀"; // Emoji
    case 3:
        return "let a\u0000b = 1;"; // Null byte
    case 4:
        return "let a\x01b = 2;"; // Control character
    case 5:
        return "\"hello\\u0000world\""; // String with null
    default:
        return "let x = 1; x";
    }
}

std::string FuzzGenerator::generateExtremeLengthInput()
{
    if (randomBool(0.5)) {
        // Very long identifier
        std::stringstream ss;
        ss << "let ";
        for (int i = 0; i < 10000; ++i)
            ss << "x";
        ss << " = 1;";
        return ss.str();
    } else {
        // Very long number
        std::stringstream ss;
        ss << "let x = 0.";
        for (int i = 0; i < 1000; ++i)
            ss << randomInt(0, 9);
        ss << ";";
        return ss.str();
    }
}

std::string FuzzGenerator::generateMissingSemicolon()
{
    std::stringstream ss;
    ss << "let x = 1\n"; // Missing semicolon
    ss << "let y = 2;\n";
    ss << "x + y";
    return ss.str();
}

std::string FuzzGenerator::generateUnmatchedBrackets()
{
    switch (randomInt(0, 3)) {
    case 0:
        return "let x = (1 + 2;"; // Missing closing paren
    case 1:
        return "let x = [1, 2, 3;"; // Missing closing bracket
    case 2:
        return "if true { let x = 1;"; // Missing closing brace
    case 3:
        return "[[extern fn f() -> int;"; // Missing closing bracket
    default:
        return "";
    }
}

std::string FuzzGenerator::generateInvalidTokens()
{
    std::vector<std::string> invalidTokens = { "@", "#", "$", "`", "~", "\\", "|" };
    std::string token                      = randomChoice(invalidTokens);
    return "let x = 1 " + token + " 2;";
}

std::string FuzzGenerator::generateTypeMismatch()
{
    switch (randomInt(0, 4)) {
    case 0:
        return "let x:vec2 = 1;"; // Scalar assigned to vector
    case 1:
        return "let x:int = 3.14;"; // Float assigned to int
    case 2:
        return "let x:bool = 42;"; // Int assigned to bool
    case 3:
        return "fn f() -> int = 3.14;"; // Float returned as int
    case 4:
        return "let x:[int, num] = [1, true];"; // Bool in tuple
    default:
        return "";
    }
}

FuzzGenerator::CompilerOptions FuzzGenerator::generateRandomOptions()
{
    CompilerOptions opts;

    opts.emitAST           = randomBool(0.1);
    opts.emitRVM           = randomBool(0.1);
    opts.readSSAIR         = randomBool(0.05);
    opts.skipOptimization  = randomBool(0.1);
    opts.optimizationLevel = randomInt(0, 3);
    opts.warningAsError    = randomBool(0.1);

    // Generate random warning flags
    if (randomBool(0.7))
        opts.warningFlags = 0;
    else if (randomBool(0.5))
        opts.warningFlags = utils::RT_WARNING_DEFAULT;
    else
        opts.warningFlags = randomInt(0, (int)utils::RT_WARNING_ALL);

    // Set optimization flags based on level or randomly
    if (opts.optimizationLevel > 0 || randomBool(0.3)) {
        opts.optConstantFolding         = randomBool(0.8);
        opts.optMathFolding             = randomBool(0.8);
        opts.optDeadCode                = randomBool(0.7);
        opts.optInlineFunctions         = randomBool(0.6);
        opts.optForceInlineFunctions    = randomBool(0.1);
        opts.optMathIdentities          = randomBool(0.5);
        opts.optTrigonometricIdentities = randomBool(0.3);
        opts.optCSE                     = randomBool(0.7);
        opts.optPRE                     = randomBool(0.4);
        opts.optDissolveTuples          = randomBool(0.5);
    }

    return opts;
}

std::vector<std::string> FuzzGenerator::generateOptionArgs(const CompilerOptions& opts)
{
    std::vector<std::string> args;

    if (opts.emitAST)
        args.push_back("--emit-ast");
    if (opts.emitRVM)
        args.push_back("--emit-rvm");
    if (opts.readSSAIR)
        args.push_back("--input-ir");
    if (opts.skipOptimization)
        args.push_back("--skip-optimization");

    // Optimization level
    if (opts.optimizationLevel > 0) {
        args.push_back("-O");
        args.push_back(std::to_string(opts.optimizationLevel));
    }

    // Warning flags
    if (opts.warningFlags != (uint32_t)utils::RT_WARNING_DEFAULT) {
        if (opts.warningFlags == 0) {
            args.push_back("--no-warnings");
        } else if (opts.warningFlags == (uint32_t)utils::RT_WARNING_ALL) {
            args.push_back("-W");
            args.push_back("all");
        } else {
            // Add individual warning flags
            if (opts.warningFlags & utils::RT_WARNING_TRAILING_SEMICOLON) {
                args.push_back("-W");
                args.push_back("trailing-semicolon");
            }
            if (opts.warningFlags & utils::RT_WARNING_IMPLICIT_CAST) {
                args.push_back("-W");
                args.push_back("implicit-cast");
            }
            // Add more warning flags as needed
        }
    }

    if (opts.warningAsError) {
        args.push_back("-W");
        args.push_back("error");
    }

    // Individual optimization flags (if not using -O)
    if (opts.optimizationLevel == 0) {
        if (opts.optConstantFolding)
            args.push_back("--opt-constant-folding");
        if (opts.optMathFolding)
            args.push_back("--opt-math-folding");
        if (opts.optDeadCode)
            args.push_back("--opt-dead-code");
        if (opts.optInlineFunctions)
            args.push_back("--opt-inline-functions");
        if (opts.optForceInlineFunctions)
            args.push_back("--opt-force-inline-functions");
        if (opts.optMathIdentities)
            args.push_back("--opt-math-identities");
        if (opts.optTrigonometricIdentities)
            args.push_back("--opt-trigonometric-identities");
        if (opts.optCSE)
            args.push_back("--opt-cse");
        if (opts.optPRE)
            args.push_back("--opt-pre");
        if (opts.optDissolveTuples)
            args.push_back("--opt-dissolve-tuples");
    }

    return args;
}

std::string FuzzGenerator::randomIdentifier()
{
    static const std::vector<std::string> letters = {
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"
    };
    static const std::vector<std::string> digits = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
    };

    std::string id;
    int length = randomInt(1, 20);

    // First character must be letter
    id += randomChoice(letters);

    // Remaining characters can be letters or digits
    for (int i = 1; i < length; ++i) {
        if (randomBool(0.8))
            id += randomChoice(letters);
        else
            id += randomChoice(digits);
    }

    return id;
}

std::string FuzzGenerator::randomNumber()
{
    switch (randomInt(0, 4)) {
    case 0:
        return std::to_string(randomInt(-1000, 1000));
    case 1: {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(randomInt(0, 10))
           << randomDouble(-1000.0, 1000.0);
        return ss.str();
    }
    case 2: {
        std::stringstream ss;
        double val = randomDouble(0.0, 1000.0);
        int exp    = randomInt(-10, 10);
        ss << val << "e" << (exp >= 0 ? "+" : "") << exp;
        return ss.str();
    }
    case 3:
        return randomBool() ? "0" : "1";
    case 4:
        return generateEdgeCaseNumbers();
    default:
        return "0";
    }
}

std::string FuzzGenerator::randomStringLiteral()
{
    std::string str = "\"";
    int length      = randomInt(0, 50);

    for (int i = 0; i < length; ++i) {
        char c = static_cast<char>(randomInt(32, 126)); // Printable ASCII
        if (c == '"' || c == '\\')
            str += '\\';
        str += c;
    }

    str += "\"";
    return str;
}

std::string FuzzGenerator::randomType()
{
    return randomChoice(TYPES);
}

std::string FuzzGenerator::randomOperator()
{
    return randomChoice(OPERATORS);
}

std::string FuzzGenerator::generateLetDeclaration(size_t depth)
{
    std::stringstream ss;
    ss << "let ";

    if (randomBool(0.3))
        ss << "mut ";

    ss << randomIdentifier();

    if (randomBool(0.4))
        ss << ":" << randomType();

    ss << " = " << generateRandomExpression(depth);

    return ss.str();
}

std::string FuzzGenerator::generateFnDeclaration(size_t depth)
{
    std::stringstream ss;

    if (randomBool(0.2))
        ss << generateAttributeList() << " ";

    ss << "fn " << randomIdentifier() << "(";

    int numParams = randomInt(0, 3);
    for (int i = 0; i < numParams; ++i) {
        if (i > 0)
            ss << ", ";
        ss << randomIdentifier() << ":" << randomType();
    }

    ss << ")";

    if (randomBool(0.7))
        ss << " -> " << randomType();

    ss << " = " << generateRandomExpression(depth);

    return ss.str();
}

std::string FuzzGenerator::generateIfExpression(size_t depth)
{
    std::stringstream ss;
    ss << "if " << generateRandomExpression(depth) << " { "
       << generateRandomExpression(depth) << " }";

    if (randomBool(0.5))
        ss << " else { " << generateRandomExpression(depth) << " }";

    return ss.str();
}

std::string FuzzGenerator::generateBinaryExpression(size_t depth)
{
    std::string left  = generateRandomExpression(depth);
    std::string op    = randomOperator();
    std::string right = generateRandomExpression(depth);

    return left + " " + op + " " + right;
}

std::string FuzzGenerator::generateCallExpression(size_t depth)
{
    std::stringstream ss;

    if (randomBool(0.3))
        ss << randomChoice(BUILTIN_FUNCTIONS);
    else
        ss << randomIdentifier();

    ss << "(";

    int numArgs = randomInt(0, 3);
    for (int i = 0; i < numArgs; ++i) {
        if (i > 0)
            ss << ", ";
        ss << generateRandomExpression(depth);
    }

    ss << ")";

    return ss.str();
}

std::string FuzzGenerator::generateVectorExpression(size_t depth)
{
    std::stringstream ss;
    ss << "[";

    int size = randomChoice<int>(std::vector<int>{ 2, 3, 4 });
    for (int i = 0; i < size; ++i) {
        if (i > 0)
            ss << ", ";
        ss << generateRandomExpression(depth);
    }

    ss << "]";
    return ss.str();
}

std::string FuzzGenerator::generateTupleExpression(size_t depth)
{
    std::stringstream ss;
    ss << "[";

    int size = randomInt(2, 4);
    for (int i = 0; i < size; ++i) {
        if (i > 0)
            ss << ", ";
        ss << generateRandomExpression(depth);
    }

    ss << "]";
    return ss.str();
}

std::string FuzzGenerator::generateAttributeList()
{
    std::stringstream ss;
    ss << "[[";

    int numAttrs                   = randomInt(1, 3);
    std::vector<std::string> attrs = { "extern", "pure" };

    for (int i = 0; i < numAttrs; ++i) {
        if (i > 0)
            ss << ", ";
        ss << randomChoice(attrs);

        if (randomBool(0.3)) {
            ss << "=";
            if (randomBool())
                ss << (randomBool() ? "true" : "false");
            else
                ss << "\"" << randomIdentifier() << "\"";
        }
    }

    ss << "]]";
    return ss.str();
}

template <typename T>
T FuzzGenerator::randomChoice(const std::vector<T>& choices)
{
    if (choices.empty())
        return T();
    std::uniform_int_distribution<size_t> dist(0, choices.size() - 1);
    return choices[dist(mRNG)];
}

bool FuzzGenerator::randomBool(double probability)
{
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(mRNG) < probability;
}

int FuzzGenerator::randomInt(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(mRNG);
}

double FuzzGenerator::randomDouble(double min, double max)
{
    std::uniform_real_distribution<double> dist(min, max);
    return dist(mRNG);
}

// Explicit template instantiations
template std::string FuzzGenerator::randomChoice<std::string>(const std::vector<std::string>&);
template int FuzzGenerator::randomChoice<int>(const std::vector<int>&);

} // namespace PExpr::fuzzy