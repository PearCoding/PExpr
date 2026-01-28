#pragma once

#include "SymbolTable.h"
#include "ast/Closure.h"
#include "ast/Pattern.h"
#include "utils/Reporter.h"

#include <map>
#include <set>
#include <vector>

namespace PExpr::type {

/// Utility class for analyzing closures to detect captured and mutated variables.
class ClosureAnalyzer {
public:
    /// @param captureUsage If true usage of variables will be captured
    /// @param captureModification If true mutable variable modification will be captured
    ClosureAnalyzer(utils::Reporter& reporter, bool captureUsage, bool captureModification);

    /// Analyze a closure to detect variables captured from parent scopes.
    /// @param focusedClosure All captured variables must be above this closure
    /// @param currentClosure The closure to analyze
    /// @param[out] outCapturedUsage Map of captured variables used in the closure (includes modification)
    /// @param[out] outCapturedMutable Map of captured variables modified in the closure
    void analyzeClosure(const Ptr<ast::Closure>& focusedClosure, const Ptr<ast::Closure>& currentClosure,
                        std::map<std::string, VariableDef>& outCapturedUsage,
                        std::map<std::string, VariableDef>& outCapturedMutable);

private:
    utils::Reporter& mReporter;

    /// Collect captured variable names used inside 'expr' relative to the given
    /// "focused" closure. Any variable resolved to a symbol table above this is
    /// considered captured and inserted into outCapturedUsage map and if modified outCapturedMutable as well.
    void collectCapturesFromExpression(const Ptr<ast::Closure>& focusedClosure, const Ptr<ast::Closure>& currentClosure,
                                       const Ptr<ast::Expression>& expr,
                                       std::map<std::string, VariableDef>& outCapturedUsage,
                                       std::map<std::string, VariableDef>& outCapturedMutable);
    void collectCapturesFromClosureBody(const Ptr<ast::Closure>& focusedClosure, const Ptr<ast::Closure>& currentClosure,
                                        std::map<std::string, VariableDef>& outCapturedUsage,
                                        std::map<std::string, VariableDef>& outCapturedMutable);

    /// Collect mutable assignments from a pattern (for variable assignment statements)
    void collectMutableAssignmentsFromPattern(const Ptr<ast::Closure>& focusedClosure, const Ptr<ast::Closure>& currentClosure,
                                              const Ptr<ast::Pattern>& pattern,
                                              std::map<std::string, VariableDef>& outCapturedUsage,
                                              std::map<std::string, VariableDef>& outCapturedMutable);

    const bool mCaptureUsage;
    const bool mCaptureModification;
};

} // namespace PExpr::type