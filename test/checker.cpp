#include <cmath>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string_view>

#include "PExpr.h"

using namespace PExpr;

static inline void rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(),
            s.end());
}

int main(int argc, char** argv)
{
    using namespace std::string_view_literals;

    bool fileinput = false;

    std::string input;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == "-f"sv) {
            fileinput = true;
            input     = "";
        } else {
            input += argv[i];
            input += " ";
        }
    }

    if (fileinput) {
        std::string path = input;
        rtrim(path);

        std::ifstream f(path);
        if (!f.good()) {
            std::cerr << "Could not open '" << path << "'" << std::endl;
            return EXIT_FAILURE;
        }

        f.seekg(0, std::ios::end);
        input.reserve(f.tellg());
        f.seekg(0, std::ios::beg);
        input.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }

    Environment env;
    auto ast = env.parse(input, true);

    if (ast == nullptr)
        return EXIT_FAILURE;

    std::cout << StringVisitor::visit(ast) << std::endl;

    return EXIT_SUCCESS;
}