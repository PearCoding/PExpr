# Common Subexpression Elimination (CSE) with Basic Block Analysis

## Why Basic Blocks Matter for CSE

Common Subexpression Elimination (CSE) is an optimization that eliminates redundant computations by reusing previously computed results. However, naive CSE that operates across entire functions can be **unsafe** and **incorrect** due to control flow considerations. This document explains why basic block boundaries are crucial for correct CSE.

## The Problem: Control Flow Dependencies

Consider this simple program:

```rust
fn example(cond: bool) -> num = {
    if cond {
        let x = expensiveCompute() * 2.0;
        x
    } else {
        let y = expensiveCompute() * 2.0;  // Same expression!
        y
    }
};
```

Naive CSE might think: "Both branches compute `expensiveCompute() * 2.0`, let's compute it once before the branch and reuse it." This is **WRONG** because:

1. **Different execution paths**: The expression is only needed on the path that's actually taken
2. **Side effects**: `expensiveCompute()` might have different side effects on different calls
3. **Value differences**: The function might return different values on different calls

## Basic Block Boundaries

A **basic block** is a sequence of instructions with:
- Single entry point (first instruction)
- Single exit point (last instruction)  
- No internal branches (except at the end)

Control flow can only enter at the beginning and exit at the end. This makes basic blocks the natural unit for many optimizations.

## CSE Safety Rules with Basic Blocks

### Rule 1: CSE Only Within Same Basic Block
Expressions can only be eliminated if they appear in the **same basic block**. This ensures:
- All operands are defined on the same execution path
- No control flow can change which definition reaches the expression
- The expression is guaranteed to execute if the block executes

### Rule 2: No CSE Across Different Dominance Regions
Even if two blocks are in the same function, they may have different **dominance** relationships:
- Block A dominates Block B if all paths to B go through A
- An expression from a dominating block cannot be reused in a dominated block unless it's **available** on all paths

### Rule 3: Respect Side Effects
Functions with side effects (I/O, mutation) cannot be eliminated even within the same block unless they're known to be **pure**.

## Examples from `cse_basic_blocks.pexpr`

### Example 1: Branch-Separated Identical Expressions
```rust
if cond {
    // Block 1
    x = a * b + 5.0;
    y = a * b + 5.0;  // Same block - CAN eliminate
} else {
    // Block 2  
    x = a * b + 5.0;  // Different block - CANNOT eliminate from Block 1
    z = a * b + 5.0;  // Same block - CAN eliminate within Block 2
}
```

**Correct CSE Behavior**:
- Eliminate `y` in Block 1 (reuse `x`)
- Eliminate `z` in Block 2 (reuse `x` in Block 2)
- Keep separate computations in Block 1 and Block 2

### Example 2: Nested Control Flow
```rust
if cond1 {
    if cond2 {
        // Block A1
        x = getInputPure();
        y = getInputPure();  // Same block - eliminate
    } else {
        // Block A2
        x = getInputPure();  // Different block from A1
        y = getInputPure();  // Same block - eliminate within A2
    }
}
```

**Key Insight**: Block A1 and A2 are mutually exclusive - they can never both execute. CSE across them is not just unsafe, it's impossible.

## Why This Matters for PRE (Partial Redundancy Elimination)

PRE builds on CSE concepts but is more sophisticated:
- **CSE**: Eliminates redundancy when expressions are identical on the same path
- **PRE**: Eliminates redundancy when expressions are identical on some but not all paths

PRE needs basic block analysis even more critically because it:
1. Computes **availability** (which blocks have the expression)
2. Computes **anticipability** (which blocks need the expression)
3. Finds **placement points** (where to insert computations)
4. Performs **code motion** (moving computations to optimal locations)

Without accurate basic block identification, PRE could:
- Insert computations in wrong places
- Move code across unsafe boundaries
- Create incorrect program behavior

## Testing Basic Block-Aware CSE

The test file `test/examples/sscp/cse_basic_blocks.pexpr` contains examples that should produce different SSA IR with block-aware CSE:

### Verification:
Compile with and without `EliminateCommonSubexpressions` option and compare:
- Count of `getInputPure()` expressions
- Placement of computations relative to branches
- Phi node generation for moved values
