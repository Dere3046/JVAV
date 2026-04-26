# JVL Frontend Language Reference

JVL (JVAV Language) is the high-level frontend language of the JVAV platform. It compiles to JVAV assembly (`.jvav`) via `jvlc`, then to binary (`.bin`) via `jvavc`, and finally executes on the `jvm` virtual machine.

---

## Basic Syntax

### Program Structure

A JVL program consists of zero or more `import` statements followed by declarations:

```jvl
import "std/io.jvl";
import "std/math.jvl";

func add(a: int, b: int): int {
    return a + b;
}

func main(): int {
    putint(add(3, 4));   // 7
    return 0;
}
```

### Comments

```jvl
// Line comment
/* Block comment */
```

---

## Types

| Type | Size | Copy? | Description |
|------|------|-------|-------------|
| `int` | 128-bit | Yes | Signed integer (full 128-bit range) |
| `char` | 128-bit | Yes | Character / small integer |
| `bool` | 128-bit | Yes | Boolean (`true` / `false`) |
| `void` | — | — | Function return type only |
| `ptr<T>` | 128-bit | **No** | Pointer to a value of type `T` (Move semantics) |
| `array<T>` | 128-bit | **No** | Alias for `ptr<T>` |

### Type Inference

Local variables support type inference from their initializer:

```jvl
var x: int = 5;       // explicit
var y = 3;            // inferred as int
var z = true;         // inferred as bool
var c = 'A';          // inferred as char
```

Inference is **not** supported for:
- Function parameters (must be annotated)
- Function return types (must be annotated)
- Empty initializers (`var x;` is invalid)

### Type Compatibility

JVL uses weak numeric coercion for builtins but is otherwise strictly typed:

| Assignment | Allowed? |
|------------|----------|
| `int` ← `int` | ✓ |
| `int` ← `char` | ✓ (weak coercion) |
| `int` ← `bool` | ✓ (weak coercion) |
| `ptr<T>` ← `ptr<T>` | ✓ |
| `ptr<T>` ← `ptr<U>` (T≠U) | ✗ |
| `void` as variable type | ✗ |

---

## Variables and Constants

```jvl
var x: int = 10;           // mutable variable
const PI = 3;              // compile-time constant (no type annotation needed)
```

Variables must be initialized at declaration. `const` values are inlined at compile time.

---

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

### Operator Reference

| Operator | Description |
|----------|-------------|
| `+` `-` `*` `/` `%` | Arithmetic |
| `&` `\|` `^` `~` `<<` `>>` | Bitwise |
| `==` `!=` `<` `>` `<=` `>=` | Comparison |
| `&&` `\|\|` `!` | Logical |
| `=` | Assignment |
| `&x` | Immutable borrow |
| `&mut x` | Mutable borrow |

---

## Control Flow

### If / Else

```jvl
if (x > 0) {
    putint(x);
} else if (x < 0) {
    putint(-x);
} else {
    putchar(48);   // '0'
}
```

### While Loop

```jvl
var i = 0;
while (i < 10) {
    putint(i);
    i = i + 1;
}
```

### For Loop

```jvl
for (var i = 0; i < 10; i = i + 1) {
    putint(i);
}
```

The `for` loop consists of three parts separated by `;`:
1. **Init**: executed once before the loop (`var i = 0`)
2. **Condition**: checked before each iteration (`i < 10`)
3. **Step**: executed after each iteration (`i = i + 1`)

All three parts are optional. `for (;;) { ... }` creates an infinite loop.

---

## Functions

### Declaration

```jvl
func name(param: type): return_type {
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

### Parameters and Arguments

Arguments are passed by value. For non-Copy types (`ptr<T>`), this means **Move** semantics:

```jvl
func consume(p: ptr<int>): void {
    free(p);
}

