#include "Environment.h"
#include "ast/Expression.h"
#include "parser/Parser.h"
#include "type/Mangler.h"
#include "type/Parameter.h"
#include "type/SymbolTable.h"
#include "type/TypeChecker.h"
#include "type/UpliftPass.h"

namespace PExpr {
using namespace ast;
using namespace type;

Environment::Environment()
    : mGlobals()
    , mReporter()
{
    mGlobals.addDefaultTypeAliases();
}

Environment::~Environment()
{
}

void Environment::registerVariable(const std::string& name, const Type& type)
{
    mGlobals.addVariable(VariableDef(name, type, false));
}

void Environment::registerFunction(const std::string& name, const std::vector<Type>& parameterTypes, const Type& returnType, bool hasSideEffect)
{
    // Build a ParameterList using default parameter names p0, p1, ...
    ParameterList params;
    params.reserve(parameterTypes.size());
    for (size_t i = 0; i < parameterTypes.size(); ++i)
        params.push_back(Parameter{ "p" + std::to_string(i), parameterTypes[i] });

    const std::string mangledName = makeMangledNameFromTypes(name, parameterTypes, nullptr);
    mGlobals.addFunction(FunctionDef(name, mangledName, std::move(params), returnType, true, hasSideEffect));
}

Ptr<Closure> Environment::parse(std::istream& stream)
{
    parser::Lexer lexer(stream, mReporter);
    parser::Parser parser(lexer, mReporter);

    auto expr = parser.parse(&mGlobals);

    if (!expr || parser.hasError())
        return nullptr;

    if (!doTypeChecking(expr))
        return nullptr;

    if (mReporter.errorCount() > 0)
        return nullptr;

    // run uplift pass to transform captured variables into parameters
    UpliftPass uplift(mReporter);
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
    TypeChecker checker(mReporter);
    const auto retType = checker.handle(closure);
    if (retType.kind() == TypeKind::Unspecified || retType.kind() == TypeKind::Error)
        return false;
    return true;
}
} // namespace PExpr
