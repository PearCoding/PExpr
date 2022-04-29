#include <iostream>

#include "PExpr.h"

#include "StringVisitor.h"
#include "internal/Lexer.h"
#include "internal/Parser.h"

using namespace PExpr;

int main(int argc, char** argv)
{
    std::string input;
    for (int i = 1; i < argc; ++i) {
        input += argv[i];
        input += " ";
    }

    std::stringstream stream(input);
    Lexer lexer(stream);
    Parser parser(lexer);

    auto ast = parser.parse();

    if (ast == nullptr) {
        std::cout << "No input" << std::endl;
        return EXIT_SUCCESS;
    }

    std::cout << StringVisitor::visit(ast) << std::endl;
    return EXIT_SUCCESS;
}