# MimiWorld Ownership System

MimiWorld (米米世界) is the JVAV ownership and borrow checker, inspired by Rust. It ensures memory safety at compile time without a garbage collector by tracking ownership and borrow state of all non-Copy values.

## Core Concepts

### Ownership

Every value of a non-Copy type has exactly **one owner** at any time. When ownership is transferred, the previous owner becomes invalid.

```jvl
var p: ptr<int> = alloc(1);   // p owns the heap allocation
var q = p;                     // ERROR: p has been moved
// p is now invalid; using p is a compile error
```

### Copy vs Move

| Category | Types | Semantics |
|----------|-------|-----------|
| Copy | `int`, `char`, `bool` | Assignment copies the value; original remains valid |
| Move | `ptr<T>`, `array<T>` | Assignment transfers ownership; original becomes invalid |

### Move operations

Ownership is transferred in two situations:

1. **Assignment**: `var q = p;` moves `p` into `q`
2. **Function call**: `consume(p);` moves `p` into the function parameter

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

## Borrowing

Instead of transferring ownership, you can borrow a value. Borrowing creates a temporary reference that does not take ownership.

### Immutable borrow

```jvl
func read(p: &int): int {
    return p[0];
}

func main(): int {
    var x = 42;
    read(&x);        // immutable borrow
    putint(x);       // OK: borrow ended, x is still owned
    return 0;
}
```

Rules for immutable borrows:
- Multiple immutable borrows (`&x`) are allowed simultaneously
- The borrowed value cannot be moved or mutably borrowed while immutable borrows exist
- The borrowed value can be read through other immutable borrows

### Mutable borrow

```jvl
func write(p: &mut int): void {
    p[0] = 42;
}

func main(): int {
    var x = 10;
    write(&mut x);   // mutable borrow
    putint(x);       // OK: borrow ended
    return 0;
}
```

Rules for mutable borrows:
- Only one mutable borrow (`&mut x`) is allowed at a time
- The borrowed value cannot be accessed in any other way during the mutable borrow
- Immutable borrows cannot coexist with a mutable borrow

### Borrow conflict example

```jvl
func main(): int {
    var x = 10;
    var a = &x;       // immutable borrow
    var b = &mut x;   // ERROR: cannot mutably borrow x while immutable borrow exists
    return 0;
}
```

## Ownership Rules

The MimiWorld checker enforces the following rules at compile time:

1. **One owner**: Each non-Copy value has exactly one owner
2. **Move on assignment**: Assigning a non-Copy value transfers ownership
3. **Move on call**: Passing a non-Copy value as an argument transfers ownership
4. **Borrow conflicts**: Cannot borrow mutably while any borrow exists
5. **Use after move**: Using a moved value is a compile error
6. **Uninitialized use**: Using an uninitialized variable is a compile error
7. **Heap safety**: `alloc()` returns a unique owner; `free()` consumes it

## Scope-Based Lifetime

Borrows automatically end when the scope exits:

```jvl
func main(): int {
    var x = 42;
    {
        var b = &mut x;
        b[0] = 100;
    }   // mutable borrow ends here
    putint(x);   // OK — x is no longer borrowed
    return 0;
}
```

All borrows are scope-bound. There are no lifetime annotations; borrows always end at the enclosing block boundary.

## Use-After-Free Detection

Using a pointer after `free()` is a compile error:

```jvl
func main(): int {
    var p: ptr<int> = alloc(1);
    free(p);
    p[0] = 42;   // ERROR: use of moved value 'p'
    return 0;
}
```

The ownership system tracks that `free()` consumes the pointer, making subsequent uses invalid.

## Common Patterns

### Pattern 1: Borrow instead of move

If you need to use a pointer after passing it to a function, borrow it:

```jvl
func print_first(p: &int): void {
    putint(p[0]);
}

func main(): int {
    var arr = alloc(3);
    arr[0] = 7; arr[1] = 8; arr[2] = 9;
    print_first(&arr);     // borrow — arr remains valid
    putint(arr[1]);        // OK
    free(arr);
    return 0;
}
```

### Pattern 2: Reassign after move

If a function consumes ownership, the caller must accept that the original variable is invalid:

```jvl
func dup(src: ptr<int>): ptr<int> {
    var dst = alloc(3);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    return dst;
}

func main(): int {
    var a = alloc(3);
    a[0] = 1; a[1] = 2; a[2] = 3;
    var b = dup(a);        // a is moved into dup
    // a is invalid here
    free(b);
    return 0;
}
```

### Pattern 3: Swap via mutable borrows

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
    putint(x);   // 10
    putint(y);   // 5
    return 0;
}
```

## Implementation

MimiWorld is implemented in `jvavc/front/src/sema.cpp`. Each variable tracks:

| Field | Meaning |
|-------|---------|
| `initialized` | Has the variable been assigned? |
| `moved` | Has ownership been transferred away? |
| `borrowCount` | Number of active immutable borrows |
| `mutBorrowed` | Is there an active mutable borrow? |
| `used` | Has the variable been read? |

The semantic analyzer walks the AST and enforces ownership rules at compile time. It produces Rust-style error messages with error codes, source locations, and help text.

## Error Messages

MimiWorld provides detailed diagnostics:

```
error[E1000]: use of moved value `p`
 --> test.jvl:5:5
    |
  5 |     var q = p;
    |         ^
   = help: reassign `p` or use a borrow (`&p`) instead

error[E1001]: cannot mutably borrow `x` because it is already borrowed
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

## Limitations

MimiWorld is a **simplified** ownership system compared to Rust:

- No lifetime annotations (all borrows end at scope exit)
- No `Drop` trait (JVAV does not auto-insert `free()` on scope exit)
- No pattern matching or destructuring
- Control flow merging is conservative
- The VM bump allocator does not reclaim freed memory
- Borrow checker operates at compile time; runtime behavior is unchanged

## Comparison with Rust

| Feature | Rust | MimiWorld |
|---------|------|-----------|
| Ownership tracking | Yes | Yes |
| Borrow checking | Yes | Yes |
| Lifetime annotations | Required for references | Not supported (scope-bound) |
| Drop trait | Yes | No |
| Pattern matching | Yes | No |
| Smart pointers | Yes (`Box`, `Rc`, `Arc`) | Only raw `ptr<T>` |
| Compile-time guarantees | Full | Simplified |
