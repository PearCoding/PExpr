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

    // Traverse and update call expressions to append additional arguments when
    // the target function signature expects more parameters (e.g. uplifted captures).
    void updateCallsInExpression(const Ptr<ast::Closure>& currentClosure, Ptr<ast::Expression>& expr,
                                 const FunctionDef& oldDef, const FunctionDef& newDef,
                                 const std::unordered_map<Ptr<VariableDef>, Ptr<VariableDef>>& parameterToCaptured,
                                 const std::map<std::string, Ptr<VariableDef>>& mutableCaptures);
    void updateCallsInClosure(const Ptr<ast::Closure>& closure,
                     const FunctionDef& oldDef, const FunctionDef& newDef,
                     const std::unordered_map<Ptr<VariableDef>, Ptr<VariableDef>>& parameterToCaptured,
                     const std::map<std::string, Ptr<VariableDef>>& mutableCaptures);

    void updateVariables(const Ptr<ast::Closure>& closure, const std::unordered_map<Ptr<VariableDef>, Ptr<VariableDef>>& capturedToParameter);

    // Create a tuple expression for the return value that includes original return
    // plus updated mutable captured variables.
    Ptr<ast::Expression> createReturnTuple(const Ptr<ast::Closure>& funcClosure,
                                           const Ptr<ast::Expression>& originalReturnExpr,
                                           const std::map<std::string, Ptr<VariableDef>>& mutableCaptures);
};

} // namespace PExpr::type
