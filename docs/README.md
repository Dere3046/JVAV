# JVAV Documentation

This directory contains comprehensive documentation for the JVAV programming language and toolchain.

## Documentation Structure

### JVM (Virtual Machine)

Documentation for the JVAV Virtual Machine, the 128-bit executor that runs compiled JVAV binaries.

- **[architecture.md](JVM/architecture.md)** — VM architecture, registers, instruction format, and execution loop
- **[instruction_set.md](JVM/instruction_set.md)** — Complete reference for all 30 JVAV instructions
- **[memory_model.md](JVM/memory_model.md)** — Memory layout, stack behavior, heap allocator, and word-addressable design
- **[syscalls.md](JVM/syscalls.md)** — System call reference, mailbox protocol, and all 20 syscalls
- **[execution_model.md](JVM/execution_model.md)** — Execution loop, instruction dispatch, and runtime behavior

### Compiler

Documentation for the JVAV compiler toolchain, which transforms JVL source code into executable binaries.

#### Overview

- **[index.md](compiler/index.md)** — Compiler architecture, toolchain pipeline, and project structure

#### Frontend (JVL Language)

The frontend compiler (`jvlc`) translates JVL source code into JVAV assembly.

- **[jvl_language.md](compiler/front/jvl_language.md)** — Complete JVL language reference: syntax, types, operators, control flow, functions
- **[type_system.md](compiler/front/type_system.md)** — Type system: Copy/Move types, inference, compatibility, borrow types
- **[ownership.md](compiler/front/ownership.md)** — MimiWorld ownership and borrow checker: rules, patterns, and error messages
- **[builtin_functions.md](compiler/front/builtin_functions.md)** — Built-in functions: I/O, heap management, process control
- **[import_system.md](compiler/front/import_system.md)** — Module import system: resolution, deduplication, and standard library
- **[error_handling.md](compiler/front/error_handling.md)** — Error codes, diagnostic format, and common errors

#### Backend (Assembler & Linker)

The backend (`jvavc`) transforms JVAV assembly into binary executables.

- **[jvav_assembly.md](compiler/back/jvav_assembly.md)** — JVAV assembly language: instructions, directives, labels, and syntax
- **[instruction_encoding.md](compiler/back/instruction_encoding.md)** — Binary instruction format, encoding examples, and pseudo-instruction expansion
- **[directives.md](compiler/back/directives.md)** — Assembler directives: sections, symbols, data definition, and `.syscall`
- **[calling_convention.md](compiler/back/calling_convention.md)** — Function calling convention: argument passing, prologue/epilogue, and register usage
- **[linker.md](compiler/back/linker.md)** — Multi-file linking: symbol resolution, address assignment, and entry point
- **[pseudo_instructions.md](compiler/back/pseudo_instructions.md)** — Pseudo-instructions: expansion rules and temporary register selection

## Quick Reference

### File Extensions

| Extension | Description |
|-----------|-------------|
| `.jvl` | JVL source code (frontend language) |
| `.jvav` | JVAV assembly (backend language) |
| `.bin` | JVAV binary (VM executable) |

### Toolchain Commands

```bash
# Compile JVL to assembly
jvlc source.jvl output.jvav

# Assemble and link to binary
jvavc source.jvav output.bin

# Assemble multiple files
jvavc main.jvav lib.jvav -o program.bin

# One-shot compile and run
jvlc --run source.jvl

# Execute binary
jvm program.bin

# Disassemble
disasm program.bin
```

### Key Features

- **128-bit architecture**: All registers and memory words are 128 bits
- **Word-addressable**: Address `N` refers to word `N`, not byte `N * 16`
- **MimiWorld ownership**: Rust-inspired ownership and borrow checking
- **Standard library**: `std/io`, `std/math`, `std/mem`, `std/string`, `std/file`
- **Multi-file linking**: Separate compilation with symbol-based linking
- **Rust-style diagnostics**: Error codes, source snippets, and help text

## Contributing to Documentation

Documentation should be:
- **Accurate**: Based on actual source code behavior
- **Comprehensive**: Cover syntax, semantics, and edge cases
- **Clear**: Use examples and avoid ambiguity
- **Consistent**: Follow the style of existing documents

When updating documentation, ensure all affected sections are updated to maintain consistency across the documentation set.
