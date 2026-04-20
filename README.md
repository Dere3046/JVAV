# JVAV Programming Language

> **Disclaimer:** This is a joke / parody project. JVAV was "proposed by Dr. Zhang Haoyang" and implemented by **Dere3046**. Please do not harass, attack, or send hate to Zhang Haoyang or anyone associated with JVAV. This project exists purely for fun and educational purposes.

JVAV is a complete custom programming language and toolchain built from scratch, featuring a 128-bit instruction set architecture, a C-like frontend language (JVL), an ARM-like assembly backend (JVAV), a virtual machine, and a disassembler.

[中文版本](./README_zh.md)

---

## Quick Start

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make -j$(nproc)
```

### Compile & Run

```bash
# Frontend: .jvl -> .jvav
./jvlc hello.jvl hello.jvav

# Backend: .jvav -> .bin
./jvavc hello.jvav hello.bin

# Run
./jvm hello.bin
```

### Multi-file Linking

```bash
./jvavc math.jvav main.jvav -o program.bin
./jvm program.bin
```

---

## Project Structure

```
JVAV/
├── jvavc/
│   ├── front/       # Frontend compiler (.jvl -> .jvav), C++17
│   ├── back/        # Backend assembler & linker (.jvav -> .bin), C++17
│   └── tools/       # Disassembler (static + trace), C99
├── jvm/             # Virtual machine executor, C99
├── tests/           # Automated tests (back: 12, front: 25)
└── docs/            # Detailed documentation
```

---

## Key Features

- **128-bit instruction format** — Every instruction is a fixed 16-byte word
- **JVL language** — C-like syntax with functions, variables, control flow, and `import` modules
- **JVAV assembly** — ARM-like textual assembly with pseudo-instructions
- **MimiWorld ownership** — Rust-inspired ownership, move, and borrow checking (`&x`, `&mut x`)
- **Multi-file linker** — Structured linking with EQU global collection and base address relocation
- **Import system** — Recursive module imports with cyclic import guard
- **Disassembler** — Static disassembly and dynamic trace mode

---

## Documentation

| Document | Description |
|----------|-------------|
| [docs/frontend_syntax.md](docs/frontend_syntax.md) | JVL language syntax, types, ownership |
| [docs/backend_syntax.md](docs/backend_syntax.md) | JVAV assembly syntax and instruction set |
| [docs/jvm_features.md](docs/jvm_features.md) | VM architecture, memory model, syscalls |
| [docs/mimiworld_ownership.md](docs/mimiworld_ownership.md) | MimiWorld ownership & borrow checker |

---

## Testing

```bash
./test_back    # 13 backend tests
./test_front   # 28 frontend tests
```

---

## License

MIT License
