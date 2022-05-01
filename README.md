# PExpr [![Build](https://github.com/PearCoding/PExpr/actions/workflows/build.yml/badge.svg)](https://github.com/PearCoding/PExpr/actions/workflows/build.yml)

A fairly simple inline expression optimized to be transpiled to other languages.
The intended field of application is computer graphics, hpc and other math frameworks.

## Why?

In contrary to many other expressive languages, this one is solely intended to be transpiled to other languages (e.g., GLSL, HLSL, Artic) and allows easy definition of external functions, variables and constants. Therefore, the syntax is simple and limited to a single statement. To support complex programs you may use a node based system  (as in [Ignis](https://github.com/PearCoding/Ignis)) or your own statement based dsl on top of PExpr.

## Does it optimize?

Optimization, except constant unfolding (to some degree), is not applied and kept as an exercise to the language the expression is transpiled to.


## Dependencies

PExpr has no other dependencies, except a modern C++17 compiler.

## Similar libraries

If you want to embed and directly use PExpr, it is recommended to use [SeExpr](https://github.com/wdas/SeExpr) instead, as it already has many "runners" included, has many standard functions and is already feature proof. On the other hand, PExpr is easier to build and to transpile to other languages than SeExpr.

Keep in mind that this project has no connection to SeExpr, except that it is the reason I started this project. SeExpr is too complex for my other projects (e.g., [Ignis](https://github.com/PearCoding/Ignis)) to embed into. 
