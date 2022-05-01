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

void Environment::registerDef(const VariableDef& def)
{
    mDefinitions.registerDef(def);
}

void Environment::registerDef(const FunctionDef& def)
{
    mDefinitions.registerDef(def);
}

Ptr<Expression> Environment::parse(std::istream& stream, bool skipTypeChecking) const
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

Ptr<Expression> Environment::parse(const std::string& str, bool skipTypeChecking) const
{
    std::stringstream stream(str);
    return parse(stream, skipTypeChecking);
}

bool Environment::doTypeChecking(const Ptr<Expression>& expr) const
{
    internal::TypeChecker checker(mDefinitions);
    auto retType = checker.handle(expr);
    if (retType == ElementaryType::Unspecified)
        return false;

    expr->setReturnType(retType);
    return true;
}
} // namespace PExpr
