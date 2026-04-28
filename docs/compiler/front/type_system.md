# JVL Type System

JVL uses a static type system with explicit type annotations for function signatures and optional inference for local variables. The type system enforces memory safety through ownership tracking integrated with type checking.

## Type Categories

### Copy types

Copy types implement value semantics: assignment creates an independent copy, and the original remains valid.

| Type | Description |
|------|-------------|
| `int` | 128-bit signed integer |
| `char` | 128-bit character/small integer |
| `bool` | 128-bit boolean |

Copy types are:
- Passed by value (copied)
- Allowed in arithmetic and bitwise operations
- Comparable with `==`, `!=`, `<`, `>`, `<=`, `>=`
- Compatible through weak numeric coercion

### Move types

Move types implement ownership semantics: assignment transfers ownership, and the original becomes invalid.

| Type | Description |
|------|-------------|
| `ptr<T>` | Pointer to a value of type `T` |
| `array<T>` | Array of values of type `T` (alias for `ptr<T>`) |

Move types are:
- Passed by value (moved)
- Created by `alloc()`
- Consumed by `free()`
- Borrowed with `&` and `&mut`
- Indexed with `[]`

### Void type

`void` is a special type used only for function return types. It indicates that a function does not return a value.

```jvl
func greet(): void {
    putstr("Hello", 5);
}
```

Variables cannot have type `void`.

## Type Syntax

### Primitive types

```jvl
var x: int = 42;
var c: char = 'A';
var b: bool = true;
```

### Pointer types

```jvl
var p: ptr<int> = alloc(1);
var q: ptr<char> = alloc(10);
```

The angle brackets `<>` enclose the pointee type. Pointer types can point to any type, including other pointer types.

### Array types

```jvl
var a: array<int> = alloc(5);
```

`array<T>` is semantically identical to `ptr<T>`. Both represent a pointer to a contiguous block of memory.

## Type Inference

Local variable declarations may omit the type when an initializer is present:

```jvl
var x = 42;        // inferred as int
var y = true;      // inferred as bool
var z = 'A';       // inferred as char
```

The inference algorithm examines the initializer expression:
- Integer literals → `int`
- Boolean literals → `bool`
- Character literals → `char`
- String literals → `array<char>` (alias for `ptr<char>`)
- Function call results → return type of the function
- Borrow expressions → borrowed pointer type

**Cases where inference is not allowed**:

```jvl
var x;              // ERROR: no initializer
func f(x) { ... }   // ERROR: parameter type required
func f(): { ... }   // ERROR: return type required
```

## Type Compatibility

### Assignment compatibility

The following assignments are valid:

| Source type | Target type | Behavior |
|-------------|-------------|----------|
| `int` | `int` | Direct copy |
| `char` | `int` | Weak coercion (char promoted to int) |
| `bool` | `int` | Weak coercion (bool promoted to int) |
| `ptr<T>` | `ptr<T>` | Move semantics |
| `int` literal | `ptr<T>` | Implicit literal address conversion |

The following assignments are **invalid**:

| Source type | Target type | Reason |
|-------------|-------------|--------|
| `ptr<T>` | `ptr<U>` (T≠U) | Type mismatch |
| `int` variable | `ptr<T>` | No implicit conversion |
| `void` | any | void is not a value type |

### Function argument compatibility

Arguments must match parameter types according to the same rules as assignment. For Move types, passing an argument transfers ownership to the function.

### Return value compatibility

The returned expression must match the function's declared return type. For non-void functions, all control paths must return a value.

## Borrow Type System

Borrow expressions create temporary references:

```jvl
&x       // Immutable borrow: type is &T (borrows x as immutable)
&mut x   // Mutable borrow: type is &mut T (borrows x as mutable)
```

Borrow types are tracked separately from regular types during semantic analysis. They cannot be stored in variables or returned from functions.

## Type Checking in Expressions

### Arithmetic expressions

Both operands must be numeric types (`int`, `char`, `bool`). The result is `int` (promoted from `char` or `bool` if necessary).

```jvl
var a: int = 10;
var b: char = 5;
var c = a + b;    // c is int, b is coerced to int
```

### Comparison expressions

Both operands must be comparable. Numeric types can be compared with each other. Pointer types can only be compared for equality with pointers of the same type.

```jvl
var x = 10 < 20;       // bool
var p = alloc(1);
var q = alloc(1);
var same = p == q;     // bool (compares addresses)
```

### Logical expressions

Operands must be `bool`. The result is `bool`.

```jvl
var a = true && false;   // bool
```

### Index expressions

The base must be `ptr<T>` or `array<T>`. The index must be `int` (or coercible to `int`). The result type is `T`.

```jvl
var p: ptr<int> = alloc(3);
var x = p[0];    // x is int
```

### Call expressions

The callee must be a function. Arguments are checked against parameter types. The result type is the function's return type.

```jvl
func add(a: int, b: int): int { return a + b; }
var x = add(3, 4);    // x is int
```

## Type Errors

The compiler emits type errors with error codes in the `E1000` range:

| Error | Cause |
|-------|-------|
| Type mismatch in assignment | Assigning incompatible types |
| Type mismatch in argument | Passing wrong type to function |
| Type mismatch in return | Returning wrong type from function |
| Invalid operand for operator | Using non-numeric types in arithmetic |
| Cannot infer type | Missing type annotation where inference fails |
| Void variable | Declaring a variable with type `void` |

## Type Representation in the Compiler

The compiler represents types with the `Type` AST node:

```cpp
enum TypeKind {
    TYPE_INT, TYPE_CHAR, TYPE_BOOL, TYPE_VOID,
    TYPE_PTR, TYPE_ARRAY
};

struct Type {
    TypeKind kind;
    std::shared_ptr<Type> sub;  // for ptr<T> or array<T>
    int arraySize = 0;           // for array<T>[size]
};
```

Type checking is performed during semantic analysis (`sema.cpp`). The semantic analyzer maintains a symbol table mapping variable names to their types and tracks borrow state for ownership checking.
