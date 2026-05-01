# JVL Language Extensions

This document describes the new language features added to JVL (JVAV Language) to support systems programming, memory-mapped I/O, and low-level operations.

## Table of Contents

1. [Struct and Union Types](#struct-and-union-types)
2. [sizeof and offsetof](#sizeof-and-offsetof)
3. [byte and uint Types](#byte-and-uint-types)
4. [Multidimensional Arrays](#multidimensional-arrays)
5. [Type Casting](#type-casting)
6. [volatile Modifier](#volatile-modifier)
7. [Inline Assembly](#inline-assembly)
8. [Const Export to Assembly](#const-export-to-assembly)
9. [Implementation Notes](#implementation-notes)

---

## Struct and Union Types

### Syntax

```jvl
struct Point {
    x: int;
    y: int;
}

union Value {
    i: int;
    c: char;
    data: int[4];
}
```

### Field Access

Structs and unions are accessed through pointers using the `->` operator:

```jvl
var p: ptr<Point> = alloc(sizeof(Point));
p->x = 3;
p->y = 4;
putint(p->x);
```

The `.` operator is also supported for direct struct values, but in practice structs are always manipulated through pointers.

### Memory Layout

**Structs**: Fields are laid out sequentially. The offset of each field is the sum of the sizes of all preceding fields.

```jvl
struct Example {
    a: int;      // offset 0
    b: int[4];   // offset 1
    c: char;     // offset 5
}
// sizeof(Example) = 6 words
```

**Unions**: All fields start at offset 0. The size is the maximum field size.

```jvl
union Example {
    i: int;      // offset 0
    data: int[4]; // offset 0
}
// sizeof(Example) = 4 words
```

### Arrays of Structs

Array indexing on `ptr<Struct>` automatically scales by the struct size:

```jvl
var items: ptr<Item> = alloc(10 * sizeof(Item));
items[i]->field = value;  // address = items + i * sizeof(Item) + offset(field)
```

### Edge Cases

- Empty structs are allowed but have size 0
- Recursive structs (containing a pointer to themselves) are supported
- Field names must be unique within a struct/union
- Struct/union types must be defined before use

---

## sizeof and offsetof

### sizeof

Returns the size of a type or expression in 128-bit words.

```jvl
sizeof(int)              // 1
sizeof(int[8])           // 8
sizeof(Point)            // struct size
sizeof(expression)       // size of expression's type
```

### offsetof

Returns the offset of a field within a struct or union in 128-bit words.

```jvl
struct Point {
    x: int;
    y: int;
}

offsetof(Point, x)       // 0
offsetof(Point, y)       // 1
```

### Compile-time Evaluation

Both `sizeof` and `offsetof` are compile-time constants. They can be used in constant expressions:

```jvl
const BLOCK_SIZE = 256;
const BUFFER_SIZE = sizeof(int[BLOCK_SIZE]);
```

### Edge Cases

- `sizeof(void)` returns 1
- `offsetof` on a union always returns 0
- Unknown struct types in `sizeof`/`offsetof` produce compile errors

---

## byte and uint Types

### byte

The `byte` type represents an 8-bit logical value. It is semantically identical to `char` but conveys intent for raw memory operations.

```jvl
var buf: ptr<byte> = alloc(64);
buf[0] = 0xFF;
buf[1] = 0x00;
```

**Important**: On the JVAV VM, each `byte` occupies one full 128-bit word. The VM does not support packed byte arrays. `ptr<byte>` provides logical byte addressing where each byte is stored in its own word.

### uint

The `uint` type represents an unsigned 128-bit integer. It has the same machine representation as `int` but is treated as unsigned in semantic analysis.

```jvl
var flags: uint = 0xFFFFFFFF;
var mask: uint = (1u << 5);
```

### Type Compatibility

Both `byte` and `uint` are Copy types. They are implicitly compatible with `int`:

| From | To | Allowed? |
|------|-----|----------|
| `byte` | `int` | Yes |
| `uint` | `int` | Yes |
| `int` | `uint` | Yes |
| `byte` | `uint` | Yes |

---

## Multidimensional Arrays

### Syntax

Fixed-size arrays use C-style syntax:

```jvl
var row: int[8];           // array of 8 ints
var matrix: int[4][8];     // 4 arrays of 8 ints each
```

### Type Representation

`int[4][8]` is represented as `array<array<int, 8>, 4>`. The dimensions are parsed left-to-right but nested right-to-left to match C semantics.

### Usage with Pointers

Multidimensional arrays are typically allocated on the heap:

```jvl
var matrix: ptr<int[4][8]> = alloc(4 * 8);
```

### sizeof

`sizeof` works correctly with multidimensional arrays:

```jvl
sizeof(int[4][8])     // 32
sizeof(int[3][2])     // 6
```

### Limitations

- Stack-allocated multidimensional arrays are not supported
- Array indexing on `ptr<T[N]>` loads a single word, not the entire sub-array
- For true multidimensional access, use manual offset calculation or struct arrays

---

## Type Casting

### C-Style Casts

```jvl
(Type)expression
```

Examples:

```jvl
var p: ptr<int> = (ptr<int>)0xFFE0;
var c: char = (char)65;
var u: uint = (uint)-1;
```

### Supported Casts

- Any numeric type to any other numeric type
- `int` literal to any pointer type
- Pointer type to pointer type
- Any type to `void` (discards value)

### Implementation

At the machine level, casts are generally no-ops because all values are 128-bit words. The cast operator serves as a type-system assertion and enables operations that would otherwise be rejected by semantic analysis.

---

## volatile Modifier

### Syntax

```jvl
volatile ptr<Type>
volatile int
```

### Purpose

The `volatile` modifier indicates that a variable's value may change unpredictably (e.g., due to hardware or concurrent modification). It is primarily used for memory-mapped I/O.

```jvl
var mailbox: volatile ptr<int> = 0xFFE0;
var cmd = mailbox[0];   // Read from hardware register
mailbox[1] = cmd;       // Write to hardware register
```

### Current Behavior

In the current implementation, `volatile` is parsed and preserved in the AST but does not affect code generation. Future implementations may prevent optimization of volatile accesses.

---

## Inline Assembly

### Syntax

```jvl
asm {
    "instruction1"
    "instruction2"
    ...
};
```

### Usage

Inline assembly blocks contain raw JVAV assembly instructions that are emitted verbatim into the output:

```jvl
func read_status(): int {
    asm {
        "LDI R0, 0xFFE0"
        "LDR R0, [R0]"
    };
    return 0;  // R0 already contains result
}
```

### Register Conventions

When writing inline assembly:
- **R0**: Return value / first argument
- **R1-R3**: Additional arguments / temporaries
- **R4-R5**: Temporaries (safe to clobber)
- **R6**: Frame pointer (must preserve across asm block)
- **R7**: Reserved for backend use
- **SP (R9)**: Stack pointer

### Safety

Inline assembly bypasses:
- Type checking
- Register allocation
- Stack management
- Ownership tracking

**Guidelines**:
- Prefer `asm` only when necessary
- Save/restore callee-saved registers if modified
- Do not modify R6 (frame pointer) or SP without restoring
- Ensure the asm block leaves the stack in a valid state

---

## Const Export to Assembly

### Behavior

Global `const` declarations are exported to the assembly output as `.equ` directives:

```jvl
const VFS_BLOCK_SIZE = 4096;
const MAX_FILES = 16;
```

Generated assembly:
```asm
    VFS_BLOCK_SIZE: EQU 4096
    MAX_FILES: EQU 16
```

### Benefits

- Assembly files can reference JVL constants
- The linker resolves `.equ` symbols across files
- Standard library can use named constants instead of magic numbers

### Compile-time Expressions

Constants support compile-time expressions:

```jvl
const BLOCK_WORDS = VFS_BLOCK_SIZE / sizeof(int);
const BUF_SIZE = sizeof(int[BLOCK_WORDS]);
```

Supported operations in constant expressions:
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Bitwise: `&`, `|`, `^`, `<<`, `>>`, `~`
- Unary: `-`, `~`
- `sizeof(Type)`
- `offsetof(Type, field)`

---

## Implementation Notes

### Word-Addressed Architecture

The JVAV VM uses 128-bit words. All types except arrays occupy exactly one word:

| Type | Words |
|------|-------|
| `int`, `uint`, `char`, `byte`, `bool` | 1 |
| `ptr<T>` | 1 |
| `T[N]` | N * sizeof(T) |
| `struct` | Sum of field sizes |
| `union` | Max field size |

### Array Indexing Scaling

Array indexing automatically scales by the element size:

```jvl
// ptr<int>[ i ]  ->  address + i * 1
// ptr<Struct>[ i ]  ->  address + i * sizeof(Struct)
```

### Struct Field Access Code Generation

For `ptr->field`:
1. Load pointer value (address)
2. Add field offset
3. Load/store at computed address

### Type System Internals

New AST nodes added:
- `StructDecl`, `UnionDecl`: type definitions
- `SizeofExpr`, `OffsetofExpr`: compile-time size/offset
- `CastExpr`: type casting
- `FieldExpr`: field access (`->` and `.`)
- `InlineAsmStmt`: inline assembly block

New `TypeKind` values:
- `TYPE_BYTE`, `TYPE_UINT`
- `TYPE_STRUCT`, `TYPE_UNION`

### Testing

Each new feature includes integration tests in `tests/cases/front/`:
- `struct_basic.jvl`
- `sizeof_basic.jvl`
- `byte_type.jvl`
- `uint_type.jvl`
- `multidim_array.jvl`
- `cast_basic.jvl`
- `offsetof_basic.jvl`
- `union_basic.jvl`
- `inline_asm.jvl`
- `struct_array_field.jvl`
- `union_sizes.jvl`
- `const_expr.jvl`
- `volatile_ptr.jvl`

---

## Migration Guide

### From C

| C Syntax | JVL Equivalent |
|----------|----------------|
| `struct S { int x; };` | `struct S { x: int; }` |
| `S* p = malloc(sizeof(S));` | `var p: ptr<S> = alloc(sizeof(S));` |
| `p->x = 5;` | `p->x = 5;` |
| `s.x = 5;` | `s.x = 5;` (if s is struct value) |
| `sizeof(int)` | `sizeof(int)` |
| `offsetof(S, x)` | `offsetof(S, x)` |
| `(int*)ptr` | `(ptr<int>)ptr` |
| `unsigned int` | `uint` |
| `unsigned char` | `byte` |
| `int arr[8];` | `var arr: ptr<int[8]> = alloc(8);` |
| `int arr[4][8];` | `var arr: ptr<int[4][8]> = alloc(32);` |
| `volatile int* p;` | `var p: volatile ptr<int>;` |
| `__asm__("...");` | `asm { "..."; }` |

### Known Limitations

1. **Byte packing**: `ptr<byte>` does not pack bytes into words; each byte occupies one word
2. **Stack arrays**: Fixed-size arrays must be heap-allocated via `alloc()`
3. **Struct values**: Direct struct locals (`var s: Point;`) are not fully supported
4. **Array decay**: `ptr<T[N]>[i]` loads the first word of the sub-array, not its address
5. **volatile**: Currently parsed but not enforced in codegen
