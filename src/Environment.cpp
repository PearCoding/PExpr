#include "Environment.h"
#include "ast/Expression.h"
#include "opt/SSAOptimizer.h"
#include "parser/Parser.h"
#include "ssa/SSAMapper.h"
#include "type/Mangler.h"
#include "type/SymbolTable.h"
#include "type/TypeChecker.h"
#include "type/UpliftPass.h"

namespace PExpr {
using namespace ast;
using namespace type;

Environment::Environment()
    : mReporter()
{
}

Environment::~Environment()
{
}

Ptr<Closure> Environment::parse(std::istream& stream, const std::filesystem::path& filename)
{
    parser::Lexer lexer(stream, mReporter, filename);
    parser::Parser parser(lexer, mReporter);

    SymbolTable globals;
    globals.addDefaultTypeAliases();
    auto expr = parser.parse(&globals);

    if (!expr)
        return nullptr;

    // Do type checking to set all the types inside the AST correctly
    if (!doTypeChecking(expr))
        return nullptr;

    if (mReporter.errorCount() > 0)
        return nullptr;

    // run uplift pass to transform captured variables into parameters
    UpliftPass uplift(mReporter);
    uplift.handle(expr);

    return expr;
}

Ptr<Closure> Environment::parse(std::string_view str, const std::filesystem::path& filename)
{
    std::istringstream stream(str.data());
    return parse(stream, filename);
}

bool Environment::doTypeChecking(const Ptr<Closure>& closure)
{
    TypeChecker checker(mReporter);
    const auto retType = checker.handle(closure);
    return retType.isSpecified();
}

ssa::SSAProgram Environment::map(const Ptr<ast::Closure>& closure)
{
    if (!closure)
        return ssa::SSAProgram{};

    ssa::SSAMapper mapper(mReporter);
    return mapper.map(closure);
}

bool Environment::optimize(ssa::SSAProgram& program, const opt::OptimizerOptions& options)
{
    opt::SSAOptimizer::Run(options, program);
    return true;
}
} // namespace PExpr
