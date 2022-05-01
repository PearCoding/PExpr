#include "PExpr.h"

using namespace PExpr;
int main(int, char**)
{
    Environment env;
    auto ast1 = env.parse("abc(231*22.231*2.42e-3).xyz", true);
    if (!ast1)
        return EXIT_FAILURE;

    std::string src1 = StringVisitor::visit(ast1);

    auto ast2 = env.parse(src1, true);
    if (!ast2)
        return EXIT_FAILURE;

    std::string src2 = StringVisitor::visit(ast1);

    return src1 == src2 ? EXIT_SUCCESS : EXIT_FAILURE;
}