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
- `const` — Compile-time constant (exported to assembly as `.equ`)
- `import` — Module import
- `syscall` — Custom syscall declaration
- `struct` — Structure type definition
- `union` — Union type definition
- `enum` — Enumeration type definition
- `typedef` — Type alias definition

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

`func`, `var`, `const`, `if`, `else`, `while`, `for`, `return`, `import`, `syscall`, `mut`, `asm`

Type keywords: `int`, `char`, `bool`, `void`, `ptr`, `array`, `byte`, `uint`, `struct`, `union`, `enum`

Other keywords: `typedef`, `do`, `while`, `for`, `if`, `else`, `return`, `break`, `continue`, `switch`, `case`, `default`

Operators: `sizeof`, `offsetof`, `volatile`

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
| `uint` | 128-bit | Copy | Unsigned integer (same size, semantic difference) |
| `char` | 128-bit | Copy | ASCII character / small integer |
| `byte` | 128-bit | Copy | Byte value (same layout as char) |
| `bool` | 128-bit | Copy | Boolean (`true` / `false`) |
| `void` | — | — | Function return type only |

### Pointer types

```jvl
ptr<int>    // Pointer to int
ptr<char>   // Pointer to char
ptr<byte>   // Pointer to byte
ptr<Struct> // Pointer to struct
```

Pointer types use **Move semantics**. Assigning a pointer transfers ownership. They are 128-bit values containing a memory address.

### Array types

```jvl
array<int>         // Dynamic array (alias for ptr<int>)
int[8]             // Fixed-size array of 8 ints
int[256][512]      // Multidimensional array: 256 arrays of 512 ints
```

`array<T>` is semantically equivalent to `ptr<T>`. Both use Move semantics.

Fixed-size array syntax `T[N]` creates an array type with `N` elements. Multidimensional arrays follow C semantics: `T[a][b]` is an array of `a` elements, each being an array of `b` elements.

**Note**: Since the JVAV VM uses 128-bit words, `byte` and `char` each occupy one full word. `ptr<byte>` provides logical byte addressing where each byte is stored in its own word.

### Struct types

```jvl
struct Point {
    x: int;
    y: int;
}

struct Rect {
    top_left: Point;
    bottom_right: Point;
}
```

Struct fields are laid out sequentially in memory. Each field's offset is the sum of the sizes of all preceding fields. Structs are accessed through pointers:

```jvl
var p: ptr<Point> = alloc(sizeof(Point));
p->x = 3;
p->y = 4;
```

### Union types

```jvl
union Value {
    i: int;
    c: char;
    data: int[4];
}
```

All union fields start at offset 0. The size of a union is the maximum size of any field.

### Enum types

```jvl
enum Color {
    RED,
    GREEN = 5,
    BLUE
}
```

Enum members are compile-time constants. By default, the first member has value `0`, and each subsequent member increments by `1`. Explicit values can be assigned with `=`. Enums are emitted as `.equ` directives in the generated assembly.

Members can be used directly by name without qualification:

```jvl
var c = RED;     // c = 0
var g = GREEN;   // g = 5
```

### Typedef

```jvl
typedef int MyInt;
typedef ptr<int> IntPtr;
typedef Point PointAlias;
```

`typedef` creates an alias for an existing type. The alias can be used anywhere the original type is expected. Typedefs are resolved at compile time and do not introduce new types.

### Type modifiers

#### volatile

The `volatile` modifier indicates that a variable may be modified by external factors (e.g., memory-mapped I/O):

```jvl
var mailbox: volatile ptr<int> = 0xFFE0;
var val = mailbox[0];
```

