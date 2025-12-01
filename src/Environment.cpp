#include "Environment.h"
#include "internal/Mangler.h"
#include "internal/Parser.h"
#include "internal/SymbolTable.h"
#include "internal/TypeChecker.h"

namespace PExpr {
Environment::Environment()
    : mGlobals()
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
    const std::string mangledName = internal::makeMangledNameFromTypes(name, parameterTypes, nullptr);
    mGlobals.addFunction(FunctionDef(name, mangledName, parameterTypes, returnType, true));
}

Ptr<Closure> Environment::parse(std::istream& stream, bool skipTypeChecking) const
{
    internal::Lexer lexer(stream);
    internal::Parser parser(lexer);

    auto expr = parser.parse(&mGlobals);

    if (!expr || parser.hasError())
        return nullptr;

    if (!skipTypeChecking) {
        if (!doTypeChecking(expr))
            return nullptr;
    }

    return expr;
}

Ptr<Closure> Environment::parse(const std::string& str, bool skipTypeChecking) const
{
    std::stringstream stream(str);
    return parse(stream, skipTypeChecking);
}

bool Environment::doTypeChecking(const Ptr<Closure>& closure) const
{
    internal::TypeChecker checker(mGlobals);
    auto retType = checker.handle(closure);
    if (retType == ElementaryType::Unspecified)
        return false;

    closure->expression()->setReturnType(retType);
    return true;
}
} // namespace PExpr
