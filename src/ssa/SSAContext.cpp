#include "SSAContext.h"

#include <algorithm>
#include <ranges>
#include <sstream>

namespace PExpr::ssa {
void SSAContext::analyze(const InstructionList& instructions)
{
    // Scan all instructions and extract variable names to build counters
    std::ranges::for_each(instructions, [this](const auto& instrPtr) {
        PEXPR_ASSERT(instrPtr, "Expected valid instructions inside the list");
        instrPtr->forEachValue([this](const SSAValue& val) {
            analyzeValue(val);
        });
    });
}

void SSAContext::analyze(const SSAProgram& program)
{
    // Analyze main body
    analyze(program.Body);

    // Analyze all functions
    for (const auto& func : program.Functions)
        analyze(func.Body);
}

void SSAContext::analyzeValue(const SSAValue& val)
{
    // Only analyze named values (not constants)
    if (val.isConstant())
        return;

    PEXPR_ASSERT(!val.name().empty(), "A non-constant value must have a valid name");

    // Try to parse the variable name
    auto [base, version] = val.split();

    // Update the counter if this version is higher than current
    auto& counter = mCounters[base];
    if (version > counter)
        counter = version;
}

std::string SSAContext::fresh(const std::string& base, bool updateScope)
{
    int& c = mCounters[base];
    ++c;

    if (updateScope && !mScopeStack.empty())
        currentScope()[base] = c;

    std::stringstream ss;
    ss << base << "." << c;
    return ss.str();
}

void SSAContext::pushScope()
{
    if (mScopeStack.empty())
        mScopeStack.emplace_back();
    else
        mScopeStack.push_back(currentScope()); // copy current scope
}

void SSAContext::popScope()
{
    PEXPR_ASSERT(!mScopeStack.empty(), "Scope stack underflow");
    mScopeStack.pop_back();
}

int SSAContext::getCurrentVersion(const std::string& base) const
{
    if (!mScopeStack.empty()) {
        const auto& scope = currentScope();
        if (const auto it = scope.find(base); it != scope.end())
            return it->second;

        return -1;
    } else {
        if (const auto it = mCounters.find(base); it != mCounters.end())
            return it->second;

        return -1;
    }
}

void SSAContext::reset()
{
    mCounters.clear();
    mScopeStack.clear();
}

void SSAContext::update(const SSAValue& val)
{
    if (val.isConstant())
        return;

    auto [base, version] = val.split();

    // Update the counter if this version is higher than current
    auto& counter = mCounters[base];
    if (version > counter)
        counter = version;

    // Also update the current scope if we're in a scope
    if (!mScopeStack.empty())
        currentScope()[base] = version;
}

std::unordered_map<std::string, int>& SSAContext::currentScope()
{
    PEXPR_ASSERT(!mScopeStack.empty(), "No scope active");
    return mScopeStack.back();
}

const std::unordered_map<std::string, int>& SSAContext::currentScope() const
{
    PEXPR_ASSERT(!mScopeStack.empty(), "No scope active");
    return mScopeStack.back();
}

} // namespace PExpr::ssa
