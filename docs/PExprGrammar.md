Overview
--------
This document captures the grammar implemented by the recursive-descent parser in src/internal/Parser.cpp and lexer in src/internal/Lexer.cpp.
Rules are presented in an informal BNF-like notation derived from the parser functions.

Tokens (Lexer Output)
---------------------
TokenType enum defines all possible tokens:
- Literals: NumberLiteral, IntegerLiteral, StringLiteral, BooleanLiteral (true/false)
- Identifiers: Identifier
- Operators: Plus (+), Minus (-), Mul (*), Div (/), Mod (%), Pow (^), Dot (.), 
             And (&&), Or (||), Less (<), Greater (>), LessEqual (<=), GreaterEqual (>=),
             Equal (==), NotEqual (!=), ExclamationMark (!)
- Punctuation: Comma (,), Colon (:), Semicolon (;), Assign (=), ArrowRight (->),
               OpenParentheses ((), ClosedParentheses ()), OpenBraces ({), ClosedBraces (}),
               OpenSquareBracket ([), ClosedSquareBracket (])
- Keywords: If, Elif, Else, As, Let, Mutable, Function (fn), Using
- Predefined type names: bool, int, num, str, vec2, vec3, vec4

Comments: Line comments start with `//`, block comments are `/* ... */`

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
There are four kinds of statements handled inside a closure:
- variable_statement (single identifier)
- destructuring_statement (pattern matching)
- function_statement (extern or intern, with attributes)
- type_alias_statement

statement ::= variable_statement | destructuring_statement | function_statement | type_alias_statement

type_alias_statement ::= 'using' Identifier '=' type ';'

Destructuring Patterns
----------------------
Patterns allow destructuring tuples into multiple variables at once.

destructuring_pattern ::= '[' pattern_element ( ',' pattern_element )* ']'

pattern_element ::= destructuring_pattern | simple_binding

simple_binding ::= [ 'mut' ] Identifier [ ':' type ]

Note: Type annotations and `mut` qualifiers are only allowed in declaration patterns, not assignment patterns.

Destructuring Statements
------------------------
destructuring_statement ::= declaration_destructuring | assignment_destructuring

declaration_destructuring ::= 'let' '*' destructuring_pattern '=' expression ';'

assignment_destructuring ::= '*' destructuring_pattern '=' expression ';'

Note: Destructuring require a `*` prefix to distinguish them from tuple expressions.

Attributes
----------
Attributes appear before statements in double square brackets.

attributes ::= '[[' attribute ( ',' attribute )* ']]'
attribute ::= Identifier [ '=' ( BooleanLiteral | IntegerLiteral | NumberLiteral | StringLiteral ) ]

Common attributes:
- `[[extern]]` or `[[extern=true]]`: marks function as external (no body)
- `[[pure]]` or `[[pure=true]]`: marks function as having no side effects

function_statement ::= [ attributes ] 'fn' Identifier '(' parameter_def_list ')' [ '->' type ] ( '=' expression ';' | ';' )

Note: External functions (with `[[extern]]` attribute) end with `;` instead of `= expression ;`

variable_statement ::= [ attributes ] ( 'let' [ 'mut' ] Identifier [ ':' type ] '=' expression ';'
                                     | Identifier '=' expression ';' )

parameter_def_list (for function declarations)
----------------------------------------------
parameter_def_list ::= /* empty */ 
                     | param ( ',' param )*

param ::= Identifier [ ':' type ]

Expressions
-----------

expression ::= binary_expression

Binary expressions use precedence climbing. The binary operators and their precedences (higher number => lower binding) are:

Precedence (highest bind = 1) mapping used in parser:
- 1 : Pow (^) - right-associative in parser algorithm
- 2 : Mul (*) | Div (/) | Mod (%)
- 3 : Plus (+) | Minus (-)
- 4 : Equal (==) | NotEqual (!=) | Less (<) | Greater (>) | LessEqual (<=) | GreaterEqual (>=)
- 5 : And (&&)
- 6 : Or (||)

binary_expression ::= (left-associative by precedence via precedence-climbing)
                      unary_expression ( (binary_op unary_expression) ... )

Unary expressions
-----------------
unary_expression ::= ('+' | '-' | '!') unary_expression
                   | postfix_expression

Postfix expressions
-------------------
Postfix expressions support chaining (multiple postfix operators can be applied to an expression).

