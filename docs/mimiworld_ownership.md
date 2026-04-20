# MimiWorld Ownership System

> MimiWorld (米米世界) is the JVAV ownership and borrow checker, inspired by Rust. It ensures memory safety at compile time without a garbage collector.

## Core Concepts

### Ownership

Every value (of a non-Copy type) has exactly **one owner** at any time.

```jvl
var p: ptr<int> = alloc_int();   // p owns the pointer
var q = p;                        // ownership moves from p to q
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
    putint(*p);   // read through borrow
}

func main(): int {
    var x = 42;
    print_int(&x);   // immutable borrow
    putint(x);       // OK: borrow ended, x is still owned
    return 0;
}
```

## Ownership Rules

1. **One owner** — Each non-Copy value has exactly one owner
2. **Move on assignment** — Assigning a non-Copy value transfers ownership
3. **Move on call** — Passing a non-Copy value as an argument transfers ownership
4. **Borrow conflicts** — Cannot borrow mutably while any borrow exists
5. **Use after move** — Using a moved value is a compile error
6. **Uninitialized use** — Using an uninitialized variable is a compile error

## Error Messages

MimiWorld provides detailed, Rust-style error messages:

```
[MimiWorld Error] line 5: Use of moved value 'p'
  --> hint: reassign 'p' or use a borrow (&p) instead

[MimiWorld Error] line 8: Cannot mutably borrow 'x' because it is already borrowed
  --> hint: drop existing borrows before taking &mut

[MimiWorld Warning] line 3: Unused variable 'tmp'
  --> hint: remove the declaration or prefix with _ to suppress
```

## Limitations

MimiWorld is a **simplified** ownership system compared to Rust:

- No lifetimes annotations (all borrows end at scope exit)
- No `Drop` trait (JVAV has no heap allocator in the VM)
- No pattern matching / destructuring
- Control flow merging is conservative (if a variable is moved in any branch, it is considered moved after the branch)
- Borrow checker operates at the semantic analysis level; runtime behavior is unchanged

## Example: Safe Pointer Usage

```jvl
func swap(a: &mut int, b: &mut int) {
    var tmp = *a;
    *a = *b;
    *b = tmp;
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
