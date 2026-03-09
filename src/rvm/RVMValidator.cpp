#include "RVMValidator.h"
#include "RVMInterpreter.h"
#include "RVMLiveAnalyzer.h"
#include "type/Mangler.h"

#include <unordered_map>
#include <unordered_set>

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
    // Skip CALL and RET instructions - they generate placeholder values
    // with unspecified types in forEachValue - by design
    Opcode op = instr.opcode();
    if (op == Opcode::CALL_INTERNAL || op == Opcode::CALL_EXTERNAL || op == Opcode::RET)
        return true;

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
        {
            std::vector<type::Type> params = { type::Type(type::TypeKind::String) };
            std::string getNumberMangled   = type::makeMangledNameFromTypes("getNumber", params, nullptr);
            interp.registerExternalFunction(getNumberMangled, [](const std::vector<ValueVariant>& args) -> ValueVariant {
                if (args.empty() || !std::holds_alternative<std::string>(args[0]))
                    return 0.0;
                return RVMInterpreter::parseValue(std::get<std::string>(args[0]));
            });
        }

        // Register pass(num) -> num
        {
            std::vector<type::Type> params = { type::Type(type::TypeKind::Number) };
            std::string passMangled        = type::makeMangledNameFromTypes("pass", params, nullptr);
            interp.registerExternalFunction(passMangled, [](const std::vector<ValueVariant>& args) -> ValueVariant {
                if (args.empty() || !std::holds_alternative<Number>(args[0]))
                    return 0.0;
                return std::get<Number>(args[0]);
            });
        }

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

bool RVMValidator::validateUseBeforeDefinition(const RVMProgram& program, std::string& errorMsg)
{
    std::unordered_map<RegId, size_t> firstUsePositions;
    std::unordered_map<RegId, size_t> firstDefPositions;

    for (size_t i = 0; i < program.size(); ++i) {
        const auto& instr = program[i];

        // Check for uses
        instr->forEachSource([&](const RVMValue& src) {
            if (src.isRegister()) {
                RegId reg = src.regId();
                if (firstUsePositions.find(reg) == firstUsePositions.end())
                    firstUsePositions[reg] = i;
            }
        });

        // Check definitions
        instr->forEachDestination([&](const RVMValue& dst) {
            if (dst.isRegister()) {
                RegId reg = dst.regId();
                if (firstDefPositions.find(reg) == firstDefPositions.end())
                    firstDefPositions[reg] = i;
            }
        });
    }

    // Check for any uses that occur before their first definition
    for (const auto& [reg, usePos] : firstUsePositions) {
        auto it = firstDefPositions.find(reg);
        if (it == firstDefPositions.end()) {
            // Register used but never defined
            errorMsg = "Register " + std::to_string(reg) + " used but never defined";
            return false;
        }
        if (usePos < it->second) {
            // Register used before its first definition
            errorMsg = "Register " + std::to_string(reg) + " used at position " + std::to_string(usePos) + " before definition at position " + std::to_string(it->second);
            return false;
        }
    }

    return true;
}

bool RVMValidator::validateRegisterAllocation(const RVMProgram& program, std::string& errorMsg)
{
    // Get live intervals
    auto intervals = RVMLiveAnalyzer::analyzeProgram(program);

    // Check for overlapping intervals with the same allocated register
    std::unordered_map<RegId, std::vector<RVMLiveAnalyzer::LiveInterval>> allocatedIntervals;

    // Group intervals by their original register (for validation, not allocation)
    for (const auto& interval : intervals) {
        allocatedIntervals[interval.Register].push_back(interval);
    }

    // Check each register's intervals for overlaps
    for (const auto& [reg, regIntervals] : allocatedIntervals) {
        // Sort by start position
        auto sortedIntervals = regIntervals;
        std::sort(sortedIntervals.begin(), sortedIntervals.end(),
                  [](const RVMLiveAnalyzer::LiveInterval& a, const RVMLiveAnalyzer::LiveInterval& b) {
                      return a.Start < b.Start;
                  });

        // Check for overlaps
        for (size_t i = 1; i < sortedIntervals.size(); ++i) {
            // Check if intervals overlap (end > start of next)
            if (sortedIntervals[i - 1].End > sortedIntervals[i].Start) {
                errorMsg = "Register " + std::to_string(reg) + " has overlapping live intervals [" + std::to_string(sortedIntervals[i - 1].Start) + "-" + std::to_string(sortedIntervals[i - 1].End) + "] and [" + std::to_string(sortedIntervals[i].Start) + "-" + std::to_string(sortedIntervals[i].End) + "]";
                return false;
            }
        }
    }

    return true;
}

} // namespace PExpr::rvm
