# JVL Frontend Language Syntax

JVL (JVAV Language) is the high-level frontend language of the JVAV platform. It compiles to JVAV assembly (`.jvav`) via the `jvlc` compiler.

## Basic Types

| Type | Size | Copy? | Description |
|------|------|-------|-------------|
| `int` | 128-bit | Yes | Signed integer |
| `char` | 128-bit | Yes | Character (word-sized) |
| `bool` | 128-bit | Yes | Boolean |
| `void` | — | — | No return value |
| `ptr<T>` | 128-bit | **No** | Pointer to T |
| `array<T>` | 128-bit | **No** | Array of T |

## Variables & Constants

```jvl
var x: int = 5;       // mutable variable
var y = 3;            // type inferred as int
const MAX = 100;      // compile-time constant
```

## Functions

```jvl
func add(a: int, b: int): int {
    return a + b;
}

func main(): int {
    return add(3, 5);
}
```

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

| Precedence | Operators |
|------------|-----------|
| 1 (lowest) | `=` |
| 2 | `\|\|` |
| 3 | `&&` |
| 4 | `\|`, `^`, `&` |
| 5 | `==`, `!=` |
| 6 | `<`, `>`, `<=`, `>=` |
| 7 | `<<`, `>>` |
| 8 | `+`, `-` |
| 9 | `*`, `/`, `%` |
| 10 (highest) | unary `-`, `!`, `~`, `&`, `&mut` |

## Import Modules

```jvl
import "math.jvl";

func main(): int {
    return factorial(5);
}
```

- Relative paths are resolved from the source file's directory
- Cyclic imports are silently deduplicated

## Built-in Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `putint(x)` | `func putint(x: int): int` | Print integer to stdout |
| `putchar(c)` | `func putchar(c: int): void` | Print character to stdout |
| `alloc(n)` | `func alloc(size: int): ptr<int>` | Allocate `size` words on the heap |
| `free(p)` | `func free(p: ptr<int>): void` | Release heap allocation (consumes ownership) |

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
