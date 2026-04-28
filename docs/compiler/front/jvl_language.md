# JVL Language Reference

JVL (JVAV Language) is the high-level frontend language of the JVAV platform. It is a statically typed, imperative language with C-like syntax, featuring a Rust-inspired ownership system called MimiWorld.

## Program Structure

A JVL program consists of zero or more `import` statements followed by top-level declarations:

```jvl
import "std/io.jvl";
import "std/math.jvl";

func add(a: int, b: int): int {
    return a + b;
}

func main(): int {
    println(add(3, 4));
    return 0;
}
```

### Top-level declarations

- `func` — Function declaration
- `var` — Global variable declaration
- `const` — Compile-time constant
- `import` — Module import
- `syscall` — Custom syscall declaration

## Lexical Elements

### Comments

```jvl
// Line comment extends to end of line

/* Block comment
   may span multiple lines */
```

### Identifiers

Identifiers begin with a letter or underscore, followed by letters, digits, or underscores:

```jvl
name
_name
name123
```

### Keywords

The following are reserved keywords:

`func`, `var`, `const`, `if`, `else`, `while`, `for`, `return`, `import`, `syscall`, `mut`

Type keywords: `int`, `char`, `bool`, `void`, `ptr`, `array`

Boolean literals: `true`, `false`

### Integer literals

Integer literals are parsed as 128-bit signed integers:

```jvl
42        // decimal
0x2A      // hexadecimal
0b101010  // binary
```

### Character literals

Character literals represent a single ASCII character, stored as a 128-bit integer:

```jvl
'A'
'\n'
'\t'
'\\'
'\''
```

Escape sequences supported: `\n`, `\t`, `\r`, `\\`, `\'`, `\"`, `\xNN`

### String literals

String literals are stored as contiguous arrays of 128-bit words, one character per word:

```jvl
"Hello, World!"
"Line 1\nLine 2"
```

**Important**: JVL string literals are automatically **null-terminated** by the frontend codegen (`DB "...", 0`). Always pass explicit lengths when using string functions, as syscalls that take lengths do not read null terminators.

## Types

### Primitive types

| Type | Size | Copy semantics | Description |
|------|------|----------------|-------------|
| `int` | 128-bit | Copy | Signed integer (full 128-bit range) |
| `char` | 128-bit | Copy | ASCII character / small integer |
| `bool` | 128-bit | Copy | Boolean (`true` / `false`) |
| `void` | — | — | Function return type only |

### Pointer types

```jvl
ptr<int>    // Pointer to int
ptr<char>   // Pointer to char
```

Pointer types use **Move semantics**. Assigning a pointer transfers ownership. They are 128-bit values containing a memory address.

### Array types

```jvl
array<int>   // Array of ints (alias for ptr<int>)
```

`array<T>` is semantically equivalent to `ptr<T>`. Both use Move semantics.

### Type inference

Local variables support type inference from their initializer:

```jvl
var x: int = 5;    // explicit type
var y = 3;         // inferred as int
var z = true;      // inferred as bool
var c = 'A';       // inferred as char
```

Inference is **not** supported for:
- Function parameters (must be annotated)
- Function return types (must be annotated)
- Empty initializers (`var x;` is invalid)

## Variables and Constants

### Variable declaration

```jvl
var name: type = initializer;
var name = initializer;       // with type inference
```

Variables must be initialized at declaration. There are no uninitialized variables in JVL.

### Constant declaration

```jvl
const NAME = value;
```

Constants are compile-time values. They do not require type annotations and are inlined at compile time.

### Scope

Variables are block-scoped. A block is any region enclosed in braces `{}`.

```jvl
func main(): int {
    var x = 10;        // x is valid here
    {
        var y = 20;    // y is valid only in this block
    }
    // y is not valid here
    return 0;
}
```

## Operators

### Precedence (highest to lowest)

| Precedence | Operators | Associativity |
|------------|-----------|---------------|
| 1 (highest) | `-x`, `!x`, `~x`, `&x`, `&mut x` | Right |
| 2 | `*`, `/`, `%` | Left |
| 3 | `+`, `-` | Left |
| 4 | `<<`, `>>` | Left |
| 5 | `<`, `>`, `<=`, `>=` | Left |
| 6 | `==`, `!=` | Left |
| 7 | `&` (bitwise) | Left |
| 8 | `^` | Left |
| 9 | `\|` | Left |
| 10 | `&&` | Left |
| 11 | `\|\|` | Left |
| 12 (lowest) | `=` | Right |

### Arithmetic operators

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition | `a + b` |
| `-` | Subtraction | `a - b` |
| `*` | Multiplication | `a * b` |
| `/` | Division (signed) | `a / b` |
| `%` | Modulo (signed) | `a % b` |
| `-x` | Negation | `-a` |

### Bitwise operators

| Operator | Description | Example |
|----------|-------------|---------|
| `&` | Bitwise AND | `a & b` |
| `\|` | Bitwise OR | `a \| b` |
| `^` | Bitwise XOR | `a ^ b` |
| `~` | Bitwise NOT | `~a` |
| `<<` | Shift left | `a << b` |
| `>>` | Shift right (arithmetic) | `a >> b` |

### Comparison operators

