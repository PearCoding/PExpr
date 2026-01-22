#include "SSCPFunctionInliner.h"

namespace PExpr::ssa::intrinsics {
static Number sin_intrinsic(Number a) { return std::sin(a); }
static Number cos_intrinsic(Number a) { return std::cos(a); }
static Number tan_intrinsic(Number a) { return std::tan(a); }
static Number asin_intrinsic(Number a) { return std::asin(a); }
static Number acos_intrinsic(Number a) { return std::acos(a); }
static Number atan_intrinsic(Number a) { return std::atan(a); }

void setupIntrinsics(SSCPFunctionInliner& inliner)
{
    auto addP1N = [&](const std::string& name, std::function<Number(Number)> callback) {
        inliner.addIntrinsic(FunctionDef(name, name, { Parameter{ "v", ElementaryType::Number } }, ElementaryType::Number, true, false),
                             [callback](const std::vector<ExtendedValueVariant>& args) -> ExtendedValueVariant { return callback(std::get<Number>(args.at(0))); });
    };

    addP1N("sin", sin_intrinsic);
    addP1N("cos", cos_intrinsic);
    addP1N("tan", tan_intrinsic);
    addP1N("asin", asin_intrinsic);
    addP1N("acos", acos_intrinsic);
    addP1N("atan", atan_intrinsic);
}
} // namespace PExpr::ssa::intrinsics