func main(): int {
    var p: ptr<int> = alloc(1);
    consume(p);      // p is moved into consume
    // p is invalid here
    return 0;
}
```

### Return Statement

`return` is required in all non-void functions. The compiler checks that all code paths return a value.

---

## Built-in Functions

These functions are recognized by the compiler and do not need to be declared or imported:

| Function | Signature | Description |
|----------|-----------|-------------|
| `putint(x)` | `func putint(x: int): int` | Print signed integer to stdout |
| `putchar(c)` | `func putchar(c: int): void` | Print low byte as ASCII character |
| `getint()` | `func getint(): int` | Read integer from stdin |
| `getchar()` | `func getchar(): int` | Read character from stdin |
| `alloc(n)` | `func alloc(n: int): ptr<int>` | Allocate `n` 128-bit words on heap |
| `free(p)` | `func free(p: ptr<int>): void` | Release heap allocation |
| `exit(code)` | `func exit(code: int): void` | Terminate with exit code |
| `putstr(s, n)` | `func putstr(s: ptr<int>, n: int): void` | Print `n` chars from memory address |
| `sleep(ms)` | `func sleep(ms: int): void` | Pause execution for `ms` milliseconds |

**Note:** `getint()` and `getchar()` are available at the assembly level but are not currently exposed as JVL builtins (they exist in the `.syscall` wrappers but are not registered in the semantic analyzer). To use them from JVL, declare them manually:

```jvl
extern func getint(): int;
extern func getchar(): int;
```

### Standard Library Functions

Import via `import "std/xxx.jvl"`:

```jvl
import "std/io.jvl";      // println, print_newline, print_space, putstr, exit
import "std/math.jvl";    // abs, max, min, clamp, pow
import "std/mem.jvl";     // memcpy, memset
import "std/string.jvl";  // str_putn
```

Standard library functions are regular JVL functions that compose built-in operations.

---

## Pointers and Heap Allocation

```jvl
var p: ptr<int> = alloc(3);   // allocate 3 words
p[0] = 10;
p[1] = 20;
p[2] = 30;
putint(p[0] + p[1] + p[2]);   // 60
free(p);
```

Pointer arithmetic is **not** supported. Use array indexing `p[i]`.

### Ownership Rules

- `alloc()` returns a unique owner
- Assigning a pointer moves ownership
- `free()` consumes ownership
- Using a moved pointer is a compile error

See `mimiworld_ownership.md` for full details.

---

## Custom System Calls

JVL supports declaring custom syscalls directly in source code. This generates a `.syscall` wrapper in the output assembly without requiring hand-written assembly files.

```jvl
syscall my_custom, 99, 2;

func main(): int {
    my_custom(255, 1);
    return 0;
}
```

**Syntax:**
```jvl
syscall name, cmd_id, arg_count;
```

**Constraints:**
- `arg_count` must be in range `0..3`
- The declared name becomes a builtin function available in the module
- The compiler emits `.syscall name, cmd_id, arg_count` at the end of generated assembly
- The VM must already support the `cmd_id` in its `syscall_dispatch` handler

This is useful for using VM syscalls that are not included in the standard builtins (e.g., file I/O, mmap, or custom syscalls you added to the VM).

## Import System

```jvl
import "std/io.jvl";
import "relative/path.jvl";
```

Import resolution:
1. If the path is absolute, use it directly
2. If relative, resolve relative to the importing file's directory
3. If not found, search alongside the compiler executable (`../std/`)
4. If still not found, produce error E0300 (cannot find standard library)

Imported files are parsed and their top-level declarations become available in the importing module. There is no namespacing — all declarations share a global namespace.

---

## String Literals

```jvl
var s = "Hello, World!";
```

String literals support C-style escapes:

| Escape | Meaning |
|--------|---------|
| `\n` | newline |
| `\t` | tab |
| `\r` | carriage return |
| `\\` | backslash |
| `\"` | double quote |
| `\xNN` | byte with hex value `NN` |

**Important:** In the JVAV VM, strings are stored as word arrays (one character per 128-bit word). Passing a string to `putstr` requires the length in characters:

```jvl
var msg = "Hi!";
putstr(msg, 3);
```

There is **no null terminator**. Always pass the length explicitly.

---

## Borrow Expressions

```jvl
func read(p: &int): int {
    return p[0];
}

func write(p: &mut int): void {
    p[0] = 42;
}

func main(): int {
    var x = 10;
    read(&x);        // immutable borrow
    write(&mut x);   // mutable borrow
    return 0;
}
```

Borrowing rules (enforced at compile time):
- Multiple immutable borrows (`&x`) are allowed simultaneously
- Only one mutable borrow (`&mut x`) is allowed at a time
- Cannot borrow mutably while any borrow (mutable or immutable) exists
- Borrowed value cannot be moved or accessed directly during the borrow

---

## Error Codes

The compiler emits Rust-style diagnostics with error codes:

| Code Range | Category |
|------------|----------|
| E0100–E0199 | Lexer errors (invalid characters, unterminated strings, etc.) |
| E0200–E0299 | Parser errors (missing semicolons, mismatched braces, etc.) |
| E0300–E0399 | I/O errors (file not found, import resolution, etc.) |
| E1000+ | Semantic errors (undefined variables, type mismatches, ownership violations, etc.) |

Each error includes:
- Error code and message
- File location (`--> file.jvl:line:col`)
- Source snippet with context lines
- `^` caret underline pointing to the error position
- Help/note text where applicable

---

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
