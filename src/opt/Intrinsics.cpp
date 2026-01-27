#include "SSCPFunctionInliner.h"
#include "type/Mangler.h"

#include <array>

namespace PExpr::opt::intrinsics {
using namespace type;

void setupIntrinsics(SSCPFunctionInliner& inliner)
{
    const auto addP1N = [&](const std::string& name, std::function<Number(Number)> callback) {
        inliner.addIntrinsic(FunctionDef(name,
                                         makeMangledNameFromTypes(name, std::to_array({ Type(TypeKind::Number) }), nullptr),
                                         { Parameter{ "p0", Type(TypeKind::Number) } }, Type(TypeKind::Number), true, false),
                             [callback](const std::vector<ValueVariant>& args) -> ValueVariant { return callback(std::get<Number>(args.at(0))); });
    };

    const auto addP2N = [&](const std::string& name, std::function<Number(Number, Number)> callback) {
        inliner.addIntrinsic(FunctionDef(name,
                                         makeMangledNameFromTypes(name, std::to_array({ Type(TypeKind::Number), Type(TypeKind::Number) }), nullptr),
                                         { Parameter{ "p0", Type(TypeKind::Number) }, Parameter{ "p1", Type(TypeKind::Number) } }, Type(TypeKind::Number), true, false),
                             [callback](const std::vector<ValueVariant>& args) -> ValueVariant { return callback(std::get<Number>(args.at(0)), std::get<Number>(args.at(1))); });
    };

    addP1N("sin", [](Number a) { return std::sin(a); });   // [[extern, pure]] fn sin(a:num) -> num;
    addP1N("cos", [](Number a) { return std::cos(a); });   // [[extern, pure]] fn cos(a:num) -> num;
    addP1N("tan", [](Number a) { return std::tan(a); });   // [[extern, pure]] fn tan(a:num) -> num;
    addP1N("asin", [](Number a) { return std::asin(a); }); // [[extern, pure]] fn asin(a:num) -> num;
    addP1N("acos", [](Number a) { return std::acos(a); }); // [[extern, pure]] fn acos(a:num) -> num;
    addP1N("atan", [](Number a) { return std::atan(a); }); // [[extern, pure]] fn atan(a:num) -> num;

    addP1N("exp", [](Number a) { return std::exp(a); });   // [[extern, pure]] fn exp(a:num) -> num;
    addP1N("exp2", [](Number a) { return std::exp2(a); }); // [[extern, pure]] fn exp2(a:num) -> num;
    addP1N("log", [](Number a) { return std::log(a); });   // [[extern, pure]] fn log(a:num) -> num;
    addP1N("log2", [](Number a) { return std::log2(a); }); // [[extern, pure]] fn log2(a:num) -> num;

    addP1N("sqrt", [](Number a) { return std::sqrt(a); }); // [[extern, pure]] fn sqrt(a:num) -> num;
    addP1N("cbrt", [](Number a) { return std::cbrt(a); }); // [[extern, pure]] fn cbrt(a:num) -> num;

    addP2N("pow", [](Number a, Number b) { return std::pow(a, b); });     // [[extern, pure]] fn pow(a:num, b:num) -> num;
    addP2N("atan2", [](Number a, Number b) { return std::atan2(a, b); }); // [[extern, pure]] fn atan2(a:num, b:num) -> num;
}
} // namespace PExpr::opt::intrinsics