# PExpr

A fairly simple inline expression optimized to be transpiled to other languages.
The intended field of application is computer graphics, hpc and other math frameworks.

## Why?
In contrary to many other expressive languages, this one is solely intended to be transpiled to other languages (e.g., GLSL, HLSL, Artic) and allows easy definition of external functions, variables and constants. Therefore, the syntax is simple and limited to a single statement. To support complex programs you may use a node based system or your own statement based dsl on top of PExpr.

## Does it optimize?
Optimization, except constant unfolding (to some degree), is not applied and kept as an exercise to the language the expression is transpiled to.