`volatile` is currently a semantic hint; it does not change code generation.

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
const SIZE = sizeof(int[8]);
```

Constants are compile-time values. They do not require type annotations. Global constants are exported to assembly as `.equ` directives, making them available to other assembly files and the linker.

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
| 1 (highest) | `-x`, `!x`, `~x`, `&x`, `&mut x`, `sizeof(x)`, `offsetof(T, f)`, `(T)x` | Right |
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
| 12 | `?:` (ternary) | Right |
| 13 (lowest) | `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=` | Right |

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

**Note**: JVL implements short-circuit evaluation for `&&` and `\|\|`. The right operand is only evaluated if the left operand does not determine the result.

### Assignment

```jvl
x = expression;
```

Assignment is an expression (returns the assigned value) but is only permitted as a statement-level expression or within `for` loop steps.

### Compound assignment

```jvl
x += y;   // x = x + y
x -= y;   // x = x - y
x *= y;   // x = x * y
x /= y;   // x = x / y
x %= y;   // x = x % y
x &= y;   // x = x & y
x |= y;   // x = x | y
x ^= y;   // x = x ^ y
x <<= y;  // x = x << y
x >>= y;  // x = x >> y
```

Compound assignment operators combine an arithmetic/bitwise operation with assignment. They are desugared to `x = x op y` during compilation.

### Increment and decrement

```jvl
++x;   // prefix increment: x = x + 1
--x;   // prefix decrement: x = x - 1
```

Only prefix forms (`++x`, `--x`) are supported. Postfix forms (`x++`, `x--`) are not implemented.

### Ternary operator

```jvl
var result = condition ? value_if_true : value_if_false;
```

The ternary operator evaluates `condition` and returns `value_if_true` if non-zero, otherwise `value_if_false`. Both branches must be expressions (not statements).

### sizeof operator

```jvl
sizeof(int)           // 1 (word)
sizeof(int[8])        // 8 (words)
sizeof(MyStruct)      // struct size in words
sizeof(expression)    // size of expression's type
```

`sizeof` returns the size of a type or expression in 128-bit words.

### offsetof operator

```jvl
offsetof(Point, x)    // offset of field x in Point (words)
offsetof(MyStruct, field)
```

`offsetof` returns the offset of a field within a struct or union, measured in 128-bit words.

### Type casting

```jvl
var p: ptr<int> = (ptr<int>)0x1000;
var c = (char)65;
```

C-style casts `(Type)expr` are supported. Since JVAV is weakly typed at the machine level, most casts are no-ops in the generated code but provide type safety in the frontend.

### Field access

```jvl
p->field      // Access field via pointer
expr.field    // Access field directly (if expr is struct-typed)
```

The `->` operator accesses a field through a pointer. The `.` operator accesses a field on a struct value. In practice, structs are usually accessed through pointers.

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

### Do-while loop

```jvl
do {
    // statements
} while (condition);
```

Executes the body at least once, then repeats while the condition is true. The condition is checked after each iteration.

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

### Switch statement

```jvl
switch (expression) {
    case 1: {
        // statements
    }
    case 2: {
        // statements
    }
    default: {
        // statements
    }
}
```

The `switch` statement evaluates the expression and jumps to the matching `case`. Cases **fall through** to the next case (no automatic break). The `default` case is optional and matches any value not handled by other cases.

**Note**: For return-path analysis, a `switch` without a `default` case is considered to **not** cover all control paths. If a function with a non-void return type contains a `switch` without `default`, the compiler will report a missing return statement unless every case body returns a value and a `default` case is present.

### Break and Continue

```jvl
while (true) {
    if (condition) {
        break;      // exit the loop
    }
    if (other) {
        continue;   // skip to next iteration
    }
}
```

`break` exits the innermost enclosing `while`, `for`, `do-while` loop, or `switch` statement. `continue` skips the rest of the current iteration and jumps to the loop condition.

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

Arguments are passed by value. For Copy types (`int`, `char`, `bool`, `byte`, `uint`), this copies the value. For Move types (`ptr<T>`), this transfers ownership.

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

## Inline Assembly

Inline assembly allows embedding raw JVAV assembly instructions directly in JVL code:

```jvl
asm {
    "LDI R0, 0xFFE0"
    "LDR R1, [R0]"
};
```

Each string literal in the asm block contains one assembly instruction. Instructions are emitted verbatim into the generated assembly file. A semicolon after each instruction is optional.

**Use cases**:
- Memory-mapped I/O access
- Direct register manipulation
- Performance-critical code sequences
- Syscall mailbox operations

**Example**: Reading from a mailbox port
```jvl
func read_mailbox(): int {
    asm {
        "LDI R0, 0xFFE4"
        "LDR R0, [R0]"
    };
    return 0;  // R0 already contains the value
}
```

**Warning**: Inline assembly bypasses the compiler's register allocation and type checking. Incorrect usage can corrupt the stack, overwrite callee-saved registers, or cause undefined behavior.

## Pointers and Arrays

### Pointer declaration

```jvl
var p: ptr<int> = alloc(3);
var b: ptr<byte> = alloc(64);
```

### Array indexing

```jvl
p[0] = 10;
p[1] = 20;
p[2] = 30;
putint(p[0] + p[1] + p[2]);   // 60
```

Pointer arithmetic is **not** supported. Use array indexing `p[i]` to access elements.

For arrays of structs, indexing scales by the struct size automatically:

```jvl
var items: ptr<Item> = alloc(10 * sizeof(Item));
items[i]->field = value;
```

### Array literals

Array literals create a contiguous array on the stack:

```jvl
var arr = {1, 2, 3};
putint(arr[0]);  // 1
putint(arr[1]);  // 2
putint(arr[2]);  // 3
```

Array literals can be assigned to `ptr<T>` or `array<T>` variables. The element count is inferred from the literal.

### Struct literals

Struct literals initialize a struct with named fields:

```jvl
struct Point {
    x: int;
    y: int;
}

