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
    /// @param[out] outCapturedUsage Map of captured variables used in the closure (includes modification)
    /// @param[out] outCapturedMutable Map of captured variables modified in the closure
    void analyzeClosure(const ast::Closure* focusedClosure,
                        std::map<std::string, Ptr<VariableDef>>& outCapturedUsage,
                        std::map<std::string, Ptr<VariableDef>>& outCapturedMutable);

private:
    utils::Reporter& mReporter;

    /// Collect mutable assignments from an expression (for assignment expressions like [a, b] = ... or c = f)
    void collectMutableAssignmentsFromExpression(const ast::Closure* focusedClosure, const ast::Closure* currentClosure,
                                                 const Ptr<ast::Expression>& expr,
                                                 std::map<std::string, Ptr<VariableDef>>& outCapturedUsage,
                                                 std::map<std::string, Ptr<VariableDef>>& outCapturedMutable);

    const bool mCaptureUsage;
    const bool mCaptureModification;
};

} // namespace PExpr::type