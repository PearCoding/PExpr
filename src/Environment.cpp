#include "Environment.h"
#include "internal/DefContainer.h"
#include "internal/Parser.h"
#include "internal/TypeChecker.h"

namespace PExpr {
class EnvironmentPrivate {
public:
    DefContainer Definitions;
};

Environment::Environment()
    : mInternal(std::make_unique<EnvironmentPrivate>())
{
}

Environment::~Environment()
{
}

void Environment::registerDef(const VariableDef& def)
{
    mInternal->Definitions.registerDef(def);
}

void Environment::registerDef(const FunctionDef& def)
{
    mInternal->Definitions.registerDef(def);
}

Ptr<Expression> Environment::parse(std::istream& stream, bool skipTypeChecking) const
{
    Lexer lexer(stream);
    Parser parser(lexer);

    auto expr = parser.parse();

    if (!expr || parser.hasError())
        return nullptr;

    if (!skipTypeChecking) {
        TypeChecker checker(mInternal->Definitions);
        auto retType = checker.handle(expr);
        if (retType == ElementaryType::Unspecified)
            return nullptr;

        expr->setReturnType(retType);
    }

    return expr;
}

} // namespace PExpr