| Operator | Description | Example |
|----------|-------------|---------|
| `==` | Equal | `a == b` |
| `!=` | Not equal | `a != b` |
| `<` | Less than | `a < b` |
| `>` | Greater than | `a > b` |
| `<=` | Less than or equal | `a <= b` |
| `>=` | Greater than or equal | `a >= b` |

### Logical operators

| Operator | Description | Example |
|----------|-------------|---------|
| `&&` | Logical AND | `a && b` |
| `\|\|` | Logical OR | `a \|\| b` |
| `!` | Logical NOT | `!a` |

**Note**: JVL implements short-circuit evaluation for `&&` and `||`. The right operand is only evaluated if the left operand does not determine the result.

### Assignment

```jvl
x = expression;
```

Assignment is an expression (returns the assigned value) but is only permitted as a statement-level expression or within `for` loop steps.

### Borrow operators

| Operator | Description | Example |
|----------|-------------|---------|
| `&x` | Immutable borrow | `func(&x)` |
| `&mut x` | Mutable borrow | `func(&mut x)` |

Borrow expressions create references without transferring ownership. See the [Ownership](ownership.md) document for full details.

## Control Flow

### If statement

```jvl
if (condition) {
    // statements
} else if (another_condition) {
    // statements
} else {
    // statements
}
```

The `else if` and `else` branches are optional. Conditions must be of type `bool` or a type convertible to `bool`.

### While loop

```jvl
while (condition) {
    // statements
}
```

Executes the body repeatedly while the condition is true. The condition is checked before each iteration.

### For loop

```jvl
for (init; condition; step) {
    // statements
}
```

The `for` loop consists of three parts separated by semicolons:

1. **Init**: Executed once before the loop (typically a variable declaration)
2. **Condition**: Checked before each iteration
3. **Step**: Executed after each iteration (typically an assignment)

All three parts are optional. `for (;;) { ... }` creates an infinite loop.

**Example**:
```jvl
for (var i = 0; i < 10; i = i + 1) {
    putint(i);
}
```

### Return statement

```jvl
return expression;
```

Returns a value from a function. In `void` functions, `return` may be used without an expression.

All non-void functions must return a value on all control paths. The compiler checks this statically.

## Functions

### Function declaration

```jvl
func name(param1: type1, param2: type2): return_type {
    // body
    return value;
}
```

Functions with `void` return type omit the return value:

```jvl
func greet(): void {
    putstr("Hello", 5);
}
```

### Parameters

Parameters must have explicit type annotations:

```jvl
func add(a: int, b: int): int {
    return a + b;
}
```

Arguments are passed by value. For Copy types (`int`, `char`, `bool`), this copies the value. For Move types (`ptr<T>`), this transfers ownership.

### Recursion

JVL supports recursion. Each recursive call creates a new stack frame:

```jvl
func factorial(n: int): int {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}
```

## Pointers and Arrays

### Pointer declaration

```jvl
var p: ptr<int> = alloc(3);
```

### Array indexing

```jvl
p[0] = 10;
p[1] = 20;
p[2] = 30;
putint(p[0] + p[1] + p[2]);   // 60
```

Pointer arithmetic is **not** supported. Use array indexing `p[i]` to access elements.

### Heap allocation

```jvl
var p: ptr<int> = alloc(n);   // allocate n words
// use p
free(p);                        // release ownership
```

The `alloc()` builtin allocates `n` 128-bit words on the heap. The `free()` builtin releases ownership (writes a tombstone in the VM).

### Dereferencing

Array indexing `p[0]` is the primary dereferencing mechanism. There is no unary `*` dereference operator.

## Custom Syscalls

JVL supports declaring custom syscalls directly in source code:

```jvl
syscall name, cmd_id, arg_count;
```

**Constraints**:
- `arg_count` must be in range `0..3`
- The declared name becomes a builtin function in the module
- Return type is always `int`
- The compiler emits `.syscall name, cmd_id, arg_count` in the output assembly

**Example**:
```jvl
syscall my_custom, 99, 2;

func main(): int {
    var result = my_custom(255, 1);
    return 0;
}
```

## Type Conversions

### Weak numeric coercion

JVL allows implicit conversion between numeric types:

| From | To | Allowed? |
|------|-----|----------|
| `int` | `int` | Yes |
| `char` | `int` | Yes (weak coercion) |
| `bool` | `int` | Yes (weak coercion) |
| `int` (literal) | `ptr<T>` | Yes (for literal addresses only) |
| `ptr<T>` | `ptr<U>` (T≠U) | No |
| `int` (variable) | `ptr<T>` | No |

### Literal address conversion

Integer literals can be implicitly converted to pointer types for accessing memory-mapped I/O:

```jvl
func write_mailbox(addr: int, val: int): void {
    var p: ptr<int> = addr;   // implicit conversion
    p[0] = val;
}
```

This only works for literal integer values, not variables.

## Complete Example

```jvl
import "std/io.jvl";
import "std/math.jvl";

func factorial(n: int): int {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

func main(): int {
    println(factorial(5));        // 120
    
    var msg = "JVAV";
    putstr(msg, 4);               // JVAV
    print_newline();
    
    exit(0);
    return 0;
}
```
