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
        internal::TypeChecker checker(mDefinitions);
        auto retType = checker.handle(expr);
        if (retType == ElementaryType::Unspecified)
            return nullptr;

        expr->setReturnType(retType);
    }

    return expr;
}

} // namespace PExpr
