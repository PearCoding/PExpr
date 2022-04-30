#include "Environment.h"
#include "internal/Parser.h"
#include "internal/TypeChecker.h"
#include "internal/VariableContainer.h"

namespace PExpr {
class EnvironmentPrivate {
public:
    PExpr::VariableContainer VariableContainer;
};

Environment::Environment()
    : mInternal(std::make_unique<EnvironmentPrivate>())
{
}

Environment::~Environment()
{
}

void Environment::registerVariable(const std::string& name, ElementaryType type)
{
    mInternal->VariableContainer.registerVariable(name, type);
}

Ptr<Expression> Environment::parse(std::istream& stream) const
{
    Lexer lexer(stream);
    Parser parser(lexer);

    auto expr = parser.parse();

    if (!expr || parser.hasError())
        return nullptr;

    TypeChecker checker(mInternal->VariableContainer);
    auto retType = checker.handle(expr);
    if (retType == ElementaryType::Unspecified)
        return nullptr;

    expr->setReturnType(retType);
    return expr;
}

} // namespace PExpr
