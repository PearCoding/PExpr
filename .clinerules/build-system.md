The build directory is @build/Debug or @build/Release depending on the cmake build type.

The test executable (CText + Catch2) is @build/Debug/bin/pexpr_tests.exe

The compiler for PExpr is @build/Debug/bin/pexprc.exe
The compiler supports reading PExpr source files, SSA IR source files (--input-ssa) and RVM (register virtual machine) (--input-rvm) source files.
It outputs by default the corresponding SSA IR into a file alongside the original file, but use --std-output to directly print the output into the terminal.
In some test cases --emit-ast can be used to output the AST instead. With --emit-rvm a RVM IR output can be produced. Ready to be digested by a virtual machine interpreter.
Note that using --help more information can be found on how to use the compiler.

Files containing PExpr source have the file extension ".pexpr". The grammar for PExpr is explained in @docs/PExprGrammar.md .
Files containing PExpr SSA IR have the file extension ".pexprir". The grammar for SSA IR is explained in @docs/SSAIRGrammar.md .
Files containing PExpr RVM IR have the file extension ".pexprrvm". The grammar for RVM IR is explained in @docs/RVMIRGrammar.md .

On Windows we use CMake and Ninja in VSCode using a Powershell terminal.

The project uses C++20 features.