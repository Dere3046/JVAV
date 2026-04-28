# JVL Import System

JVL provides a module import system for organizing code into reusable components. Imports allow you to use functions and declarations from other `.jvl` files.

## Import Syntax

```jvl
import "path/to/module.jvl";
```

The import path is a string literal specifying the file to import. Both double and forward slashes are supported in paths.

## Import Resolution

The compiler resolves import paths in the following order:

1. **Absolute path**: If the path starts with `/` or a drive letter, it is used directly
2. **Relative path**: Resolved relative to the importing file's directory
3. **Standard library fallback**: If not found, search alongside the compiler executable (`../std/`)

**Example resolution**:
```jvl
import "std/io.jvl";           // Searches: current_dir/std/io.jvl, then compiler_dir/../std/io.jvl
import "./utils/helpers.jvl";  // Relative to the importing file
import "/absolute/path.jvl";   // Used as-is
```

## Import Semantics

### Global namespace

Imported declarations become available in the **global namespace**. There is no namespacing or module prefixing.

```jvl
// math.jvl
func square(x: int): int {
    return x * x;
}

// main.jvl
import "math.jvl";

func main(): int {
    putint(square(5));   // 25
    return 0;
}
```

### Duplicate import deduplication

If the same file is imported multiple times (directly or transitively), it is only processed once:

```jvl
// a.jvl imports c.jvl
// b.jvl imports c.jvl
// main.jvl imports a.jvl and b.jvl
// c.jvl is processed exactly once
```

Cross-directory imports that resolve to the same physical file are also deduplicated using canonical path comparison.

### Cyclic imports

Cyclic imports (A imports B, B imports A) are **not** currently detected by the compiler and will cause a stack overflow during compilation. Avoid circular dependencies between modules.

## Standard Library

The `std/` directory at the project root contains standard library modules:

| Module | Description |
|--------|-------------|
| `std/io.jvl` | Console output functions |
| `std/math.jvl` | Integer math utilities |
| `std/mem.jvl` | Memory operations |
| `std/string.jvl` | String output |
| `std/file.jvl` | File I/O operations |

Standard library modules are regular JVL files. They can be read and modified like any other JVL code.

### PATH setup

The `std/` directory must be accessible from the compiler. It is automatically found if:

1. You run `jvlc` from the project root directory
2. You place `std/` next to the `jvlc` executable

**Error example**:
```
error[E0300]: cannot find standard library `std/io.jvl`
```

**Fix**: Run from the project root or copy `std/` to the compiler's directory.

## Writing Library Modules

Any `.jvl` file can be imported as a library. Follow these conventions:

1. **No `main()` function**: Libraries export functions, not entry points
2. **Explicit parameter types**: JVL requires annotated function parameters
3. **Document ownership**: If your function consumes a pointer, name it clearly
4. **Place in project directory**: Use `import "lib/foo.jvl";` or `import "std/foo.jvl";`

**Example library**:
```jvl
// lib/utils.jvl

func square(x: int): int {
    return x * x;
}

func max(a: int, b: int): int {
    if (a > b) {
        return a;
    }
    return b;
}
```

**Using the library**:
```jvl
// main.jvl
import "lib/utils.jvl";

func main(): int {
    println(square(7));    // 49
    println(max(3, 5));    // 5
    return 0;
}
```

## Import Implementation

The import system is implemented in the semantic analyzer (`sema.cpp`):

1. When an `import` declaration is encountered, the compiler resolves the file path
2. The imported file is lexed, parsed, and semantically analyzed
3. All top-level declarations from the imported module are added to the current module's symbol table
4. Imported modules are recursively processed for their own imports
5. A visited set prevents duplicate processing and cyclic imports

The code generator emits all imported module code into a single assembly file, eliminating the need for link-time resolution of imported symbols.
