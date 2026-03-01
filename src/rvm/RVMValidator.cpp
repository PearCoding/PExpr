#include "RVMValidator.h"
#include "RVMInterpreter.h"
#include "type/Mangler.h"

namespace PExpr::rvm {

bool RVMValidator::checkIfElementary(const RVMProgram& program)
{
    for (const auto& instr : program) {
        if (!checkIfElementary(*instr))
            return false;
    }
    return true;
}

bool RVMValidator::checkIfElementary(const RVMInstr& instr)
{
    bool bad = false;
    instr.forEachValue([&bad](const RVMValue& val) {
        if (!checkIfElementary(val))
            bad = true;
    });
    return !bad;
}

bool RVMValidator::checkIfElementary(const RVMValue& value)
{
    return value.type().isSpecified() && !value.type().isTuple();
}

bool RVMValidator::validateOptimizations(const RVMProgram& original,
                                         const RVMProgram& optimized,
                                         const type::Type& returnType)
{
    auto run = [&](const RVMProgram& prog) {
        RVMInterpreter interp;

        // Register getNumber(str) -> num
        std::vector<type::Type> params = { type::Type(type::TypeKind::String) };
        std::string getNumberMangled   = type::makeMangledNameFromTypes("getNumber", params, nullptr);
        interp.registerExternalFunction(getNumberMangled, [](const std::vector<ValueVariant>& args) -> ValueVariant {
            if (args.empty() || !std::holds_alternative<std::string>(args[0]))
                return 0.0;
            return RVMInterpreter::parseValue(std::get<std::string>(args[0]));
        });

        return interp.execute(prog, returnType);
    };

    ValueVariant resOrig = run(original);
    ValueVariant resOpt  = run(optimized);

    // Deep comparison of results (nested lambdas require a bit of care with std::function or similar for recursion)
    auto compare = [](auto&& self, const ValueVariant& v1, const ValueVariant& v2) -> bool {
        if (v1.index() != v2.index())
            return false;

        if (std::holds_alternative<std::shared_ptr<TupleVariant>>(v1)) {
            const auto& t1 = std::get<std::shared_ptr<TupleVariant>>(v1);
            const auto& t2 = std::get<std::shared_ptr<TupleVariant>>(v2);
            if (t1->elements.size() != t2->elements.size())
                return false;
            for (size_t i = 0; i < t1->elements.size(); ++i) {
                if (!self(self, t1->elements[i], t2->elements[i]))
                    return false;
            }
            return true;
        }

        return v1 == v2;
    };

    return compare(compare, resOrig, resOpt);
}

} // namespace PExpr::rvm
