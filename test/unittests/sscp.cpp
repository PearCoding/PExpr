#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "Environment.h"
#include "ssa/SSAMapper.h"
#include "ssa/SSAPassSSCP.h"

using namespace PExpr;
using namespace PExpr::ssa;
using namespace PExpr::internal;

TEST_CASE("SSAPassSSCP: constant folding of binary ops", "[sscp]")
{
    std::stringstream stream("let mut a = 2; let mut b = 3; let mut c = a + b; c");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    // run SSCP pass
    SSAPassSSCP pass;
    pass.run(prog);

    auto dumped = prog.dump();

    // Expect the constant value "5" present
    REQUIRE(dumped.find("5") != std::string::npos);
}

TEST_CASE("SSAPassSSCP: dead code elimination removes unused assigns", "[sscp]")
{
    std::stringstream stream("let x = 1; let y = 2; x");
    Environment env;
    auto ast = env.parse(stream);

    SSAMapper mapper;
    auto prog = mapper.map(ast);

    // Ensure y assign exists before pass (sanity)
    auto before = prog.dump();
    REQUIRE((before.find("y.") != std::string::npos || before.find("y:") != std::string::npos));

    SSAPassSSCP pass;
    pass.run(prog);

    auto after = prog.dump();

    // After pass, 'y' assignment should be removed (dead)
    REQUIRE(after.find("y.") == std::string::npos);
    // x's return should still be present
    REQUIRE(after.find("return ") != std::string::npos);
}
