#pragma once

#include "SSAStructs.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace PExpr::ssa {
/// SSAContext provides variable name generation for SSA optimization passes.
/// It analyzes an instruction list to extract current variable counters,
/// then provides a fresh() method to generate new unique variable names.
/// It also supports scope management for nested contexts.
class SSAContext {
public:
    using InstructionList = std::vector<std::shared_ptr<SSAInstr>>;

    SSAContext() = default;

    /// Analyze an instruction list and rebuild the counter map
    /// This scans all SSA values in the instructions to find the maximum
    /// version number for each base variable name
    void analyze(const InstructionList& instructions);

    /// Analyze a full SSA program (body + all functions)
    void analyze(const SSAProgram& program);

    /// Generate a fresh variable name based on the given base name
    /// Increments the counter for the base and returns a unique name
    /// @param base The base name for the variable (e.g., "name", "%", "x")
    /// @param updateScope If true, update the current scope with the new version
    /// @return A unique variable name like "name.5" or "%.12"
    std::string fresh(const std::string& base, bool updateScope = false);

    /// Push a new scope onto the scope stack
    /// Copies the current scope if one exists, otherwise creates a new empty scope
    void pushScope();

    /// Pop the current scope from the scope stack
    void popScope();

    /// Get the current version/counter for a given base name in the current scope
    /// @param base The base name to query
    /// @return The current counter value (0 if never used in current scope)
    int getCurrentVersion(const std::string& base) const;

    /// Reset all counters and clear all scopes
    void reset();

    /// Update context from a variable name (e.g., "x.3")
    /// This updates both global counters and current scope
    void update(const SSAValue& val);

private:
    /// Analyze a single SSA value and update counters
    void analyzeValue(const SSAValue& val);

    /// Get the current scope (mutable)
    std::unordered_map<std::string, int>& currentScope();
    
    /// Get the current scope (const)
    const std::unordered_map<std::string, int>& currentScope() const;

    /// Map from base variable name to the current maximum version number
    std::unordered_map<std::string, int> mCounters;
    
    /// Stack of scopes for nested contexts (each scope maps base names to versions)
    std::vector<std::unordered_map<std::string, int>> mScopeStack;
};

} // namespace PExpr::ssa
