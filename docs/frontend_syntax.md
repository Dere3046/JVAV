# JVL Frontend Language Syntax

JVL (JVAV Language) is the high-level frontend language of the JVAV platform. It compiles to JVAV assembly (`.jvav`) via the `jvlc` compiler.

## Basic Types

| Type | Size | Copy? | Description |
|------|------|-------|-------------|
| `int` | 128-bit | Yes | Signed integer (arbitrary precision within 128 bits) |
| `char` | 128-bit | Yes | Character (word-sized, UTF-8 code unit) |
| `bool` | 128-bit | Yes | Boolean (`true` / `false`) |
| `void` | — | — | No return value; cannot be used as a variable type |
| `ptr<T>` | 128-bit | **No** | Pointer to a value of type `T` |
| `array<T>` | 128-bit | **No** | Dynamic array of elements of type `T` (currently alias for `ptr<T>`) |

### Type Derivation and Inference

JVL supports **local type inference** for variable declarations:

```jvl
var x: int = 5;       // explicit type annotation
var y = 3;            // inferred as `int` from the initializer
var z = true;         // inferred as `bool`
```

### String Literals

String literals support C-style escape sequences:

| Escape | Meaning |
|--------|---------|
| `\n` | newline |
| `\t` | tab |
| `\r` | carriage return |
| `\\` | backslash |
| `\"` | double quote |
| `\xNN` | byte with hex value `NN` |

Inference rules:
- Integer literals → `int`
- Boolean literals → `bool`
- Character literals → `char`
- `alloc(n)` → `ptr<int>`

Type inference is **not** supported for:
- Function parameters (must be annotated)
- Function return types (must be annotated)
- Empty initializers (`var x;` is invalid)

### Type Compatibility

JVL uses **nominal typing** with strict compatibility rules:

| Context | Allowed |
|---------|---------|
| `int` ← `int` | ✓ |
| `int` ← `bool` | ✗ (no implicit conversion) |
| `int` ← `char` | ✗ (no implicit conversion) |
| `ptr<T>` ← `ptr<T>` | ✓ |
| `ptr<T>` ← `ptr<U>` where T ≠ U | ✗ |
| `array<T>` ← `ptr<T>` | ✓ (alias) |

There are **no implicit conversions** between types. Use explicit casts via built-in functions if needed.

### `void` Restrictions

`void` is only valid as a **function return type**:

```jvl
func foo(): void {          // OK
    putint(1);
}

// The following are INVALID:
// var x: void;             // ERROR: void cannot be used as a variable type
// func bar(x: void): int;  // ERROR: void parameter
// ptr<void>                // ERROR: void pointee
```

### Pointer Types `ptr<T>`

A pointer stores a memory address (128-bit word address in the JVAV VM):

```jvl
var p: ptr<int> = alloc(3);     // p points to 3 words of heap memory
p[0] = 10;                       // dereference and store
p[1] = 20;
putint(p[0] + p[1]);             // prints 30
free(p);                         // release ownership
```

Pointer arithmetic is **not supported** directly; use array indexing `p[i]`.

### Array Types `array<T>`

`array<T>` is currently an alias for `ptr<T>` in the JVAV toolchain:

```jvl
var arr: array<int> = alloc(5);  // same as ptr<int>
arr[0] = 1;
arr[1] = 2;
```

Future versions may add length tracking and bounds checking.

## Variables & Constants

```jvl
var x: int = 5;       // mutable variable
var y = 3;            // type inferred as int
const MAX = 100;      // compile-time constant (inferred type, immutable)
```

All variables must be initialized at declaration. Uninitialized variables are rejected by the semantic analyzer.

## Functions

```jvl
func add(a: int, b: int): int {
    return a + b;
}

func main(): int {
    return add(3, 5);
}
```

Function parameters are passed by value:
- **Copy types** (`int`, `char`, `bool`): the value is copied
- **Move types** (`ptr<T>`, `array<T>`): ownership is transferred to the callee

