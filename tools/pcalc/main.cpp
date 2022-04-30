#include <iostream>

#include "PExpr.h"

using namespace PExpr;

int main(int argc, char** argv)
{
    std::string input;
    for (int i = 1; i < argc; ++i) {
        input += argv[i];
        input += " ";
    }

    std::stringstream stream(input);
    Environment env;
    env.registerDef(ConstantDef("Pi", ElementaryType::Number, 3.141592));
    env.registerDef(ConstantDef("Eps", ElementaryType::Number, (Number)FltEps));

    env.registerDef(FunctionDef("sin", ElementaryType::Number, { ElementaryType::Number }));
    env.registerDef(FunctionDef("cos", ElementaryType::Number, { ElementaryType::Number }));

    auto ast = env.parse(stream);

    if (ast == nullptr)
        return EXIT_FAILURE;

    std::cout << StringVisitor::visit(ast) << std::endl;
    return EXIT_SUCCESS;
}