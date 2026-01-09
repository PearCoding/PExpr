#include "Environment.h"
#include "Parameter.h"
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
    // Build a ParameterList using default parameter names p0, p1, ...
    ParameterList params;
    params.reserve(parameterTypes.size());
    for (size_t i = 0; i < parameterTypes.size(); ++i)
        params.push_back(Parameter{ "p" + std::to_string(i), parameterTypes[i] });

    const std::string mangledName = internal::makeMangledNameFromTypes(name, parameterTypes, nullptr);
    mGlobals.addFunction(FunctionDef(name, mangledName, std::move(params), returnType, true));
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

    if (mReporter.errorCount() > 0)
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
