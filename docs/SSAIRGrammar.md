SSA IR Grammar
==============

The SSA IR (Static Single Assignment Intermediate Representation) is a low-level intermediate representation used for optimizations. It is serialized in human-readable format by SSASerializer (src/ssa/SSASerializer.cpp).

File Extension
--------------
- SSA IR files: `.pexprir`

Program Structure
-----------------
program ::= { function } { instruction }

function ::= [ attributes ] 'fn' Identifier '(' parameter_list ')' ':' type newline
             { instruction }
             'endfn' newline

attributes ::= '[[' attribute ( ',' attribute )* ']]'
attribute ::= 'extern' | 'pure'

parameter_list ::= /* empty */
                 | value ( ',' value )*

Values
------
value ::= constant ':' type
        | Identifier ':' type

constant ::= BooleanLiteral | IntegerLiteral | NumberLiteral | StringLiteral | '[' constant ( ',' constant )* ']'

type ::= 'bool' | 'int' | 'num' | 'str' | 'vec' Digit+ | '[' type ( ',' type )* ']'

Instructions
------------
instruction ::= label_instruction
              | assign_instruction
              | call_instruction
              | return_instruction
              | branch_instruction
              | goto_instruction
              | phi_instruction

### Label Instruction
label_instruction ::= Identifier ':'

### Assign Instruction
assign_instruction ::= value '=' operator '(' operand_list ')'

operator ::= 'assign' | 'pos' | 'neg' | 'not' | 'add' | 'sub' | 'mul' | 'div' | 'pow' | 'mod'
           | 'and' | 'or' | 'ls' | 'gt' | 'le' | 'ge' | 'eq' | 'neq' | 'cast' | 'access'

operand_list ::= /* empty */
               | value ( ',' value )*

### Call Instruction
call_instruction ::= value '=' 'call' '[' Identifier ']' '(' argument_list ')'

argument_list ::= /* empty */
                | value ( ',' value )*

### Return Instruction
return_instruction ::= 'return' value

### Branch Instruction
branch_instruction ::= 'branch' value '->' Identifier

### Goto Instruction
goto_instruction ::= 'goto' Identifier

### Phi Instruction
phi_instruction ::= value '=' 'phi' '[' condition_list ']' '(' branch_list ')'

condition_list ::= value ( ',' value )*
branch_list ::= value ( ',' value )*

Comments
--------
Line comments start with '//', block comments with '/*' and end with '*/'.

Examples
--------

### Simple assignment
```
%.1:int = add(5:int, 3:int)
```

### Function definition
```
[[extern]] fn _Z5print_Pn(n:num) : void
endfn
```

### Branch and phi
```
branch %.1:bool -> lbl.3
lbl.3:
%.6:int = phi[%.1:bool](a:int, %.3:int)
```

### Call instruction
```
%.16:void = call[_Z5print_Ps]("Hello":str)
```

### External and pure attributes
```
[[extern]] fn readInt() : int
endfn

[[extern, pure]] fn sqrt(x:num) : num
endfn

[[pure]] fn add(x:int, y:int) : int
  %.1:int = add(x:int, y:int)
  return %.1:int
endfn
```

### Tuple constants
```
%.1:[int, int] = assign([1:int, 2:int]:[int, int])
```

### Vector types
```
%.1:vec2 = add([1.0:num, 2.0:num]:vec2, [3.0:num, 4.0:num]:vec2)
```

Operator Mappings
-----------------
- `assign`: Simple assignment (used for tuple construction)
- `pos`, `neg`, `not`: Unary operators
- `add`, `sub`, `mul`, `div`, `pow`, `mod`: Binary arithmetic
- `and`, `or`: Logical operators
- `ls`, `gt`, `le`, `ge`, `eq`, `neq`: Comparison operators
- `cast`: Type cast
- `access`: Tuple/vector element access

Implementation Notes
--------------------
- SSA values are either named (e.g., `%.1:int`) or constants (e.g., `5:int`)
- Phi instructions merge values from different control flow paths
- Labels mark basic block boundaries
- Functions can be external (no body) or internal (with body)
- The `[[pure]]` attribute indicates a function has no side effects