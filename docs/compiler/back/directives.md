# JVAV Assembler Directives

Assembler directives are commands to the assembler that control the assembly process, define data, manage symbols, and generate special code sequences.

## Section Directives

### .data

Switches to the data section for defining constants and initialized data.

```asm
    .data
msg:    DB "Hello", 0
arr:    DT 1, 2, 3
```

Data placed after `.data` goes into the data segment, which is part of the loaded binary. The frontend compiler automatically switches to `.data` for string literals at the end of generated assembly.

### .text

Switches to the code section (default).

```asm
    .text
_start:
    CALL main
    HALT
```

All instructions and labels emitted after `.text` go into the executable code region.

## Symbol Directives

### .global

Exports a symbol, making it available for linking with other files.

```asm
    .global _start
    .global my_function
    .global my_data
```

Rules:
- At least one file in a linked program must define `_start` as `.global`
- `.global` symbols can be referenced from other files without `.extern`
- A symbol can be both defined and marked `.global` in the same file

### .extern

Declares an external symbol that is defined in another file.

```asm
    .extern printf
    .extern external_data
```

Rules:
- Symbols referenced but not defined in a file should be declared `.extern`
- Without `.extern`, undefined labels cause linker errors
- `.extern` does not allocate space or define a value; it only informs the linker

### EQU

Defines a compile-time constant. EQU symbols are replaced with their literal value during assembly.

```asm
PI:     EQU 314159
MAX_LEN: EQU 1024

    LDI R0, PI          ; R0 = 314159
    LDI R1, MAX_LEN     ; R1 = 1024
```

Rules:
- EQU symbols do not occupy memory
- They can be used anywhere an immediate value is expected
- They are resolved at assembly time, not runtime
- EQU symbols from one file are not automatically visible in linked files unless exported

## Data Definition Directives

### DB (Define Byte)

Defines byte-sized data. Each value occupies one 128-bit word.

```asm
msg:    DB "H", "e", "l", "l", "o", 0
path:   DB "data.txt", 0
```

Memory layout for `DB "A", "B", 0`:
```
Address + 0: 0x00000000000000000000000000000041  ('A')
Address + 1: 0x00000000000000000000000000000042  ('B')
Address + 2: 0x00000000000000000000000000000000  (null)
```

**Important notes**:
- Strings must be quoted to avoid colon/slash parsing issues
- Null terminators are required for syscalls that use `read_vm_string()`
- Each character uses one full word (16 bytes)

### DW (Define Word)

Defines 16-bit word data. Each value occupies one 128-bit word.

```asm
vals:   DW 100, 200, 300
```

Memory layout:
```
Address + 0: 0x00000000000000000000000000000064  (100)
Address + 1: 0x000000000000000000000000000000C8  (200)
Address + 2: 0x0000000000000000000000000000012C  (300)
```

### DT (Define 128-bit Word)

Defines full 128-bit word data.

```asm
big:    DT 0x123456789ABCDEF, 42
```

Memory layout:
```
Address + 0: 0x0000000000000000000123456789ABCDEF
Address + 1: 0x0000000000000000000000000000002A  (42)
```

## Special Directives

### .syscall

Generates a callable syscall wrapper function.

```asm
    .syscall name, cmd_id, arg_count
```

The generated wrapper:
1. Pushes the old frame pointer and saves R1–R3 (callee-saved)
2. Establishes a new frame pointer
3. Loads arguments from the stack frame (FP+5 through FP+8) into the syscall mailbox (`0xFFE1`–`0xFFE3`)
4. Writes `cmd_id` to mailbox `0xFFE0`
5. Reads the return value from `0xFFE4` into R0
6. Restores R3, R2, R1, the old frame pointer, and returns

**Constraints**:
- `arg_count` must be in range `0..3`
- The wrapper is automatically marked `.global`
- The generated function is indistinguishable from a user function at the call site

**Example**:
```asm
    .syscall putint, 15, 1     ; putint(x)
    .syscall putstr, 19, 2     ; putstr(addr, len)
    .syscall exit, 18, 1       ; exit(code)
    .syscall fopen, 4, 2       ; fopen(path, mode)
    .syscall fwrite, 7, 3      ; fwrite(fd, buf, count)
```

**Multi-arg syscall usage**:
```asm
    LDI R0, path
    LDI R1, mode
    PUSH R1
    PUSH R0
    CALL fopen
    LDI R4, 2
    ADD SP, SP, R4       ; clean up 2 args

path: DB "test.txt", 0
mode: DB "wb", 0

    .syscall fopen, 4, 2
```

### #include

Textually includes another assembly file.

```asm
#include "lib/utils.jvav"
#include <std/macros.jvav>
```

Rules:
- The included file's contents are inserted verbatim at the inclusion point
- Duplicate includes are silently deduplicated by canonical path
- Include resolution: relative to the including file's directory, then the current working directory
- `#include` happens at parse time, before label resolution
- Not to be confused with multi-file linking (which happens at link time)

## Directive Summary

| Directive | Purpose |
|-----------|---------|
| `.data` | Switch to data section |
| `.text` | Switch to code section |
| `.global name` | Export symbol for linking |
| `.extern name` | Declare external symbol |
| `LABEL: EQU value` | Define compile-time constant |
| `DB v1, v2, ...` | Define byte data (1 byte per word) |
| `DW v1, v2, ...` | Define 16-bit word data (1 word per value) |
| `DT v1, v2, ...` | Define 128-bit word data |
| `.syscall name, id, args` | Generate syscall wrapper |
| `#include "file"` | Textually include file |

## Common Pitfalls

### Forgetting null terminators

Syscalls like `SYS_FOPEN` use `read_vm_string()`, which stops at the first zero byte:

```asm
; WRONG — path may include garbage bytes
path: DB "data.txt"

; CORRECT — explicitly null-terminated
path: DB "data.txt", 0
```

### Confusing #include with linking

```asm
; Textual inclusion (parse time)
#include "lib.jvav"

; Symbol-based linking (link time)
; jvavc main.jvav lib.jvav -o out.bin
```

Use `#include` for shared constants and macros. Use multi-file linking for separate compilation units.

### Colons in unquoted data

The assembler treats `:` as a label suffix. If your data contains a colon outside quotes, it will be misinterpreted:

```asm
; WRONG — parsed as label "DB \"A" with data "B"
msg: DB "A:B", 0

; CORRECT — the entire string is quoted
msg: DB "Error: failed", 0
```