## Control Flow

```jvl
if (x > 0) {
    putint(x);
} else {
    putint(-x);
}

while (i < 10) {
    i = i + 1;
}

for (var i = 0; i < 10; i = i + 1) {
    putint(i);
}
```

## Operators

| Precedence | Operators | Associativity |
|------------|-----------|---------------|
| 1 (lowest) | `=` | Right |
| 2 | `\|\|` | Left |
| 3 | `&&` | Left |
| 4 | `\|`, `^`, `&` | Left | bitwise OR, XOR, AND |
| 5 | `==`, `!=` | Left | equality |
| 6 | `<`, `>`, `<=`, `>=` | Left | comparison |
| 7 | `<<`, `>>` | Left | shift left, shift right |
| 8 | `+`, `-` | Left | addition, subtraction |
| 9 | `*`, `/`, `%` | Left | multiplication, division, modulo |
| 10 (highest) | unary `-`, `!`, `~` | Right | negation, logical NOT, bitwise NOT |
| borrow | `&expr`, `&mut expr` | — | immutable / mutable borrow (not an arithmetic operator) |

## Import Modules

```jvl
import "math.jvl";

func main(): int {
    return factorial(5);
}
```

- Relative paths are resolved from the source file's directory
- The `std/` directory at the project root is automatically searched; `import "std/io.jvl";` works from any source file
- Cyclic imports are silently deduplicated

## Built-in Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `putint(x)` | `func putint(x: int): int` | Print integer to stdout |
| `putchar(c)` | `func putchar(c: int): void` | Print character to stdout |
| `alloc(n)` | `func alloc(size: int): ptr<int>` | Allocate `size` words on the heap |
| `free(p)` | `func free(p: ptr<int>): void` | Release heap allocation (consumes ownership) |

## Standard Library

The JVAV standard library resides in the `std/` directory at the project root. Import via `import "std/xxx.jvl";` — jvlc automatically resolves the `std/` path relative to the project root.

### `std/io.jvl`

| Function | Signature | Description |
|----------|-----------|-------------|
| `println(x)` | `func println(x: int): void` | Print integer followed by newline |
| `print_newline()` | `func print_newline(): void` | Print newline (`\n`) |
| `print_space()` | `func print_space(): void` | Print space (` `) |

### `std/math.jvl`

| Function | Signature | Description |
|----------|-----------|-------------|
| `abs(x)` | `func abs(x: int): int` | Absolute value |
| `max(a, b)` | `func max(a: int, b: int): int` | Maximum of two integers |
| `min(a, b)` | `func min(a: int, b: int): int` | Minimum of two integers |
| `clamp(x, lo, hi)` | `func clamp(x: int, lo: int, hi: int): int` | Clamp to range `[lo, hi]` |
| `pow(base, exp)` | `func pow(base: int, exp: int): int` | Integer power |

### `std/mem.jvl`

| Function | Signature | Description |
|----------|-----------|-------------|
| `memcpy(dst, src, n)` | `func memcpy(dst: ptr<int>, src: ptr<int>, n: int): void` | Copy `n` words from `src` to `dst` |
| `memset(p, val, n)` | `func memset(p: ptr<int>, val: int, n: int): void` | Set `n` words starting at `p` to `val` |

### `std/string.jvl`

| Function | Signature | Description |
|----------|-----------|-------------|
| `str_putn(s, n)` | `func str_putn(s: ptr<char>, n: int): void` | Print first `n` characters of string `s` |

---

## MimiWorld Ownership in JVL

JVL features a **MimiWorld** ownership system inspired by Rust. See [mimiworld_ownership.md](mimiworld_ownership.md) for full details.

Quick reference:

```jvl
var p: ptr<int> = alloc_int();   // p owns the allocation
var q = p;                        // ERROR: p has been moved

var r = &p;                       // OK: immutable borrow
var s = &mut p;                   // OK: mutable borrow (exclusive)
var t = &p;                       // ERROR: p is mutably borrowed
```