var p: Point = { x: 10, y: 20 };
putint(p.x);  // 10
putint(p.y);  // 20
```

Field names are required. The struct type must be explicitly annotated on the variable.

### Heap allocation

```jvl
var p: ptr<int> = alloc(n);   // allocate n words
// use p
free(p);                        // release ownership
```

The `alloc()` builtin allocates `n` 128-bit words on the heap. The `free()` builtin releases ownership (writes a tombstone in the VM).

### Built-in Functions

The following functions are pre-declared in every module:

| Function | Signature | Description |
|----------|-----------|-------------|
| `putint` | `func putint(x: int): int` | Print integer to stdout |
| `putchar` | `func putchar(c: int): void` | Print ASCII character to stdout |
| `getchar` | `func getchar(): int` | Read one character from stdin |
| `getint` | `func getint(): int` | Read one integer from stdin |
| `alloc` | `func alloc(n: int): ptr<int>` | Allocate `n` words on heap |
| `free` | `func free(p: ptr<int>): void` | Release heap allocation |
| `exit` | `func exit(code: int): void` | Terminate program |
| `putstr` | `func putstr(s: ptr<int>, len: int): void` | Print string (length required) |
| `sleep` | `func sleep(ms: int): void` | Sleep for milliseconds |

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
| `int` | `int`, `uint`, `char`, `byte`, `bool` | Yes (weak coercion) |
| `uint` | `int`, `uint`, `char`, `byte`, `bool` | Yes (weak coercion) |
| `char` | `int`, `uint`, `char`, `byte`, `bool` | Yes (weak coercion) |
| `byte` | `int`, `uint`, `char`, `byte`, `bool` | Yes (weak coercion) |
| `bool` | `int`, `uint`, `char`, `byte`, `bool` | Yes (weak coercion) |
| `int` (literal) | `ptr<T>` | Yes (for literal addresses only) |
| `ptr<T>` | `ptr<T>` | Yes |
| `ptr<T>` | `ptr<U>` (T≠U) | No (use explicit cast) |
| `ptr<T>` | `array<T>` | Yes |
| `array<T>` | `ptr<T>` | Yes |
| `array<T>` | `array<U>` (T≠U) | No |
| `int` (variable) | `ptr<T>` | No |

### Explicit casting

```jvl
var p: ptr<int> = (ptr<int>)0x1000;
var u: uint = (uint)-1;
var b: byte = (byte)255;
```

### Literal address conversion

Integer literals can be implicitly converted to pointer types for accessing memory-mapped I/O:

```jvl
func write_mailbox(val: int): void {
    var p: ptr<int> = 0xFFE0;   // implicit conversion from literal
    p[0] = val;
}
```

This only works for literal integer values, not variables.

## Complete Example

```jvl
import "std/io.jvl";
import "std/math.jvl";

