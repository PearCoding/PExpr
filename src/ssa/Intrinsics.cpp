#include "SSCPFunctionInliner.h"

namespace PExpr::ssa::intrinsics {
void setupIntrinsics(SSCPFunctionInliner& inliner)
{
    const auto addP1N = [&](const std::string& name, std::function<Number(Number)> callback) {
        inliner.addIntrinsic(FunctionDef(name, name, { Parameter{ "p0", ElementaryType::Number } }, ElementaryType::Number, true, false),
                             [callback](const std::vector<ExtendedValueVariant>& args) -> ExtendedValueVariant { return callback(std::get<Number>(args.at(0))); });
    };

    const auto addP2N = [&](const std::string& name, std::function<Number(Number, Number)> callback) {
        inliner.addIntrinsic(FunctionDef(name, name, { Parameter{ "p0", ElementaryType::Number }, Parameter{ "p1", ElementaryType::Number } }, ElementaryType::Number, true, false),
                             [callback](const std::vector<ExtendedValueVariant>& args) -> ExtendedValueVariant { return callback(std::get<Number>(args.at(0)), std::get<Number>(args.at(1))); });
    };

    addP1N("sin", [](Number a) { return std::sin(a); });
    addP1N("cos", [](Number a) { return std::cos(a); });
    addP1N("tan", [](Number a) { return std::tan(a); });
    addP1N("asin", [](Number a) { return std::asin(a); });
    addP1N("acos", [](Number a) { return std::acos(a); });
    addP1N("atan", [](Number a) { return std::atan(a); });

    addP1N("exp", [](Number a) { return std::exp(a); });
    addP1N("exp2", [](Number a) { return std::exp2(a); });
    addP1N("log", [](Number a) { return std::log(a); });
    addP1N("log2", [](Number a) { return std::log2(a); });

    addP1N("sqrt", [](Number a) { return std::sqrt(a); });
    addP1N("cbrt", [](Number a) { return std::cbrt(a); });

    addP2N("pow", [](Number a, Number b) { return std::pow(a, b); });
    addP2N("atan2", [](Number a, Number b) { return std::atan2(a, b); });
}
} // namespace PExpr::ssa::intrinsics