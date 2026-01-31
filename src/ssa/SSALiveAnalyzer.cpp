#include "SSALiveAnalyzer.h"

#include "SSAInstruction.h"
#include "utils/Reporter.h"

#include <algorithm>

namespace PExpr::ssa {

std::vector<SSAValue> SSALiveAnalyzer::analyze(const InstructionList& instructions)
{
    reset();

    // Iterate through instructions in the block
    for (const auto& instr : instructions) {
        PEXPR_ASSERT(instr, "Expected valid instructions inside the list");

        // First, process all operands (uses) in the instruction
        // This ensures we capture uses that occur before definitions in the same instruction
        instr->forEachOperand([this](const SSAValue& val) {
            processOperandValue(val);
        });

        // Then, process all targets (definitions) in the instruction
        instr->forEachTarget([this](const SSAValue& val) {
            processTargetValue(val);
        });
    }

    // Gather the highest values from the targeted values
    std::unordered_map<std::string, SSAValue> highestVersion;
    for (const auto& v : mTargetedValues) {
        auto [baseName, version] = v.split();
        if (!highestVersion.contains(baseName) || highestVersion[baseName].version() < version)
            highestVersion[baseName] = v;
    }

    // Create a vector containing the highest 
    std::vector<SSAValue> result;
    result.reserve(highestVersion.size());
    for (auto [_, v] : highestVersion)
        result.push_back(v);

    return result;
}

void SSALiveAnalyzer::reset()
{
    mUsedValues.clear();
    mTargetedValues.clear();
}

void SSALiveAnalyzer::processOperandValue(const SSAValue& val)
{
    // Skip constants - they are not variables defined outside the block
    if (val.isConstant())
        return;

    // Ignore temporaries
    if (val.isTemporary())
        return;

    if (mTargetedValues.contains(val))
        return;

    mUsedValues.insert(val);
}

void SSALiveAnalyzer::processTargetValue(const SSAValue& val)
{
    // Skip constants - they can't be targets of definitions
    if (val.isConstant())
        return;

    // Ignore temporaries
    if (val.isTemporary())
        return;

    if (mUsedValues.contains(val))
        mUsedValues.erase(val);

    mTargetedValues.insert(val);
}

} // namespace PExpr::ssa
