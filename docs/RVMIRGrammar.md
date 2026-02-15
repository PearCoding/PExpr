RVM IR Grammar
==============

The RVM IR (Register Virtual Machine Intermediate Representation) is a low-level representation used for register allocation and code generation. It is serialized in human-readable format by RVMSerializer (src/rvm/RVMSerializer.cpp).

File Extension
--------------
- RVM IR files: `.pexprrvm`

Program Structure
-----------------
program ::= { instruction }

Values
------
value ::= constant ':' type
        | register ':' type
        | string_ref ':' type

constant ::= BooleanLiteral | IntegerLiteral | NumberLiteral | StringLiteral
register ::= '%r' Digit+
string_ref ::= '#str' Digit+

type ::= 'bool' | 'int' | 'num' | 'str'

Instructions
------------
instruction ::= label_instruction
              | arithmetic_instruction
              | comparison_instruction
              | conversion_instruction
              | move_instruction
              | control_flow_instruction
              | call_instruction
              | return_instruction
              | string_literal_instruction
              | comment_instruction

### Label Instruction
label_instruction ::= Identifier ':'

### Arithmetic Instructions
arithmetic_instruction ::= arithmetic_op register value [ value ]

arithmetic_op ::= 'add' | 'sub' | 'mul' | 'div' | 'mod' | 'pow'
                | 'and' | 'or' | 'xor' | 'shl' | 'shr'

### Comparison Instructions
comparison_instruction ::= comparison_op register value value

comparison_op ::= 'cmp_eq' | 'cmp_ne' | 'cmp_lt' | 'cmp_le' | 'cmp_gt' | 'cmp_ge'

### Conversion Instructions
conversion_instruction ::= conversion_op register value

conversion_op ::= 'i2f' | 'f2i'

### Move Instruction
move_instruction ::= 'mov' register value

### Control Flow Instructions
control_flow_instruction ::= branch_instruction | jump_instruction

branch_instruction ::= branch_op Identifier value

branch_op ::= 'jz' | 'jnz'

jump_instruction ::= 'jmp' Identifier

### Call Instruction
call_instruction ::= call_type Digit+ Digit+ Identifier

call_type ::= 'call_internal' | 'call_external'

### Return Instruction
return_instruction ::= 'ret' Digit+

### String Literal Instruction
string_literal_instruction ::= 'load_string' register StringLiteral

### Comment Instruction
comment_instruction ::= '//' .*

Instruction Formats
-------------------
- 2-operand: `op dst src` (e.g., `mov %r1:int %r0:int`, `i2f %r2:num %r1:int`)
- 3-operand: `op dst src1 src2` (e.g., `add %r0:int %r1:int %r2:int`)
- Branch: `jz label value` or `jnz label value`
- Jump: `jmp label`
- Call: `call_internal param_count return_count func_name` or `call_external ...`
- Return: `ret return_count`
- String literal: `load_string dst "string"`
- Label: `label_name:`
- Comment: `// comment text`

Comments
--------
Line comments start with '//'.

Examples
--------

### Arithmetic operations
```
add %r0:int %r1:int %r2:int
mul %r3:num %r4:num %r5:num
sub %r6:int %r7:int %r8:int
div %r9:num %r10:num %r11:num
```

### Control flow
```
jz lbl1 %r0:bool
jmp lbl2
lbl1:
jnz lbl3 %r1:bool
lbl2:
lbl3:
```

### Function call
```
call_internal 2 1 _Z3gcd_Pii
call_external 1 1 _Z5print_Ps
```

### String literal
```
load_string %r0:str "Hello, world!"
```

### Move chain
```
mov %r1:int %r0:int
mov %r2:int %r1:int
mov %r3:int %r2:int
```

### Constants and registers
```
mov %r0:int 42:int
add %r1:int %r0:int 10:int
mov %r2:bool true:bool
```

### Conversion operations
```
i2f %r3:num %r2:int
f2i %r4:int %r3:num
```

### Bit operations
```
and %r5:int %r6:int %r7:int
or %r8:int %r9:int %r10:int
shl %r11:int %r12:int 2:int
```

Opcode Reference
----------------
- **Arithmetic**: `add`, `sub`, `mul`, `div`, `mod`, `pow`
- **Bitwise**: `and`, `or`, `xor`, `shl`, `shr`
- **Comparison**: `cmp_eq`, `cmp_ne`, `cmp_lt`, `cmp_le`, `cmp_gt`, `cmp_ge`
- **Conversion**: `i2f`, `f2i`
- **Move**: `mov`
- **Control flow**: `jz`, `jnz`, `jmp`
- **Function calls**: `call_internal`, `call_external`, `ret`
- **String literals**: `load_string`

Implementation Notes
--------------------
- Registers are denoted as `%rN` where N is a decimal number (e.g., `%r0`, `%r42`)
- String references are denoted as `#strN` where N is a decimal number
- Constants include type suffixes (e.g., `42:int`, `3.14:num`, `true:bool`)
- RVM IR does not support tuples or vectors - these are dissolved in earlier passes
- The first number after call instructions is parameter count, second is return count
- Branch instructions test a value and jump to a label if condition is met
- Comments are preserved in the serialized output but are lost when deserialized