postfix_expression ::= call_expression { postfix_op }*

postfix_op ::= swizzle_op | cast_op | access_op

swizzle_op ::= '.' Identifier  // swizzle: .x, .xy, .xyz, .xyzw, .r, .rg, .rgb, .rgba, etc.
               // Valid characters: x, y, z, w, r, g, b, a
               // Length: 1-4 characters

cast_op ::= 'as' type

access_op ::= '[' IntegerLiteral ']'

Call expressions
----------------
call_expression ::= Identifier '(' [ parameter_list ] ')'
                  | enclosed_expression

parameter_list ::= expression (',' expression)*

enclosed_expression ::= 
    if_expression
  | '{' closure '}'
  | '(' expression ')'
  | '[' tuple_expression ']'
  | primary_expression

if_expression (conditional expression)
--------------------------------------
if_expression ::= 'if' expression '{' closure '}' 
               { 'elif' expression '{' closure '}' }* 
               'else' '{' closure '}'

tuple_expression
----------------
tuple_expression ::= expression ( ',' expression )*

Primary expressions
-------------------
primary_expression ::=
    BooleanLiteral
  | NumberLiteral  
  | IntegerLiteral
  | StringLiteral
  | Identifier

Types
-----
type ::= Identifier | '[' type ( ',' type )+ ']'

Note: Elementary types (bool, int, num, str) and vector types (vec2, vec3, vec4) are predefined type aliases
in the global symbol table. They can be used as identifiers for function names.

Type aliases
------------
type_alias_statement ::= 'using' Identifier '=' type ';'

Examples:
- `using MyInt = int;`
- `using Pair = [num, int];`
- `using Vector2 = vec2;`

The SymbolTable contains default type aliases for: bool, int, num, str, vec2, vec3, vec4.

Additional parser behavior notes
--------------------------------
- The parser uses lookahead of at least one token (2-token buffer) in places:
  - Detect Identifier followed by Assign to distinguish variable assignment vs. other uses
  - Detect Identifier followed by OpenParentheses for call expressions
  - Detect '*' followed by OpenSquareBracket for destructuring
- Binary operator precedence is implemented with a precedence-climbing function p_binary_expression(max_prec)
- Unary operators are prefix `+ - !`
- Call expressions require the callee to be an Identifier immediately followed by '('
- Closures (block expressions) are delimited by '{' '}' and return an expression as their body, plus optional statements inside
- Function declarations:
  - Internal: 'fn name(params) = expression;' or 'fn name(params) -> type = expression;'
  - External: '[[extern]] fn name(params) -> type;' (requires explicit return type)
- Variable declarations:
  - Immutable: 'let name = expression;' or 'let name: type = expression;'
  - Mutable: 'let mut name = expression;' or 'let mut name: type = expression;'
  - Assignment (to previously declared mutable variable): 'name = expression;'
- Destructuring declarations and assignments:
  - Declaration: 'let *[pattern] = expression;' where pattern can include type annotations and 'mut' qualifiers
  - Assignment: '*[pattern] = expression;' where pattern can only contain identifiers
- Trailing semicolons after top-level expression within a closure are accepted but produce a warning
- Comments are stripped by lexer and don't reach the parser

Examples
--------
1. Variable with tuple type:
   `let t: [bool, [int, num]] = [true, [42, 3.14]];`

2. Nested access (chained postfix):
   `t[1][0]`  // accesses index 0 of the tuple at index 1

3. Function with attributes:
   `[[extern]] fn sqrt(x: num) -> num;`
   `[[pure]] fn add(x: int, y: int) -> int = x + y;`

4. Swizzle expressions:
   `v.xy`     // vec2 from first two components of v (assumed vec3 or vec4)
   `v.rgb`    // vec3 from first three components of v (assumed vec4)

5. Explicit cast:
   `x as int` // converts number x to integer

6. Destructuring declarations:
   `let *[a:vec2, b, mut c:num] = [[2,4], true, 2.0];`
   `let *[[r, s], t] = [[7, 8], 9];`

7. Destructuring assignments:
   `let mut x = 1; let mut y = 2; *[x, y] = [3, 4];`
   `let mut a = 1; let mut b = 2; let mut c = 3; *[[a, b], c] = [[10, 20], 30];`

8. Function returning tuple with destructuring:
   `fn foo() -> [int, int] = [5, 6];`
   `let *[p, q] = foo();`