# JVL Error Handling and Diagnostics

The JVL compiler produces detailed, Rust-style error messages designed to help developers quickly identify and fix issues. Errors include error codes, source locations, context snippets, and helpful suggestions.

## Diagnostic Format

Each diagnostic follows this format:

```
error[E####]: message
 --> file.jvl:line:column
    |
 LL | source line
    |     ^^^^^ error location
    |
    = help: suggestion for fixing the error
```

**Example**:
```
error[E1001]: use of moved value `p`
 --> test.jvl:5:9
    |
  5 |     p[0] = 42;
    |         ^
    |
    = help: reassign `p` or use a borrow (`&p`) instead
```

## Error Code Ranges

| Range | Category | Examples |
|-------|----------|----------|
| E0100–E0199 | Lexer errors | Invalid characters, unterminated strings, unknown escape sequences |
| E0200–E0299 | Parser errors | Missing semicolons, mismatched braces, unexpected tokens |
| E0300–E0399 | I/O errors | Import file not found, cannot read file, standard library missing |
| E0400–E0499 | Code generation errors | Internal codegen failures |
| E1000–E1999 | Semantic / MimiWorld errors | Type mismatches, ownership violations, missing returns |

## Common Errors

### Lexer Errors (E0100–E0199)

#### Invalid character

```
error[E0100]: invalid character '@'
 --> test.jvl:3:5
    |
  3 | var @x = 10;
    |     ^
```

#### Unterminated string

```
error[E0101]: unterminated string literal
 --> test.jvl:2:9
    |
  2 | var s = "hello;
    |         ^^^^^^^^
```

#### Unknown escape sequence

```
error[E0102]: unknown escape sequence \z
 --> test.jvl:4:13
    |
  4 | var s = "\z";
    |             ^^
```

### Parser Errors (E0200–E0299)

#### Missing semicolon

```
error[E0200]: expected ';' after statement
 --> test.jvl:3:14
    |
  3 | var x = 10
    |              ^
    |              help: add a semicolon here
```

#### Mismatched braces

```
error[E0201]: mismatched closing brace
 --> test.jvl:7:1
    |
  7 | }
    | ^
    | help: missing opening brace
```

#### Unexpected token

```
error[E0202]: unexpected token `)`
 --> test.jvl:4:10
    |
  4 | func foo(): {
    |          ^
    | help: expected type after `:`
```

### I/O Errors (E0300–E0399)

#### Import not found

```
error[E0300]: cannot find standard library `std/io.jvl`
 --> test.jvl:1:8
    |
  1 | import "std/io.jvl";
    |        ^^^^^^^^^^^^
    |
    = help: run from project root or copy `std/` next to the compiler binary
```

#### Cannot read file

```
error[E0301]: cannot read file `lib/missing.jvl`
 --> test.jvl:2:8
    |
  2 | import "lib/missing.jvl";
    |        ^^^^^^^^^^^^^^^^^
```

### Semantic Errors (E1000–E1999)

#### Undefined variable

```
error[E1000]: cannot find value `x` in this scope
 --> test.jvl:3:9
    |
  3 | putint(x);
    |         ^
```

#### Type mismatch

```
error[E1001]: mismatched types
 --> test.jvl:4:9
    |
  4 | var x: bool = 42;
    |         ^^^   ^^
    |         |     |
    |         |     expected `bool`, found `int`
    |         expected due to this
```

#### Use of moved value

```
error[E1002]: use of moved value `p`
 --> test.jvl:6:5
    |
  5 |     var q = p;
    |             - value moved here
  6 |     p[0] = 10;
    |     ^ value used here after move
    |
    = help: reassign `p` or use a borrow (`&p`) instead
```

#### Borrow conflict

```
error[E1003]: cannot mutably borrow `x` because it is already borrowed
 --> test.jvl:8:13
    |
  7 |     var a = &x;
    |             -- immutable borrow occurs here
  8 |     var b = &mut x;
    |             ^^^^^^
    |             mutable borrow not allowed
    |
    = help: drop existing borrows before taking `&mut`
```

#### Missing return

```
error[E1004]: missing return statement
 --> test.jvl:2:1
    |
  2 | func foo(): int {
    | ^^^^^^^^^^^^^^^
  3 |     var x = 10;
  4 | }
    | ^ function body ends without returning a value
    |
    = help: add `return` to all branches
```

#### Function not found

```
error[E1005]: cannot find function `unknown` in this scope
 --> test.jvl:3:5
    |
  3 |     unknown(1, 2);
    |     ^^^^^^^
    |
    = help: import the module or declare the function
```

## Warnings

The compiler also emits warnings for suspicious but not strictly invalid code:

### Unused variable

```
warning[W2000]: unused variable `tmp`
 --> test.jvl:3:9
    |
  3 |     var tmp: int = 0;
    |         ^^^
    |
    = help: remove the declaration or prefix with `_` to suppress
```

Prefixing a variable with `_` suppresses the unused warning:
```jvl
var _unused = 42;   // no warning
```

> **Note**: This suppression requires the variable name to start with `_`. The prefix must be the very first character of the identifier.

## Diagnostic Implementation

Diagnostics are implemented in `jvavc/front/src/diag.cpp`. The `Diag` class formats error messages with:

- Error code and severity (error, warning, note)
- File path, line, and column
- Source snippet with line numbers
- Caret underline (`^`) pointing to the error position
- Help text with suggestions

The diagnostic system supports multiple errors per compilation and attempts to continue parsing after errors to report as many issues as possible.

## Tips for Resolving Errors

1. **Start with the first error**: Later errors may be caused by earlier ones
2. **Read the help text**: Many errors include suggestions for fixes
3. **Check the source location**: The caret points to where the compiler detected the issue, which may differ from the root cause
4. **Use explicit types**: If inference fails, add type annotations
5. **Check imports**: Missing imports are a common cause of "not found" errors
6. **Review ownership**: Move and borrow errors often indicate a need to restructure code
