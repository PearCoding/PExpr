#include <cmath>
#include <fstream>
#include <iostream>

#include "PExpr.h"

using namespace PExpr;

int main(int argc, char** argv)
{
    std::stringstream sourceFiles;
    for (int i = 1; i < argc; ++i) {
        std::ifstream f(argv[i]);
        sourceFiles << f.rdbuf();
    }

    Environment env;
    auto ast = env.parse(sourceFiles);

    if (ast == nullptr)
        return EXIT_FAILURE;

    ssa::SSAMapper mapper;
    auto program = mapper.map(ast);

    std::cout << program.dump() << std::endl;

    if (!ssa::SSAValidator::checkIfTyped(&program)) {
        std::cerr << "Computed SSA is invalid due to unspecified typing!" << std::endl;
        return EXIT_FAILURE;
    }

    ssa::SSAPassSSCP sscp;
    sscp.run(program);

    std::cout << "-------------------------------" << std::endl;
    std::cout << program.dump() << std::endl;

    if (!ssa::SSAValidator::checkIfTyped(&program)) {
        std::cerr << "Computed SSA is invalid due to unspecified typing!" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}