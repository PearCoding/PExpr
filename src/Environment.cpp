#include "Environment.h"
#include "internal/DefContainer.h"
#include "internal/Parser.h"
#include "internal/TypeChecker.h"

namespace PExpr {
Environment::Environment()
    : mDefinitions()
{
}

Environment::~Environment()
{
}

void Environment::registerVariableLookupFunction(const VariableLookupFunction& cb)
{
    mDefinitions.addVariableLookupFunction(cb);
}

void Environment::registerFunctionLookupFunction(const FunctionLookupFunction& cb)
{
    mDefinitions.addFunctionLookupFunction(cb);
}

Ptr<Closure> Environment::parse(std::istream& stream, bool skipTypeChecking) const
{
    internal::Lexer lexer(stream);
    internal::Parser parser(lexer);

    auto expr = parser.parse();

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
    internal::TypeChecker checker(mDefinitions);
    auto retType = checker.handle(closure);
    if (retType == ElementaryType::Unspecified)
        return false;

    closure->expression()->setReturnType(retType);
    return true;
}
} // namespace PExpr