struct Point {
    x: int;
    y: int;
}

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
    
    var p: ptr<Point> = alloc(sizeof(Point));
    p->x = 3;
    p->y = 4;
    putint(p->x + p->y);          // 7
    
    exit(0);
    return 0;
}
```

## New Features Summary

### Struct and Union Types
- Define composite types with `struct Name { field: Type; }`
- Define overlapping types with `union Name { field: Type; }`
- Access fields through pointers with `ptr->field`

### sizeof and offsetof
- `sizeof(Type)` returns size in words
- `offsetof(Type, field)` returns field offset in words

### byte and uint Types
- `byte`: 128-bit byte value (logical byte access)
- `uint`: 128-bit unsigned integer

### Multidimensional Arrays
- `int[8]`: fixed-size array
- `int[256][512]`: multidimensional array with C semantics

### Type Casting
- C-style syntax: `(Type)expression`

### volatile Modifier
- `volatile ptr<Type>` for memory-mapped I/O

### Optional trailing semicolons

Struct, union, enum, and asm blocks may optionally have a trailing semicolon:

```jvl
struct Point {
    x: int;
    y: int;
};   // semicolon is optional

enum Color {
    RED, GREEN, BLUE
};   // semicolon is optional

asm {
    "LDI R0, 0"
};   // semicolon is optional
```

This is a convenience feature for C/C++ compatibility. Both forms are accepted.

### Inline Assembly

Inline assembly allows embedding raw JVAV assembly instructions directly in JVL code:

```jvl
asm {
    "LDI R0, 0xFFE0"
    "LDR R1, [R0]"
};
```

Object macros replace every occurrence of the name with the replacement text.

### Function Macros

```jvl
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SQUARE(x) ((x) * (x))
```

Function macros accept comma-separated parameters and expand at each use site.

### Conditional Compilation

```jvl
#define DEBUG

#ifdef DEBUG
    #define LOG_LEVEL 3
#else
    #define LOG_LEVEL 1
#endif

#if defined(FEATURE_A) && defined(FEATURE_B)
    #define MODE 1
#elif defined(FEATURE_A)
    #define MODE 2
#else
    #define MODE 0
#endif
```

Supported directives:

| Directive | Description |
|-----------|-------------|
| `#define NAME value` | Define object macro |
| `#define NAME(a,b) body` | Define function macro |
| `#undef NAME` | Remove macro definition |
| `#ifdef NAME` | True if macro is defined |
| `#ifndef NAME` | True if macro is not defined |
| `#if expr` | Evaluate constant expression |
| `#elif expr` | Else-if branch |
| `#else` | Else branch |
| `#endif` | End conditional block |
| `#error message` | Emit compilation error |

### Expression Evaluation

`#if` expressions support:
- `defined(NAME)` — 1 if macro exists, 0 otherwise
- Integer literals and macro names
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`

```jvl
#if (VERSION_MAJOR > 1) || (VERSION_MAJOR == 1 && VERSION_MINOR >= 5)
    #define API_LEVEL 2
#endif
```

### Macro Expansion Rules

- Expansion is recursive; macros in the replacement text are expanded
- Self-referential macros are protected from infinite recursion
- String literals are never expanded
- Only tokens matching complete macro names are replaced

```jvl
#define REC 1 + REC
putint(REC);   // expands to: putint(1 + REC)
```

### Preprocessor Scope

- Macros are global within a single compilation unit
- `#undef` removes a definition for subsequent lines
- Conditional compilation can nest arbitrarily deep
- In disabled blocks, only `#if`/`#ifdef`/`#ifndef`/`#elif`/`#else`/`#endif` are processed; `#define` and `#undef` are ignored
