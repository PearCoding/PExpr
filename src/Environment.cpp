#include "Environment.h"
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

void Environment::registerVariableLookupFunction(const VariableLookupFunction& cb)
{
    mGlobals.addVariableLookupFunction(cb);
}

void Environment::registerFunctionLookupFunction(const FunctionLookupFunction& cb)
{
    mGlobals.addFunctionLookupFunction(cb);
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
