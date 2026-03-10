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

namespace {

// Helper function to convert ValueVariant to a readable string
std::string valueVariantToString(const ValueVariant& val)
{
    if (std::holds_alternative<Number>(val)) {
        return "Number(" + std::to_string(std::get<Number>(val)) + ")";
    } else if (std::holds_alternative<Integer>(val)) {
        return "Integer(" + std::to_string(std::get<Integer>(val)) + ")";
    } else if (std::holds_alternative<bool>(val)) {
        return std::string("bool(") + (std::get<bool>(val) ? "true" : "false") + ")";
    } else if (std::holds_alternative<std::string>(val)) {
        return "String(\"" + std::get<std::string>(val) + "\")";
    } else if (std::holds_alternative<Tuple>(val)) {
        const auto& tuple = std::get<Tuple>(val);
        std::string result = "Tuple[";
        for (size_t i = 0; i < tuple->elements.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += valueVariantToString(tuple->elements[i]);
        }
        result += "]";
        return result;
    }
    return "Unknown";
}

} // anonymous namespace

bool RVMValidator::validateOptimizations(const RVMProgram& original,
                                         const RVMProgram& optimized,
                                         const type::Type& returnType,
                                         std::string& errorMsg)
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

        // Register passthrough(num) -> num
        {
            std::vector<type::Type> params = { type::Type(type::TypeKind::Number) };
            std::string mangled = type::makeMangledNameFromTypes("passthrough", params, nullptr);
            interp.registerExternalFunction(mangled, [](const std::vector<ValueVariant>& args) -> ValueVariant {
                if (args.empty() || !std::holds_alternative<Number>(args[0]))
                    return 0.0;
                return std::get<Number>(args[0]);
            });
        }

        // Register passthrough(int) -> int
        {
            std::vector<type::Type> params = { type::Type(type::TypeKind::Integer) };
            std::string mangled = type::makeMangledNameFromTypes("passthrough", params, nullptr);
            interp.registerExternalFunction(mangled, [](const std::vector<ValueVariant>& args) -> ValueVariant {
                if (args.empty() || !std::holds_alternative<Integer>(args[0]))
                    return Integer(0);
                return std::get<Integer>(args[0]);
            });
        }

        // Register passthrough(bool) -> bool
        {
            std::vector<type::Type> params = { type::Type(type::TypeKind::Boolean) };
            std::string mangled = type::makeMangledNameFromTypes("passthrough", params, nullptr);
            interp.registerExternalFunction(mangled, [](const std::vector<ValueVariant>& args) -> ValueVariant {
                if (args.empty() || !std::holds_alternative<bool>(args[0]))
                    return false;
                return std::get<bool>(args[0]);
            });
        }

        // Register passthrough(vec2) -> vec2
        {
            std::vector<type::Type> params = { type::Type::AsVector(2) };
            std::string mangled = type::makeMangledNameFromTypes("passthrough", params, nullptr);
            interp.registerExternalFunction(mangled, [](const std::vector<ValueVariant>& args) -> ValueVariant {
                if (args.size() < 2)
                    return ValueVariant{};
                auto tuple = std::make_shared<TupleVariant>();
                tuple->elements = { args[0], args[1] };
                return ValueVariant{ tuple };
            });
        }

        // Register passthrough(vec3) -> vec3
        {
            std::vector<type::Type> params = { type::Type::AsVector(3) };
            std::string mangled = type::makeMangledNameFromTypes("passthrough", params, nullptr);
            interp.registerExternalFunction(mangled, [](const std::vector<ValueVariant>& args) -> ValueVariant {
                if (args.size() < 3)
                    return ValueVariant{};
                auto tuple = std::make_shared<TupleVariant>();
                tuple->elements = { args[0], args[1], args[2] };
                return ValueVariant{ tuple };
            });
        }

        // Register passthrough(vec4) -> vec4
        {
            std::vector<type::Type> params = { type::Type::AsVector(4) };
            std::string mangled = type::makeMangledNameFromTypes("passthrough", params, nullptr);
            interp.registerExternalFunction(mangled, [](const std::vector<ValueVariant>& args) -> ValueVariant {
                if (args.size() < 4)
                    return ValueVariant{};
                auto tuple = std::make_shared<TupleVariant>();
                tuple->elements = { args[0], args[1], args[2], args[3] };
                return ValueVariant{ tuple };
            });
        }

        return interp.execute(prog, returnType);
    };

    ValueVariant resOrig = run(original);
    ValueVariant resOpt  = run(optimized);

    // Deep comparison of results with detailed error reporting
    auto compare = [](auto&& self, const ValueVariant& v1, const ValueVariant& v2, std::string& err, const std::string& path = "") -> bool {
        if (v1.index() != v2.index()) {
            err = "Type mismatch at " + path + ": original has type " + std::to_string(v1.index()) + 
                  " (" + valueVariantToString(v1) + "), optimized has type " + std::to_string(v2.index()) + 
                  " (" + valueVariantToString(v2) + ")";
            return false;
        }

        if (std::holds_alternative<Tuple>(v1)) {
            const auto& t1 = std::get<Tuple>(v1);
            const auto& t2 = std::get<Tuple>(v2);
            if (t1->elements.size() != t2->elements.size()) {
                err = "Tuple size mismatch at " + path + ": original has " + std::to_string(t1->elements.size()) + 
                      " elements, optimized has " + std::to_string(t2->elements.size()) + " elements";
                return false;
            }
            for (size_t i = 0; i < t1->elements.size(); ++i) {
                std::string elementPath = path + "[" + std::to_string(i) + "]";
                if (!self(self, t1->elements[i], t2->elements[i], err, elementPath)) {
                    return false;
                }
            }
            return true;
        }

        if (v1 != v2) {
            err = "Value mismatch at " + path + ": original returned " + valueVariantToString(v1) + 
                  ", optimized returned " + valueVariantToString(v2);
            return false;
        }

        return true;
    };

    return compare(compare, resOrig, resOpt, errorMsg, "<root>");
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
