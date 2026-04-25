# JVAV Programming Language

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://github.com/Dere3046/JVAV/actions/workflows/ci.yml/badge.svg)](https://github.com/Dere3046/JVAV/actions)
[![Stars](https://img.shields.io/github/stars/Dere3046/JVAV?style=social)](https://github.com/Dere3046/JVAV)
![JVAV](https://img.shields.io/badge/JVAV-128--bit-ff69b4.svg)
![C](https://img.shields.io/badge/C-99-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)

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
├── std/             # Standard library (io, math, mem, string)
├── benchmark/       # Performance benchmark suite (Python)
├── tests/           # Automated tests (back: 90, front: 100)
└── docs/            # Detailed documentation
```

---

## Key Features

- **128-bit instruction format** — Every instruction is a fixed 16-byte word
- **JVL language** — C-like syntax with functions, variables, control flow, and `import` modules
- **JVAV assembly** — ARM-like textual assembly with pseudo-instructions
- **MimiWorld ownership** — Rust-inspired ownership, move, and borrow checking (`&x`, `&mut x`)
- **Standard library** — `std/io`, `std/math`, `std/mem`, `std/string` with auto-import path resolution
- **Multi-file linker** — Structured linking with EQU global collection and base address relocation
- **Import system** — Recursive module imports with cyclic import guard
- **Disassembler** — Static disassembly and dynamic trace mode
- **Rust-style diagnostics** — Error codes, source snippets, and help messages
- **Cross-platform** — Linux, Windows (MinGW), macOS; GitHub Actions CI for all three

---

## Documentation

| Document | Description |
|----------|-------------|
| [docs/frontend_syntax.md](docs/frontend_syntax.md) | JVL language syntax, type system, inference, ownership |
| [docs/backend_syntax.md](docs/backend_syntax.md) | JVAV assembly syntax and instruction set |
| [docs/jvm_features.md](docs/jvm_features.md) | VM architecture, memory model, syscalls |
| [docs/mimiworld_ownership.md](docs/mimiworld_ownership.md) | MimiWorld ownership & borrow checker |
| [docs/stdlib.md](docs/stdlib.md) | Standard library API and usage |

---

## Testing

```bash
# Run all tests via CTest
ctest --output-on-failure

# Or run individual test binaries directly
./test_back    # 90 backend unit + integration tests
./test_front   # 100 frontend unit + integration tests
```

Tests cover:
- **Lexer**: keywords, numbers, strings, chars, comments, symbols, CRLF compatibility, errors
- **Parser**: all types, control flow, operators, precedence, borrows, imports, error recovery
- **Semantic analysis**: type inference, ownership, borrow conflicts, uninitialized variables, scope
- **Code generation**: prologue/epilogue, locals, calls, control flow, pointers, strings
- **Backend**: all instructions, EQU, .global/.extern, DB/DW/DT, #include, encoding, linking
- **Integration**: end-to-end compile & run for arithmetic, control flow, heap, recursion, imports, global variables, standard library

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
