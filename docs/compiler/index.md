# JVAV Compiler Overview

The JVAV compiler toolchain transforms high-level JVL source code into executable JVAV binary files. It consists of two major components: the frontend compiler and the backend assembler/linker.

## Toolchain Pipeline

```
.jvl source code
       |
       v
+-----------------+
|     jvlc        |  Frontend compiler
|   (C++17)       |
|  .jvl -> .jvav  |
+-----------------+
       |
       v
   .jvav assembly
       |
       v
+-----------------+
|     jvavc       |  Backend assembler & linker
|   (C++17)       |
| .jvav -> .bin   |
+-----------------+
       |
       v
    .bin binary
       |
       v
+-----------------+
|      jvm        |  Virtual machine executor
|    (C99)        |
+-----------------+
```

## Frontend (`jvlc`)

The frontend compiler translates JVL (JVAV Language) — a C-like high-level language — into JVAV assembly. It consists of four phases:

1. **Lexical analysis** (`lexer.cpp`): Converts source text into tokens
2. **Parsing** (`parser.cpp`): Builds an abstract syntax tree (AST) from tokens
3. **Semantic analysis** (`sema.cpp`): Type checking, ownership tracking, borrow checking
4. **Code generation** (`codegen.cpp`): Emits JVAV assembly from the validated AST

### Source files

- `jvavc/front/src/lexer.cpp` — Tokenizer
- `jvavc/front/src/parser.cpp` — Recursive descent parser
- `jvavc/front/src/sema.cpp` — Semantic analyzer and MimiWorld ownership checker
- `jvavc/front/src/codegen.cpp` — Assembly code generator
- `jvavc/front/src/diag.cpp` — Diagnostic formatting (Rust-style error messages)
- `jvavc/front/src/main.cpp` — CLI driver

## Backend (`jvavc`)

The backend assembler and linker transforms JVAV assembly text into binary executables. It consists of three phases:

1. **Preprocessing** (`parser.cpp`): Textual inclusion via `#include`, EQU resolution
2. **Assembly** (`parser.cpp` + `encoder.cpp`): Parse instructions, resolve labels, encode to binary
3. **Linking** (`linker.cpp`): Combine multiple object files, resolve external symbols, relocate addresses

### Source files

- `jvavc/back/src/parser.cpp` — Assembly parser and preprocessor
- `jvavc/back/src/encoder.cpp` — Instruction encoder
- `jvavc/back/src/linker.cpp` — Multi-file linker
- `jvavc/back/src/main.cpp` — CLI driver

## Tools (`disasm`)

The disassembler provides static analysis and dynamic tracing of JVAV binaries:

- `jvavc/tools/src/disasm.c` — Static disassembler and trace tool

## Compilation Modes

### Traditional pipeline

```bash
# Frontend
jvlc hello.jvl hello.jvav

# Backend
jvavc hello.jvav hello.bin

# Execution
jvm hello.bin
```

### One-shot compile and run

```bash
jvlc --run hello.jvl
```

This performs three steps automatically:
1. Compile `.jvl` to `.jvav`
2. Assemble `.jvav` to `.bin`
3. Execute with `jvm`

Intermediate files are cleaned up unless `-o` is used.

### Multi-file linking

```bash
jvavc main.jvav lib.jvav -o program.bin
```

Multiple `.jvav` files are parsed independently and linked into a single binary.

## Standard Library

The `std/` directory provides JVL modules for common operations:

- `std/io.jvl` — Console output
- `std/math.jvl` — Integer math utilities
- `std/mem.jvl` — Memory operations
- `std/string.jvl` — String output
- `std/file.jvl` — File I/O

Standard library modules are pure JVL code and are imported via the `import` statement.

## Error Handling Philosophy

Both frontend and backend produce detailed, Rust-style diagnostics:

- Error codes (e.g., `E0100`, `E1000`)
- File location with line and column
- Source snippet with context lines
- Caret underline pointing to the error
- Help text suggesting fixes

The frontend performs comprehensive semantic checking, catching type errors, ownership violations, and missing returns before code generation. The backend catches assembly syntax errors, undefined labels, and linking conflicts.

## Project Structure

```
jvavc/
├── front/           # Frontend compiler
│   ├── include/     # Header files (AST, lexer, parser, sema, codegen, diag)
│   └── src/         # Source files
├── back/            # Backend assembler & linker
│   ├── include/     # Header files (parser, encoder, linker)
│   └── src/         # Source files
└── tools/           # Disassembler
    └── src/         # Source files
```
