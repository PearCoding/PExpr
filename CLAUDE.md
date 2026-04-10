# CLAUDE.md — PExpr Development Guide

## What is PExpr?

PExpr is a C++20 expression language compiler. Source code is parsed into an AST,
lowered to SSA IR, optimised across multiple passes, then lowered again to a
register-based virtual machine (RVM) that can be interpreted or serialised.

Pipeline: **Source → Parser → AST → TypeChecker → SSA IR → Optimiser → RVM IR → Interpreter/Serialiser**

## Build

On Windows use the ".\build-claude.bat", but ensure that path resolving in "cmd" and/or powershell works as intended.

```bash
# First-time configure (only needed once or after CMake changes):
./build-claude.bat configure Release    # or Debug

# Build:
./build-claude.bat Release              # or Debug
```

On Linux you may use the basic cmake + Ninja workflow.

## Test

```bash
ctest --test-dir build --build-config Release --output-on-failure
```

Tests use **Catch2 v3**. Run a subset by tag:

```bash
build/bin/Release/pexpr_tests "[sscp][cse]"
```

Common tags: `[parser]`, `[sscp]`, `[cse]`, `[pre]`, `[rvm]`, `[identity]`, `[regression]`.

Test files live in `test/unittests/` mirroring `src/` structure. Example `.pexpr` files
live in `test/examples/`.

## Code style

- **Formatting**: `.clang-format` (WebKit-based, 4-space indent, no tabs, no column limit).
- **Files**: `CamelCase.h/.cpp` matching class names.
- **Classes**: `CamelCase`. Members: `mCamelCase`. Functions: `camelCase`.
- **Enums**: `CamelCase` values (`TypeKind::Integer`, `BinaryOperation::Add`).
- **Macros**: `PEXPR_UPPER_CASE` (`PEXPR_ASSERT`, `PEXPR_UNUSED`, `PEXPR_LIKELY`).
- **Namespaces**: `PExpr::parser`, `PExpr::ast`, `PExpr::ssa`, `PExpr::opt`, `PExpr::rvm`.
  No indentation inside namespace bodies.
- Consecutive assignments are column-aligned.

## Common pitfalls

### SSAValue: constant vs named — guard `.name()` calls

`SSAValue` is either **constant** (holds a literal) or **named** (holds a string name
like `"%.3"` or `"x_L1C5.1"`). Calling `.name()` on a constant triggers
`PEXPR_ASSERT(!isConstant(), ...)`.

```cpp
// WRONG — crashes if val is a constant:
auto n = val.name();

// CORRECT:
if (!val.isConstant()) {
    auto n = val.name();
}
```

Use `valueAsIf<T>()` (returns `nullptr` on type mismatch) for safe extraction
from the inner `ValueVariant`.

### The constant folder propagates *names*, not just constants

`SSCPConstantFolder::foldAssign` returns the operand as-is for simple assignments
(`target = operand`), even when the operand is a non-constant named value. This is
intentional — it implements **copy propagation** which CSE depends on. Do not "fix"
this by restricting it to constants only, or CSE tests will break.

### Bracket-aware parsing for tuple types

Tuple types like `[int, num]` contain commas. Any parser that splits on commas
(e.g. for value lists or function arguments) must track bracket nesting depth, or
it will mis-split nested tuple types.

## Error handling conventions

- **`PEXPR_ASSERT(cond, msg)`** — for internal invariants / programmer errors.
  Prints file:line:function, triggers debugger break in Debug, aborts in Release.
- **`Reporter::error(location, msg)` / `Reporter::warning(...)`** — for user-facing
  compilation errors. Collected and printed with source locations.
- **`std::optional` / `dynamic_cast` null checks** — for expected control flow
  (e.g. an operand that may or may not be foldable).

## PExpr language quick reference

```rust
let x = 42;                              // immutable binding
let mut y = 0;                           // mutable binding
y = y + 1;

fn square(x: num) -> num = x * x;        // function
@[extern, pure] fn sin(a: num) -> num;   // external function

let v = [1.0, 2.0, 3.0];                 // vec3 literal
v.xy                                     // swizzle → vec2
v.x                                      // scalar access

if x > 0 { x } else { -x }               // if-expression

using Vec2 = vec2;                       // type alias
let [a, b] = [1, 2];                     // destructuring
```

## Coding guidelines

### 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

### 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

### 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.

The test: Every changed line should trace directly to the user's request.

### 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.