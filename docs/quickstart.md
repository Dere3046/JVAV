# JVAV Quick Start Guide

This guide gets you from zero to a running JVAV program in 5 minutes.

## Installation

### Prerequisites

- **Windows**: MinGW-w64 GCC 14+ or MSVC (not recommended)
- **Linux**: GCC 14+ or Clang 16+
- **CMake** 3.16+

### Build

```bash
git clone https://github.com/Dere3046/JVAV.git
cd JVAV
cmake -S . -B build -G "MinGW Makefiles"   # Windows
cmake -S . -B build                         # Linux / macOS
cmake --build build -j4
```

Binaries appear in `build/bin/`:
- `jvlc` — frontend compiler (.jvl → .jvav)
- `jvavc` — backend assembler (.jvav → .bin)
- `jvm` — virtual machine executor
- `disasm` — static disassembler + trace tool

### PATH Setup (optional)

```bash
export PATH="$PWD/build/bin:$PATH"
```

The `std/` directory must be accessible from the compiler. It is automatically found if you run `jvlc` from the project root or if `std/` is placed next to the `jvlc` executable.

---

## Your First Program

Create `hello.jvl`:

```jvl
import "std/io.jvl";

func main(): int {
    println(42);
    return 0;
}
```

Compile and run:

```bash
# One-shot compile & run (cleans up intermediates)
jvlc --run hello.jvl

# Or step by step
jvlc hello.jvl hello.jvav
jvavc hello.jvav hello.bin
jvm hello.bin
```

Output:
```
42
```

---

## Using the Heap

```jvl
func main(): int {
    var p: ptr<int> = alloc(3);
    p[0] = 10;
    p[1] = 20;
    p[2] = 30;
    putint(p[0] + p[1] + p[2]);   // 60
    free(p);
    return 0;
}
```

---

## File I/O

```jvl
import "std/file.jvl";
import "std/io.jvl";

func main(): int {
    var path = "output.txt";
    var fd = fopen(path, "wb");
    if (fd == 0) {
        println(999);
        return 1;
    }
    var data = alloc(3);
    data[0] = 65; data[1] = 66; data[2] = 67;   // ABC
    fwrite(fd, data, 3);
    fclose(fd);
    free(data);
    return 0;
}
```

---

## Common Issues

### "cannot find standard library `std/io.jvl`"

Run `jvlc` from the project root directory, or copy the `std/` folder next to the `jvlc` binary.

### `jvm hello.bin` says "Cannot open bytecode file"

The `.bin` file path is relative to your shell's current directory, not the source file's directory.

### Chinese characters display as garbled text

On Windows, the tools automatically set UTF-8 console code page. If you still see garbled text, run `chcp 65001` manually before executing.

---

## Next Steps

- [frontend_syntax.md](frontend_syntax.md) — JVL language reference
- [backend_syntax.md](backend_syntax.md) — JVAV assembly reference
- [jvm_features.md](jvm_features.md) — VM architecture & syscalls
- [mimiworld_ownership.md](mimiworld_ownership.md) — Ownership & borrow checker
- [stdlib.md](stdlib.md) — Standard library API
