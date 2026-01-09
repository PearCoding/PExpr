#include "Environment.h"
#include "internal/Mangler.h"
#include "internal/Parser.h"
#include "internal/SymbolTable.h"
#include "internal/TypeChecker.h"
#include "internal/UpliftPass.h"

namespace PExpr {
Environment::Environment()
    : mGlobals()
    , mReporter()
{
}

Environment::~Environment()
{
}

void Environment::registerVariable(const std::string& name, ElementaryType type)
{
    mGlobals.addVariable(VariableDef(name, type, false));
}

void Environment::registerFunction(const std::string& name, const std::vector<ElementaryType>& parameterTypes, ElementaryType returnType)
{
    std::vector<std::string> parameterNames;
    parameterNames.reserve(parameterTypes.size());
    for (size_t i = 0; i < parameterTypes.size(); ++i)
        parameterNames.push_back("p" + std::to_string(i));

    const std::string mangledName = internal::makeMangledNameFromTypes(name, parameterTypes, nullptr);
    mGlobals.addFunction(FunctionDef(name, mangledName, parameterTypes, parameterNames, returnType, true));
}

Ptr<Closure> Environment::parse(std::istream& stream)
{
    internal::Lexer lexer(stream, mReporter);
    internal::Parser parser(lexer, mReporter);

    auto expr = parser.parse(&mGlobals);

    if (!expr || parser.hasError())
        return nullptr;

    if (!doTypeChecking(expr))
        return nullptr;

    // run uplift pass to transform captured variables into parameters
    internal::UpliftPass uplift(mGlobals, mReporter);
    uplift.handle(expr);

    return expr;
}

Ptr<Closure> Environment::parse(std::string_view str)
{
    std::istringstream stream(str.data());
    return parse(stream);
}

bool Environment::doTypeChecking(const Ptr<Closure>& closure)
{
    internal::TypeChecker checker(mGlobals, mReporter);
    auto retType = checker.handle(closure);
    if (retType == ElementaryType::Unspecified)
        return false;

    closure->expression()->setReturnType(retType);
    return true;
}
} // namespace PExpr
