# JVL Built-in Functions

JVL provides a set of built-in functions that are recognized by the compiler without requiring declarations or imports. These functions map directly to VM syscalls or are handled specially by the code generator.

## Console I/O

### putint

```jvl
func putint(x: int): int
```

Prints a signed 128-bit integer to stdout in decimal format. Returns 0.

**Example**:
```jvl
putint(42);      // prints: 42
putint(-100);    // prints: -100
```

### putchar

```jvl
func putchar(c: int): void
```

Prints the low byte of `c` as an ASCII character.

**Example**:
```jvl
putchar(72);     // prints: H
putchar(10);     // prints: newline
```

### getint

```jvl
func getint(): int
```

Reads a signed decimal integer from stdin. Returns the integer value.

**Note**: The VM provides this via syscall (SYS_GETINT), but it is **not** registered as a frontend builtin. To use it in JVL, declare it explicitly:
```jvl
syscall getint, 17, 0;
```

### getchar

```jvl
func getchar(): int
```

Reads a single character from stdin. Returns the character code or -1 on EOF.

**Note**: The VM provides this via syscall (SYS_GETCHAR), but it is **not** registered as a frontend builtin. To use it in JVL, declare it explicitly:
```jvl
syscall getchar, 16, 0;
```

### putstr

```jvl
func putstr(s: ptr<int>, n: int): void
```

Prints `n` characters starting at address `s`. Each character is read from the low byte of a 128-bit word.

**Example**:
```jvl
var msg = "Hello";
putstr(msg, 5);   // prints: Hello
```

## Heap Management

### alloc

```jvl
func alloc(n: int): ptr<int>
```

Allocates `n` 128-bit words on the heap using the VM's bump allocator. Returns the starting address or 0 on failure.

**Example**:
```jvl
var p: ptr<int> = alloc(3);
p[0] = 10;
p[1] = 20;
p[2] = 30;
```

**Ownership**: Returns a unique owner. The caller is responsible for freeing the allocation.

### free

```jvl
func free(p: ptr<int>): void
```

Releases ownership of a heap allocation. Writes a tombstone (`0xDEAD`) to the first word but does not reclaim memory.

**Example**:
```jvl
var p = alloc(10);
// use p
free(p);    // p is consumed
```

**Ownership**: Consumes the pointer. Using `p` after `free(p)` is a compile error.

## Process Control

### exit

```jvl
func exit(code: int): void
```

Terminates the VM immediately with the specified exit code.

**Example**:
```jvl
func main(): int {
    if (error) {
        exit(1);
    }
    exit(0);
    return 0;
}
```

**Note**: `exit()` does not return. Any code after `exit()` is unreachable.

### sleep

```jvl
func sleep(ms: int): void
```

Pauses execution for `ms` milliseconds.

**Example**:
```jvl
sleep(1000);   // sleep for 1 second
```

**Platform behavior**:
- Windows: Uses `Sleep()`
- POSIX: Uses `usleep()`

## Builtin Function Summary

| Function | Signature | VM Syscall | Description |
|----------|-----------|------------|-------------|
| `putint` | `func putint(x: int): int` | SYS_PUTINT (15) | Print integer |
| `putchar` | `func putchar(c: int): void` | SYS_PUTCHAR (14) | Print character |
| `getint`¹ | `func getint(): int` | SYS_GETINT (17) | Read integer |
| `getchar`¹ | `func getchar(): int` | SYS_GETCHAR (16) | Read character |

¹ Not registered as a frontend builtin; use `syscall` declaration in JVL.
| `putstr` | `func putstr(s: ptr<int>, n: int): void` | SYS_PUTSTR (19) | Print string |
| `alloc` | `func alloc(n: int): ptr<int>` | SYS_MALLOC (12) | Allocate heap memory |
| `free` | `func free(p: ptr<int>): void` | SYS_FREE (13) | Free heap memory |
| `exit` | `func exit(code: int): void` | SYS_EXIT (18) | Terminate VM |
| `sleep` | `func sleep(ms: int): void` | SYS_SLEEP (20) | Sleep |

## Custom Syscalls as Builtins

In addition to the standard builtins, you can declare custom syscalls that become available as builtin functions:

```jvl
syscall my_syscall, 99, 2;

func main(): int {
    var result = my_syscall(10, 20);
    return 0;
}
```

See [JVL Language Reference](jvl_language.md) for details on `syscall` declarations.

## Standard Library Functions

Functions in the `std/` directory are **not** builtins. They are regular JVL functions that must be imported:

```jvl
import "std/io.jvl";      // println, print_newline, print_space
import "std/math.jvl";    // abs, max, min, clamp, pow
import "std/mem.jvl";     // memcpy, memset
import "std/string.jvl";  // str_putn
import "std/file.jvl";    // fopen, fclose, fread, fwrite, fseek, ftell, mmap_file
```

Standard library functions compose builtin operations and syscalls to provide higher-level functionality.

## Implementation Details

Builtins are registered in the semantic analyzer (`sema.cpp`) with their exact signatures. The code generator (`codegen.cpp`) emits the appropriate assembly for each builtin call, typically generating a `CALL` to a `.syscall` wrapper.

When a builtin function is called:
1. The semantic analyzer verifies argument types against the registered signature
2. The code generator emits argument setup (PUSH instructions)
3. A `CALL` to the syscall wrapper is emitted
4. Stack cleanup is generated if needed

Some builtins (like `putint` and `putchar`) have special codegen paths that optimize the common case, while others use the standard `.syscall` wrapper mechanism.
