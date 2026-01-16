# PExpr [![Build](https://github.com/PearCoding/PExpr/actions/workflows/build.yml/badge.svg)](https://github.com/PearCoding/PExpr/actions/workflows/build.yml)

A fairly simple inline programming language to be transpiled to other languages or directly interpreted.
The intended field of application is computer graphics, hpc and other math frameworks.

## Why?

In contrary to many other expressive languages, this one is solely intended to be transpiled to other languages (e.g., GLSL, HLSL, Artic) or run on a stack based virtual interpreter. It allows easy definition of external functions, variables and constants. Therefore, the syntax is simple and limited to a math style expression and functional style.

## Does it optimize?

Optionally, yes. The following optimizations are available:

- Constant Folding
- Constant Folding of math expressions
- Dead-Code removal
- Function inlining

## What is the output?

Depends, the output can be a AST in code or optimized SSA IR ready for interpretation.

## Dependencies

PExpr has no other dependencies, except a modern C++20 compiler.

## Similar libraries

If you want to embed and directly use PExpr, it is recommended to use [SeExpr](https://github.com/wdas/SeExpr) instead, as it already has many "runners" included, has many standard functions and is already feature proof. On the other hand, PExpr is easier to build and to transpile to other languages than SeExpr.

Keep in mind that this project has no connection to SeExpr, except that it is the reason I started this project. SeExpr is too complex for my other projects (e.g., [Ignis](https://github.com/PearCoding/Ignis)) to embed into. 
