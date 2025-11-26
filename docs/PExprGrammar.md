Overview
--------
This document captures the grammar implemented by the recursive-descent parser in src/internal/Parser.cpp.
Rules are presented in an informal BNF-like notation derived from the parser functions.

Top-level
---------
translation_unit ::= closure EOF

closure
-------
A closure is a list of optional statements followed by a single expression.
It is used for the translation unit and for blocks.

closure ::= { statement* } expression [ ';' (warning) ]

statement
---------
There are two kinds of statements handled inside a closure:
- variable_statement (mutable or immutable)
- function_statement

statement ::= variable_statement | function_statement

variable_statement ::= Identifier [ ':' elementary_type ] '=' expression ';'
                     | 'mut' Identifier [ ':' elementary_type ] '=' expression ';'

function_statement ::= 'fn' Identifier '(' parameter_def_list ')' [ '->' elementary_type ] '=' expression ';'

parameter_def_list (for function declarations)
----------------------------------------------
parameter_def_list ::= /* empty */ 
                     | param ( ',' param )*

param ::= Identifier [ ':' elementary_type ]

Expressions
-----------

expression ::= binary_expression

Binary expressions use precedence climbing. The binary operators and their precedences (higher number => lower binding) are:

Precedence (highest bind = 1) mapping used in parser:
- 1 : Pow (right-associative in parser algorithm)
- 2 : Mul | Div | Mod
- 3 : Plus | Minus
- 4 : Equal | NotEqual | Less | Greater | LessEqual | GreaterEqual
- 5 : And
- 6 : Or

binary_expression ::= (left-associative by precedence via precedence-climbing)
                      unary_expression ( (binary_op unary_expression) ... )

binary_op token set:
  Or, And,
  Equal, NotEqual, Less, Greater, LessEqual, GreaterEqual,
  Plus, Minus,
  Mul, Div, Mod,
  Pow

Unary expressions
-----------------
unary_expression ::= ('+' | '-' | '!') unary_expression
                   | postfix_expression

Postfix & call & access
-----------------------
The parser distinguishes between call expressions and primary expressions and supports "swizzle" access (dot + identifier).

postfix_expression ::=
    call_expression
  | primary_expression

call_expression ::= Identifier '(' [ parameter_list ] ')'

parameter_list ::= expression (',' expression)*

swizzle (access) ::= '.' Identifier

Primary expressions
-------------------
primary_expression ::=
    if_branch
  | '{' closure '}'
  | '(' expression ')'
  | BooleanLiteral
  | NumberLiteral
  | IntegerLiteral
  | StringLiteral
  | Identifier

If expressions / Branch
-----------------------
if_branch ::= 'if' expression '{' closure '}' { 'elif' expression '{' closure '}' }* 'else' '{' closure '}'

Literals and identifiers
------------------------
true, false, NumberLiteral, IntegerLiteral, StringLiteral are atomic literal tokens produced by lexer.
Identifier is a name token.

Elementary types
----------------
elementary_type ::= 'bool'
                  | 'int'
                  | 'num'
                  | 'vec2'
                  | 'vec3'
                  | 'vec4'
                  | 'str'

Additional parser behavior notes
-------------------------------
- The parser uses lookahead of at least one token in places (e.g., detect Identifier followed by Assign to distinguish variable assignment vs. other uses).
- Binary operator precedence is implemented with a precedence-climbing function p_binary_expression(max_prec).
- Unary operators are prefix `+ - !`.
- Call expressions require the callee to be an Identifier immediately followed by '('.
- Closures (block expressions) are delimited by '{' '}' and return an expression as their body, plus optional statements inside.
- Function declarations start with the Function token then Identifier and a parameter list, followed by '=' and an expression and a trailing ';'.
- Variable declarations either start with the Mutable token and then an Identifier or are assignment-style: Identifier '=' expression ';'.
- Trailing semicolons after top-level expression within a closure are accepted but produce a warning.

