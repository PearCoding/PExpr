#pragma once

#include "SymbolTable.h"
#include "ast/Closure.h"
#include "ast/Pattern.h"
#include "utils/Reporter.h"

#include <map>
#include <set>
#include <vector>

namespace PExpr::type {

/// UpliftPass: transform captured variables into explicit function parameters.
/// This pass runs after typechecking and before SSA mapping. It updates
/// FunctionDeclarationStatement parameter lists and updates CallExpression
/// argument lists accordingly and wraps the call in a closure if mutable variables are captured.
class UpliftPass {
public:
    explicit UpliftPass(utils::Reporter& reporter);

    /// Run the uplift pass on the given closure (in-place modification).
    void handle(const Ptr<ast::Closure>& closure);

private:
    utils::Reporter& mReporter;

    void processClosure(const Ptr<ast::Closure>& closure);

    // Collect captured variable names used inside 'expr' relative to the given
    // "function" closure. Any variable resolved to a symbol table above this is
    // considered captured and inserted into outCaptured map.
    void collectCapturesFromExpression(const Ptr<ast::Closure>& funcClosure, const Ptr<ast::Closure>& closure,
                                       const Ptr<ast::Expression>& expr,
                                       std::map<std::string, VariableDef>& outCaptured,
                                       std::set<std::string>& mutableAssignments);
    void collectCapturesFromClosureBody(const Ptr<ast::Closure>& funcClosure, const Ptr<ast::Closure>& closure,
                                        std::map<std::string, VariableDef>& outCaptured,
                                        std::set<std::string>& mutableAssignments);

    // Collect mutable assignments from a pattern (for variable assignment statements)
    void collectMutableAssignmentsFromPattern(const Ptr<ast::Closure>& funcClosure, const Ptr<ast::Closure>& closure,
                                              const Ptr<ast::Pattern>& pattern, std::set<std::string>& mutableAssignments);

    // Traverse and update call expressions to append additional arguments when
    // the target function signature expects more parameters (e.g. uplifted captures).
    void updateCallsInExpression(Ptr<ast::Expression>& expr, const FunctionDef& oldDef, const FunctionDef& newDef,
                                 const std::vector<std::string>& mutableCaptures);
    void updateCallsInClosure(const Ptr<ast::Closure>& closure, const FunctionDef& oldDef, const FunctionDef& newDef,
                              const std::vector<std::string>& mutableCaptures);

    // Create a tuple expression for the return value that includes original return
    // plus updated mutable captured variables.
    Ptr<ast::Expression> createReturnTuple(const Ptr<ast::Closure>& funcClosure,
                                           const Ptr<ast::Expression>& originalReturnExpr,
                                           const std::vector<std::string>& mutableCaptures);
};

} // namespace PExpr::type
