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
    auto ast = env.parse(stream);

    if (ast == nullptr)
        return EXIT_FAILURE;

    std::cout << StringVisitor::visit(ast) << std::endl;
    return EXIT_SUCCESS;
}