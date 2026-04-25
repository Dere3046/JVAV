# MimiWorld Ownership System

> MimiWorld (米米世界) is the JVAV ownership and borrow checker, inspired by Rust. It ensures memory safety at compile time without a garbage collector.

## Core Concepts

### Ownership

Every value (of a non-Copy type) has exactly **one owner** at any time.

```jvl
var p: ptr<int> = alloc(1);   // p owns the heap allocation
var q = p;                     // ERROR: p has been moved
// p is now invalid; using p is a compile error
```

### Copy Types

The following types implement **Copy** semantics (assignment copies the value, original remains valid):

- `int`
- `char`
- `bool`

All other types (`ptr<T>`, `array<T>`) use **Move** semantics.

### Borrowing

You can borrow a value instead of taking ownership:

| Syntax | Name | Rules |
|--------|------|-------|
| `&x` | Immutable borrow | Multiple `&x` allowed; `x` cannot be moved or mutably borrowed |
| `&mut x` | Mutable borrow | Only one `&mut x` at a time; `x` cannot be accessed in any other way |

```jvl
func print_int(p: &int) {
    putint(p[0]);   // read through borrow
}

func main(): int {
    var x = 42;
    print_int(&x);   // immutable borrow
    putint(x);       // OK: borrow ended, x is still owned
    return 0;
}
```

## Heap Allocation

JVAV provides a simple bump allocator via built-in functions:

| Function | Signature | Description |
|----------|-----------|-------------|
| `alloc(n)` | `func alloc(size: int): ptr<int>` | Allocate `size` 128-bit words on the heap |
| `free(p)` | `func free(p: ptr<int>): void` | Release ownership of `p` (tombstone in VM) |

### Example

```jvl
func main(): int {
    var p: ptr<int> = alloc(3);
    p[0] = 7;
    p[1] = 8;
    p[2] = 9;
    putint(p[0] + p[1] + p[2]);   // prints 24
    free(p);
    return 0;
}
```

### Use-After-Free Detection

MimiWorld tracks ownership at compile time. Using a pointer after `free()` is a **compile error**:

```jvl
func main(): int {
    var p: ptr<int> = alloc(1);
    free(p);
    p[0] = 42;   // [MimiWorld Error] Use of moved value 'p'
    return 0;
}
```

## Ownership Rules

1. **One owner** — Each non-Copy value has exactly one owner
2. **Move on assignment** — Assigning a non-Copy value transfers ownership
3. **Move on call** — Passing a non-Copy value as an argument transfers ownership (including to `free()`)
4. **Borrow conflicts** — Cannot borrow mutably while any borrow exists
5. **Use after move** — Using a moved value is a compile error
6. **Uninitialized use** — Using an uninitialized variable is a compile error
7. **Heap safety** — `alloc()` returns a unique owner; `free()` consumes it

## Error Messages

MimiWorld provides detailed, Rust-style error messages with error codes, source locations, and help text:

```
error[E0000]: use of moved value `p`
 --> test.jvl:5:5
    |
  5 |     var q = p;
    |         ^
   = help: reassign `p` or use a borrow (`&p`) instead

error[E0001]: cannot mutably borrow `x` because it is already borrowed
 --> test.jvl:8:13
    |
  8 |     var t = &mut x;
    |             ^
   = help: drop existing borrows before taking `&mut`

warning[W2000]: unused variable `tmp`
 --> test.jvl:3:9
    |
  3 |     var tmp: int = 0;
    |         ^
   = help: remove the declaration or prefix with `_` to suppress
```

Error code ranges:
- `E1000–E1999`: Semantic analysis and MimiWorld ownership errors (type checking, void parameters, missing returns, move/borrow violations)
- `E0100–E0199`: Lexer errors
- `E0200–E0299`: Parser errors
- `E0300–E0399`: I/O and codegen errors

## VM Heap Implementation

The JVAV virtual machine implements a simple bump allocator:

- **Heap base**: `mem_code_end + STACK_GUARD`
- **Allocation**: `heap_ptr` grows upward
- **Stack guard**: Prevents heap/stack collision
- **Free**: Writes `0xDEAD` tombstone to first word; detects invalid access
- **Syscalls**: `SYS_MALLOC (12)` / `SYS_FREE (13)` via syscall mailbox

Memory layout:
```
[ code/data | stack_guard | heap (grows up) | ...free... | stack (grows down) ]
0           code_end      heap_base       heap_ptr                  SP
```

## Limitations

MimiWorld is a **simplified** ownership system compared to Rust:

- No lifetimes annotations (all borrows end at scope exit)
- No `Drop` trait (JVAV does not auto-insert `free()` on scope exit)
- No pattern matching / destructuring
- Control flow merging is conservative
- Bump allocator does not reclaim freed memory (fragments over time)
- Borrow checker operates at the semantic analysis level; runtime behavior is unchanged

## Example: Safe Pointer Usage

```jvl
func swap(a: &mut int, b: &mut int) {
    var tmp = a[0];
    a[0] = b[0];
    b[0] = tmp;
}

func main(): int {
    var x = 5;
    var y = 10;
    swap(&mut x, &mut y);
    putint(x);   // prints 10
    putint(y);   // prints 5
    return 0;
}
```

## Implementation

MimiWorld is implemented in `jvavc/front/src/sema.cpp`. Each variable tracks:

- `initialized` — Has the variable been assigned?
- `moved` — Has ownership been transferred away?
- `borrowCount` — Number of active immutable borrows
- `mutBorrowed` — Is there an active mutable borrow?
- `used` — Has the variable been read?

The semantic analyzer walks the AST and enforces the ownership rules at compile time.
