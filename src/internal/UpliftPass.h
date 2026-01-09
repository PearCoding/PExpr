#pragma once

#include "SymbolTable.h"
#include "Closure.h"
#include "Reporter.h"
#include <map>

namespace PExpr::internal {

/// UpliftPass: transform captured variables into explicit function parameters.
/// This pass runs after typechecking and before SSA mapping. It updates
/// FunctionDeclarationStatement parameter lists and updates CallExpression
/// argument lists accordingly.
class UpliftPass {
public:
    explicit UpliftPass(const SymbolTable& globals, Reporter& reporter);

    /// Run the uplift pass on the given closure (in-place modification).
    void handle(const Ptr<Closure>& closure);

private:
    const SymbolTable& mGlobals;
    Reporter& mReporter;

    void processClosure(const Ptr<Closure>& closure, const SymbolTable& dynDefs);

    // Collect captured variable names used inside 'expr' relative to the given
    // "local" symbol table (funcDefs). Any variable resolved to a different
    // symbol table is considered captured and inserted into outCaptured map.
    void collectCapturesFromExpression(const Ptr<Expression>& expr, const SymbolTable& funcDefs, std::map<std::string, VariableDef>& outCaptured);
    void collectCapturesFromClosureBody(const Ptr<Closure>& closure, const SymbolTable& funcDefs, std::map<std::string, VariableDef>& outCaptured);

    // Traverse and update call expressions to append additional arguments when
    // the target function signature expects more parameters (e.g. uplifted captures).
    void updateCallsInExpression(const Ptr<Expression>& expr, const SymbolTable& currentDefs);
    void updateCallsInClosure(const Ptr<Closure>& closure, const SymbolTable& currentDefs);
};

} // namespace PExpr::internal
