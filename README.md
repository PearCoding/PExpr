# PExpr 2.0 [![Build Status](https://github.com/PearCoding/PExpr/actions/workflows/build.yml/badge.svg)](https://github.com/PearCoding/PExpr/actions/workflows/build.yml)

A simple, embeddable expression language designed for transpilation to other languages (GLSL, HLSL, Artic) or direct interpretation.
Optimized for computer graphics, HPC, and mathematical frameworks with a focus on compile-time optimization and ease of integration.

## ✨ Features

- **Math-Focused Syntax**: Clean, expression-oriented language with functional programming features
- **Static Type System**: Strong static typing with type inference for variables and functions
- **Rich Type Support**: Built-in vector types (`vec2`, `vec3`, `vec4`), tuples, and destructuring patterns
- **Optimization Pipeline**: Multiple optimization passes including constant folding, dead code elimination, common subexpression elimination (CSE), partial redundancy elimination (PRE), and function inlining
- **SSA-Based IR**: Static Single Assignment intermediate representation for precise analysis and optimization
- **External Function Integration**: Declare external functions with attributes (`[[extern]]`, `[[pure]]`) for seamless integration with host languages
- **CLI Compiler Tool**: `pexprc` command-line compiler with extensive optimization controls and output formats
- **Zero Dependencies**: Only requires a C++20 compiler - no external libraries needed
- **Syntax Highlighting**: VSCode extension available in `tools/syntax/`

## 🚀 Quick Start

### Building from Source

```bash
# Clone the repository
git clone https://github.com/PearCoding/PExpr.git
cd PExpr

# Configure with CMake (out-of-source build recommended)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build . --config Release
```

### Basic Example

Create a file `example.pexpr`:

```rust
// Function definition with type annotations
fn add(x: int, y: int) = x + y;

// External function declaration (implemented by host)
[[extern]] fn sqrt(x: num) -> num;

// Vector operations
let v: vec3 = [1.0, 2.0, 3.0];
let result = v.x + v.y * 2.0;

// Conditional expression
let value = if result > 5.0 { 
    sqrt(result) 
} else { 
    result * 2.0 
};

// Return the final value
value
```

Compile and optimize:

```bash
# Compile to SSA IR with optimizations
./bin/pexprc example.pexpr -O2 -o example.pexprir

# Emit AST instead of IR
./bin/pexprc example.pexpr --emit-ast -o example-ast.pexpr
```

## 📖 Language Syntax

PExpr combines mathematical expression syntax with functional programming concepts. See [docs/PExprGrammar.md](docs/PExprGrammar.md) for complete grammar.

### Variables and Types

```rust
// Immutable variable
let x = 42;

// Mutable variable  
let mut counter = 0;
counter = counter + 1;

// Type annotations
let position: vec3 = [1.0, 2.0, 3.0];
let tuple: [int, num, bool] = [10, 3.14, true];

// Type aliases
using Vector2 = vec2;
let v: Vector2 = [0.5, 0.5];
```

### Functions

```rust
// Simple function
fn square(x: num) = x * x;

// Overloaded functions
fn add(x: int, y: int) -> int = x + y;
fn add(x: num, y: num) -> num = x + y;

// External function (implemented by host)
[[extern]] fn sin(x: num) -> num;
[[extern, pure]] fn cos(x: num) -> num;

// Function with statements
fn complexOperation(x: num) -> num = {
    let intermediate = x * 2.0;
    intermediate + 1.0
};
```

### Control Flow

```rust
// If expression (returns a value)
let absValue = if x < 0 {
    -x
} else {
    x
};

// If-elif-else chain
let classification = if value < 0.0 {
    "negative"
} elif value == 0.0 {
    "zero"
} else {
    "positive"
};

// If statement
let mut k = 0;
if x < 0 {
    k = -x;
};
```

### Tuples and Destructuring

```rust
// Tuple creation
let point = [10, 20, 30];

// Tuple access
let x = point[0];
let y = point[1];

// Destructuring declaration
let *[a, b, c] = point;

// Nested destructuring
let nested = [[1, 2], [3, 4]];
let *[[x1, y1], [x2, y2]] = nested;

// Destructuring assignment
let mut p = 0;
let mut q = 0;
*[p, q] = [5, 10];
```

### Vector Operations

```rust
// Vector creation
let color: vec3 = [0.5, 0.7, 1.0];

// Swizzle operations
let xy = color.xy;   // Extract first two components
let rgb = color.rgb; // All three components
let bgr = color.bgr; // Reversed components

// Vector arithmetic
let brightened = color * 1.2;
let mixed = mix(color1, color2, 0.5);
```

## 🔧 Optimization Pipeline

PExpr performs extensive compile-time optimizations:

### Available Optimizations

| Optimization | Description | CLI Flag |
|--------------|-------------|----------|
| **Constant Folding** | Evaluate constant expressions at compile time | `--opt-constant-folding` |
| **Math Constant Folding** | Fold mathematical operations on numbers | `--opt-math-folding` |
| **Dead Code Elimination** | Remove unreachable code | `--opt-dead-code` |
| **Function Inlining** | Inline small functions at call sites | `--opt-inline-functions` |
| **Math Identities** | Apply algebraic identities (x*0=0, x*1=x) | `--opt-math-identities` |
| **Trigonometric Identities** | Simplify trigonometric expressions | `--opt-trigonometric-identities` |
| **Common Subexpression Elimination** | Eliminate duplicate computations | `--opt-cse` |
| **Partial Redundancy Elimination** | Move invariant computations out of loops | `--opt-pre` |

### Optimization Levels

The `pexprc` compiler supports optimization levels similar to traditional compilers:

```bash
# No optimizations (fastest compilation and default)
pexprc input.pexpr -O0

# Basic optimizations
pexprc input.pexpr -O1

# Moderate optimizations
pexprc input.pexpr -O2

# Aggressive optimizations
pexprc input.pexpr -O3
```

## 🛠️ CLI Compiler: `pexprc`

The `pexprc` tool provides comprehensive control over compilation:

### Basic Usage

```bash
# Compile with default optimizations
pexprc input.pexpr

# Specify output file
pexprc input.pexpr -o output.pexprir

# Emit AST instead of IR
pexprc input.pexpr --emit-ast

# Read SSA IR instead of PExpr source
pexprc input.pexprir --input-ir
```

### Optimization Control

```bash
# Enable specific optimizations
pexprc input.pexpr --opt-constant-folding --opt-dead-code --opt-cse

# Disable specific optimizations  
pexprc input.pexpr --no-opt-constant-folding --no-opt-inline-functions

# Set optimization level
pexprc input.pexpr -O2

# Force inline all functions
pexprc input.pexpr --opt-force-inline-functions
```

### Warning Control

```bash
# Enable all warnings
pexprc input.pexpr -W all

# Treat warnings as errors
pexprc input.pexpr -W error

# Control specific warnings
pexprc input.pexpr -W trailing-semicolon -W implicit-cast

# Disable all warnings
pexprc input.pexpr --no-warnings
```

## 📚 Integration / API

PExpr is designed to be embedded in other C++ projects:

### Basic Integration Example

```cpp
#include "Environment.h"
#include "opt/Optimizer.h"
#include "ssa/SSAMapper.h"

using namespace PExpr;

// Create environment
Environment env;

// Parse source code
auto ast = env.parse("fn square(x: num) -> num = x * x; square(4.0)");

// Map to SSA IR
auto program = env.map(ast);

// Apply optimizations
env.optimize(program /* in/out */, options);

// Use the optimized program...
```

### CMake Integration

```cmake
# Add PExpr as a subdirectory
add_subdirectory(path/to/pexpr)

# Link against PExpr library
target_link_libraries(your_target PRIVATE pexpr)
```

## 🧪 Testing

PExpr includes an extensive test suite:

```bash
# Build and run tests
cd build
ctest --output-on-failure
```

Test examples are organized by component in `test/examples/`:
- `parser/` - Parser edge cases and syntax tests
- `ssa/` - SSA mapper edge cases  
- `opt/` - Optimization tests (constant folding, CSE, dead code elimination)
- `typechecker/` - Type checker error cases
- `uplift/` - Variable capture tests

## 🤝 Contributing

Contributions are welcome! Please see [TODO.md](TODO.md) for current development goals.

### Development Guidelines

1. **Code Style**: Follow the `.clang-format` configuration
2. **Testing**: Add tests for new features in `test/unittests/` or `test/examples/`
3. **Documentation**: Update relevant documentation in `docs/`
4. **Commit Messages**: Use descriptive commit messages

### Building for Development

```bash
# Debug build with assertions
mkdir build-debug && cd build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DPEXPR_WITH_ASSERTS=ON
cmake --build .
```

## 📄 License

PExpr is licensed under the terms in [LICENSE.txt](LICENSE.txt).

## 🔗 Similar Libraries

- **[SeExpr](https://github.com/wdas/SeExpr)**: More mature expression language with many built-in functions and runners
- **[Ignis](https://github.com/PearCoding/Ignis)**: Renderer using PExpr for shader expressions (by the same author)

PExpr focuses on being lightweight and easy to transpile, making it ideal for embedding in projects where SeExpr might be too complex.

---

**PExpr** is developed and maintained by [Ömercan Yazici](https://github.com/PearCoding).