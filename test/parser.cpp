#include "internal/Parser.h"
#include "StringVisitor.h"

using namespace PExpr;
using namespace PExpr::internal;
int main(int, char**)
{
    std::stringstream stream("abc(231*22.231*2.42e-3).xyz*Pi-123*(K.x+sin(22^4, 1-2%2, --1))");
    Lexer lexer(stream);
    Parser parser(lexer);

    auto ast = parser.parse();

    std::cout << StringVisitor::visit(ast) << std::endl;
    return EXIT_SUCCESS;
}