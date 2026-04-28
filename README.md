# JVAV Programming Language

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://github.com/Dere3046/JVAV/actions/workflows/ci.yml/badge.svg)](https://github.com/Dere3046/JVAV/actions)
[![CI-Android](https://github.com/Dere3046/JVAV/actions/workflows/ci-android.yml/badge.svg)](https://github.com/Dere3046/JVAV/actions)
[![Stars](https://img.shields.io/github/stars/Dere3046/JVAV?style=social)](https://github.com/Dere3046/JVAV)
![JVAV](https://img.shields.io/badge/JVAV-128--bit-ff69b4.svg)
![C/C++](https://img.shields.io/badge/C%2FC%2B%2B-99%2F17-blue.svg)

> **Disclaimer:** This is a joke / parody project. JVAV was "proposed by Dr. Zhang Haoyang" and implemented by **Dere3046**. Please do not harass, attack, or send hate to Zhang Haoyang or anyone associated with JVAV. This project exists purely for fun and educational purposes.

JVAV is a complete custom programming language and toolchain built from scratch, featuring a 128-bit instruction set architecture, a C-like frontend language (JVL), an ARM-like assembly backend (JVAV), a virtual machine, and a disassembler.

[中文版本](./README_zh.md)

---

## Compile & Run

### Traditional pipeline

```bash
# Frontend: .jvl -> .jvav
./jvlc hello.jvl hello.jvav

# Backend: .jvav -> .bin
./jvavc hello.jvav hello.bin

# Run
./jvm hello.bin
```

### One-shot compile & run

```bash
# Compile .jvl -> .jvav -> .bin and execute, then clean up intermediates
./jvlc --run hello.jvl

# Compile & run, keeping the final binary
./jvlc --run hello.jvl -o hello.bin
```

### Multi-file Linking

```bash
./jvavc math.jvav main.jvav -o program.bin
./jvm program.bin
```

---

## Packaging

```bash
cpack -C Release
```

Generates platform-specific packages (`JVAV-x.x.x-win64.zip`, `JVAV-x.x.x-Linux.tar.gz`, etc.).

---

## Project Structure

```
JVAV/
├── jvavc/
│   ├── front/       # Frontend compiler (.jvl -> .jvav), C++17
│   ├── back/        # Backend assembler & linker (.jvav -> .bin), C++17
│   └── tools/       # Disassembler (static + trace), C99
├── jvm/             # Virtual machine executor, C99
├── std/             # Standard library (io, math, mem, string, file)
├── benchmark/       # Performance benchmark suite (Python)
├── tests/           # Automated tests (back: 130, front: 179, 21 snapshots)
│   └── snapshots/   # Expected codegen / diagnostic outputs for snapshot testing
└── docs/            # Detailed documentation (compiler/, JVM/)
```

---

## Key Features

- **128-bit instruction format** — Every instruction is a fixed 16-byte word, with arithmetic and bitwise operations (AND, OR, XOR, SHL, SHR, NOT)
- **JVL language** — C-like syntax with functions, variables, control flow, and `import` modules
- **JVAV assembly** — ARM-like textual assembly with pseudo-instructions
- **MimiWorld ownership** — Rust-inspired ownership, move, and borrow checking (`&x`, `&mut x`)
- **Standard library** — `std/io`, `std/math`, `std/mem`, `std/string` with auto-import path resolution
- **Multi-file linker** — Structured linking with EQU global collection and base address relocation
- **Import system** — Recursive module imports with duplicate detection
- **Disassembler** — Static disassembly and dynamic trace mode
- **Rust-style diagnostics** — Error codes, source snippets with context lines, and help messages
- **Cross-platform** — Linux, Windows & Android (x86, x64, ARM, ARM64); statically linked binaries; GitHub Actions CI with multi-arch matrix
- **PATH-ready** — `std/` directory ships alongside `bin/`; add `bin/` to PATH and use `jvlc`/`jvavc`/`jvm`/`disasm` from anywhere
- **Version flag** — All tools support `-v` / `--version`

---

## Documentation

All documentation lives in the `docs/` directory on GitHub (no separate website).

### Start here

- [docs/README.md](docs/README.md) — Documentation index and quick reference

### Compiler documentation

- [docs/compiler/index.md](docs/compiler/index.md) — Compiler architecture and toolchain pipeline
- **Frontend (JVL)**
  - [JVL Language Reference](docs/compiler/front/jvl_language.md) — Syntax, types, operators, control flow
  - [Type System](docs/compiler/front/type_system.md) — Copy/Move types, inference, compatibility
  - [MimiWorld Ownership](docs/compiler/front/ownership.md) — Ownership, borrow checking, error messages
  - [Built-in Functions](docs/compiler/front/builtin_functions.md) — I/O, heap, process control
  - [Import System](docs/compiler/front/import_system.md) — Module resolution and standard library
  - [Error Handling](docs/compiler/front/error_handling.md) — Diagnostic format and common errors
- **Backend (Assembler & Linker)**
  - [JVAV Assembly](docs/compiler/back/jvav_assembly.md) — Instructions, directives, labels, syntax
  - [Instruction Encoding](docs/compiler/back/instruction_encoding.md) — Binary format and encoding examples
  - [Directives](docs/compiler/back/directives.md) — Sections, symbols, data, `.syscall`
  - [Calling Convention](docs/compiler/back/calling_convention.md) — Argument passing, prologue/epilogue, registers
  - [Linker](docs/compiler/back/linker.md) — Multi-file linking and symbol resolution
  - [Pseudo-Instructions](docs/compiler/back/pseudo_instructions.md) — Expansion rules

### JVM documentation

- [Architecture](docs/JVM/architecture.md) — VM architecture, registers, instruction format
- [Instruction Set](docs/JVM/instruction_set.md) — Complete reference for all 30 instructions
- [Memory Model](docs/JVM/memory_model.md) — Memory layout, stack, heap, dynamic expansion
- [Syscalls](docs/JVM/syscalls.md) — System call reference and mailbox protocol
- [Execution Model](docs/JVM/execution_model.md) — Execution loop and runtime behavior

---

## Testing

```bash
# Run all tests via CTest
ctest --output-on-failure

# Or run individual test binaries directly
./test_back    # 130 backend unit + integration tests
./test_front   # 179 frontend unit + integration tests

# Update snapshots after intentional codegen changes
./test_front --update-snapshots
```

Tests cover:
- **Lexer**: keywords, numbers, strings, chars, comments, symbols, CRLF compatibility, errors
- **Parser**: all types, control flow, operators, precedence, borrows, imports, error recovery
- **Semantic analysis**: type inference, ownership, borrow conflicts, uninitialized variables, scope
- **Code generation**: prologue/epilogue, locals, calls, control flow, pointers, strings *(snapshot-tested: full assembly diff)*
- **Backend**: all instructions, EQU, .global/.extern, DB/DW/DT, #include, encoding, linking
- **Integration**: end-to-end compile & run for arithmetic, bitwise ops, control flow, heap, recursion, imports, global variables, standard library, file I/O, version flags, missing-stdlib diagnostics
- **Diagnostics**: Rust-style error messages with exact body + help text matching; context lines, caret position, first/last line edge cases *(snapshot-tested)*
- **Data-driven**: new codegen or diagnostic tests require only adding one row to a case table

---

## Contributors

| Name | Contribution |
|------|-------------|
| Dr.zhanghaoyang | Proposed the JVAV language |
| Dere | Initial implementation |
| Derry | Massive improvements based on the prototype (using Claude AI) |
| Claude AI | Assisted Derry in rapid development and improvements |
| Moonshot AI (Kimi) | Documentation assistance |

---

## License

MIT License
