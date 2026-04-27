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
| `ptr<T>` ← `int` (literal address) | ✓ (implicit coercion for literal addresses) |
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

**Note:** `getint()` and `getchar()` are registered as builtins and available without declaration.

### Built-in Function Signatures (Exact)

| Function | Effective Signature | Notes |
|----------|---------------------|-------|
| `putint(x)` | `func putint(x: int): int` | Returns 0; prints signed decimal |
| `putchar(c)` | `func putchar(c: int): void` | Prints low byte as ASCII |
| `getint()` | `func getint(): int` | Reads signed decimal from stdin |
| `getchar()` | `func getchar(): int` | Reads one byte from stdin |
| `alloc(n)` | `func alloc(n: int): ptr<int>` | Bump allocator; no alignment |
| `free(p)` | `func free(p: ptr<int>): void` | Writes tombstone; does not reclaim |
| `exit(code)` | `func exit(code: int): void` | Stops VM immediately |
| `putstr(s, n)` | `func putstr(s: ptr<int>, n: int): void` | Prints `n` chars starting at `s` |
| `sleep(ms)` | `func sleep(ms: int): void` | Windows: `Sleep()`; POSIX: `usleep()` |
| `alloc(n)` | `func alloc(n: int): ptr<int>` | Allocate `n` 128-bit words on heap |
| `free(p)` | `func free(p: ptr<int>): void` | Release heap allocation |
| `exit(code)` | `func exit(code: int): void` | Terminate with exit code |
| `putstr(s, n)` | `func putstr(s: ptr<int>, n: int): void` | Print `n` chars from memory address |
| `sleep(ms)` | `func sleep(ms: int): void` | Pause execution for `ms` milliseconds |



### Standard Library Functions

Import via `import "std/xxx.jvl"`:

```jvl
import "std/io.jvl";      // println, print_newline, print_space, putstr, exit
import "std/math.jvl";    // abs, max, min, clamp, pow
import "std/mem.jvl";     // memcpy, memset
import "std/string.jvl";  // str_putn
import "std/file.jvl";    // fopen, fclose, fread, fwrite, fseek, ftell, mmap_file
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
- All `syscall` declarations have return type `int` (the VM always returns a value via R0)
- The compiler emits `.syscall name, cmd_id, arg_count` at the end of generated assembly
- The VM must already support the `cmd_id` in its `syscall_dispatch` handler

This is useful for using VM syscalls that are not included in the standard builtins (e.g., file I/O, mmap, or custom syscalls you added to the VM).

**Return value:** Syscalls return the value from `0xFFE4` (SYSCALL_RET mailbox). For file operations, this is typically a handle or fd (>0) on success, or 0/-1 on failure.

**Example — custom file I/O without stdlib:**
```jvl
syscall myopen, 4, 2;
syscall myclose, 5, 1;

func main(): int {
    var path = "test.txt";
    var mode = "wb";
    var fd = myopen(path, mode);
    if (fd > 0) {
        myclose(fd);
    }
    return 0;
}
```

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

**Duplicate imports are automatically deduplicated.** If module A and module B both import module C, C's code is emitted only once. Cross-directory imports that resolve to the same physical file are also deduplicated.

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

| Code Range | Category | Common Examples |
|------------|----------|-----------------|
| E0100–E0199 | Lexer errors | Invalid characters, unterminated strings, unknown escape sequences |
| E0200–E0299 | Parser errors | Missing semicolons, mismatched braces/parens, unexpected tokens |
| E0300–E0399 | I/O errors | Import file not found, cannot read file, standard library missing |
| E1000–E1999 | Semantic / MimiWorld errors | Undefined variables, type mismatches, ownership violations, missing returns |

Each error includes:
- Error code and message
- File location (`--> file.jvl:line:col`)
- Source snippet with context lines
- `^` caret underline pointing to the error position
- Help/note text where applicable

### Common Semantic Errors

| Error | Cause | Fix |
|-------|-------|-----|
| `cannot find function X in this scope` | Function not declared or imported | Add `import`, `extern`, or `syscall` declaration |
| `use of moved value X` | Variable was moved and used again | Reassign or borrow (`&X`) instead of moving |
| `cannot mutably borrow X because it is already borrowed` | Multiple overlapping borrows | Drop existing borrows before taking `&mut` |
| `missing return statement` | Non-void function lacks `return` on some path | Add `return` to all branches |
| `cannot find standard library std/X.jvl` | `std/` directory not in search path | Run from project root or copy `std/` next to binary |

---

## Implicit Conversions

### `int` → `ptr<T>` (Literal Addresses)

Integer literals can be implicitly converted to pointer types. This is primarily used for accessing memory-mapped I/O addresses:

```jvl
func write_mailbox(addr: int, val: int): void {
    var p: ptr<int> = addr;   // implicit conversion
    p[0] = val;
}

func main(): int {
    write_mailbox(0xFFE0, 14);   // trigger putchar syscall
    return 0;
}
```

**Note:** This only works for literal integer values. Assigning a variable `int` to `ptr<T>` without an explicit cast is a type error.

## `jvlc --run` Behavior

`jvlc --run` (or `-r`) is **not** an embedded VM. It performs three external subprocess calls in sequence:

1. `jvlc source.jvl → source.jvav` (frontend compile)
2. `jvavc source.jvav → source.bin` (backend assemble)
3. `jvm source.bin` (execute)

Intermediate files are cleaned up automatically unless `-o` is used. The working directory for all three steps is the same (the directory where `jvlc --run` was invoked).

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
