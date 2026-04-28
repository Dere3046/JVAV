# JVAV Assembly Language Reference

JVAV assembly is an ARM-like textual assembly language that compiles to the JVAV Virtual Machine's fixed 128-bit instruction format. It is the target language of the JVL frontend compiler and can also be written by hand.

## Program Structure

A JVAV assembly program consists of:

1. Optional `.data` and `.text` section directives
2. Labels and instructions
3. Directives (`.global`, `.extern`, `.syscall`, etc.)
4. Data definitions (`DB`, `DW`, `DT`)

```asm
    .data
msg:    DB "Hello", 0

    .text
    .global _start
_start:
    LDI R0, msg
    LDI R1, 5
    PUSH R1
    PUSH R0
    CALL putstr
    LDI R4, 2
    ADD SP, SP, R4
    HALT

    .syscall putstr, 19, 2
```

## Registers

| Register | Index | Purpose | Callee-saved |
|----------|-------|---------|--------------|
| R0 | 0 | General purpose / arguments / return value | No |
| R1–R3 | 1–3 | General purpose / arguments | **Yes** |
| R4–R5 | 4–5 | General purpose / temporaries | No |
| R6 | 6 | Frame pointer (FP) | **Yes** |
| R7 | 7 | Reserved (backend temporary) | No |
| PC | 8 | Program counter | — |
| SP | 9 | Stack pointer | **Yes** |
| FLAGS | 10 | Flags register (comparison result) | No |

### Register usage conventions

- **R0**: Used for passing argument 0 and return values. Caller-saved.
- **R1–R3**: Used for passing arguments 1–3. Callee-saved.
- **R4–R5**: Used by the assembler backend for expanding pseudo-instructions. May be clobbered.
- **R6 (FP)**: Frame pointer. Must be preserved across calls.
- **R7**: Reserved. Hand-written assembly should avoid using R7.
- **PC, SP, FLAGS**: Managed by the VM and control flow instructions.

## Instructions

### Arithmetic and logic

| Instruction | Operation | Description |
|-------------|-----------|-------------|
| `MOV Rd, Rs` | `Rd = Rs` | Register move |
| `ADD Rd, Rs, Rt` | `Rd = Rs + Rt` | Addition |
| `SUB Rd, Rs, Rt` | `Rd = Rs - Rt` | Subtraction |
| `MUL Rd, Rs, Rt` | `Rd = Rs * Rt` | Multiplication |
| `DIV Rd, Rs, Rt` | `Rd = Rs / Rt` | Signed division |
| `MOD Rd, Rs, Rt` | `Rd = Rs % Rt` | Signed modulo |
| `AND Rd, Rs, Rt` | `Rd = Rs & Rt` | Bitwise AND |
| `OR Rd, Rs, Rt` | `Rd = Rs \| Rt` | Bitwise OR |
| `XOR Rd, Rs, Rt` | `Rd = Rs ^ Rt` | Bitwise XOR |
| `SHL Rd, Rs, Rt` | `Rd = Rs << (Rt & 0x7F)` | Shift left |
| `SHR Rd, Rs, Rt` | `Rd = Rs >> (Rt & 0x7F)` | Shift right (arithmetic) |
| `NOT Rd, Rs` | `Rd = ~Rs` | Bitwise NOT |

### Memory access

| Instruction | Operation | Description |
|-------------|-----------|-------------|
| `LDR Rd, [Rs]` | `Rd = mem[Rs]` | Load from register address |
| `STR [Rs], Rd` | `mem[Rs] = Rd` | Store to register address |
| `LDR Rd, [imm]` | `Rd = mem[imm]` | Load from immediate (pseudo) |
| `STR [imm], Rd` | `mem[imm] = Rd` | Store to immediate (pseudo) |

### Control flow

| Instruction | Operation | Description |
|-------------|-----------|-------------|
| `CMP Rs, Rt` | `FLAGS = (Rs==Rt)?1:(Rs<Rt)?2:0` | Compare |
| `JMP label` | `PC = label` | Unconditional jump (pseudo) |
| `JZ label` | `if (FLAGS==1) PC = label` | Jump if zero / equal |
| `JNZ label` | `if (FLAGS!=1) PC = label` | Jump if not zero / not equal |
| `JE label` | `if (FLAGS==1) PC = label` | Jump if equal |
| `JNE label` | `if (FLAGS!=1) PC = label` | Jump if not equal |
| `JL label` | `if (FLAGS==2) PC = label` | Jump if less |
| `JG label` | `if (FLAGS==0) PC = label` | Jump if greater |
| `JLE label` | `if (FLAGS==1||FLAGS==2) PC = label` | Jump if less or equal |
| `JGE label` | `if (FLAGS==1||FLAGS==0) PC = label` | Jump if greater or equal |

### Stack and functions

| Instruction | Operation | Description |
|-------------|-----------|-------------|
| `PUSH Rs` | `mem[--SP] = Rs` | Push onto stack |
| `POP Rd` | `Rd = mem[SP++]` | Pop from stack |
| `CALL label` | `mem[--SP] = PC; PC = label` | Function call (pseudo) |
| `RET` | `PC = mem[SP++]` | Return from function |
| `LDI Rd, imm` | `Rd = imm` | Load 128-bit immediate |
| `HALT` | — | Stop execution |

## Labels

Labels define symbolic names for memory addresses:

```asm
loop:
    CMP R0, R1
    JE done
    ADD R0, R0, R2
    JMP loop
done:
    HALT
```

Label names are case-sensitive and must be unique within a file (or within the linked program for `.global` symbols).

Labels can be suffixed with `:` to mark their position:

```asm
start:
    LDI R0, 42
```

The colon is optional in some contexts but recommended for clarity.

## Constants (EQU)

`EQU` defines compile-time constants:

```asm
PI: EQU 314159
    LDI R0, PI      ; R0 = 314159
```

EQU symbols are replaced with their literal value during assembly. They do not occupy memory.

## Sections

### .data

Switches to the data section for constants and initialized data:

```asm
    .data
msg:    DB "Hello", 0
arr:    DT 1, 2, 3, 4, 5
```

### .text

Switches to the code section (default):

```asm
    .text
_start:
    CALL main
    HALT
```

The assembler tracks which section data is placed in. Frontend codegen automatically switches to `.data` for string literals at the end of output.

## Data Definition

### DB (Define Byte)

Defines byte data. Each byte occupies one 128-bit word:

```asm
msg: DB "ABC", 0
```

Memory layout:
```
addr+0: 0x000...0041  ('A')
addr+1: 0x000...0042  ('B')
addr+2: 0x000...0043  ('C')
addr+3: 0x000...0000  (null terminator)
```

### DW (Define Word)

Defines 16-bit word data. Each value occupies one 128-bit word:

```asm
vals: DW 100, 200, 300
```

### DT (Define Triple/128-bit Word)

Defines 128-bit word data:

```asm
big: DT 0x123456789ABCDEF, 42
```

### Important notes

- JVAV is **word-addressable**: `DB "abc"` stores 3 words, not a packed 3-byte value
- Each character is stored in the low byte of its own 128-bit word
- Strings for syscalls that use `read_vm_string()` must be null-terminated with `, 0`
- Colons inside unquoted data may be misinterpreted as label separators; always quote strings containing colons

## Symbol Directives

### .global

Exports a symbol for linking:

```asm
    .global _start
    .global my_function
```

### .extern

Declares an external symbol defined in another file:

```asm
    .extern external_func
```

Without `.extern`, referencing an undefined label causes a linker error.

### .syscall

Generates a syscall wrapper function:

```asm
    .syscall putint, 15, 1
    .syscall putstr, 19, 2
```

The wrapper:
1. Sets up a stack frame
2. Saves arguments to the frame
3. Loads arguments into syscall mailbox (`0xFFE1`–`0xFFE3`)
4. Writes command ID to `0xFFE0`
5. Reads return value from `0xFFE4` into R0
6. Tears down the frame and returns

Constraints:
- `arg_count` must be `0..3`
- The generated function is automatically marked `.global`

## File Inclusion

```asm
#include "file.jvav"
#include <file.jvav>
```

`#include` is a **textual preprocessor directive**. The included file's contents are inserted verbatim at the inclusion point.

Duplicate includes of the same file are silently deduplicated by canonical path.

Include resolution:
1. Relative to the including file's directory
2. Relative to the current working directory

**Important**: `#include` happens at parse time, before label resolution. Multi-file linking (passing multiple `.jvav` files to `jvavc`) happens at link time and uses symbol tables instead.

## Complete Example

```asm
; Print "Hi!" using putstr syscall
    .global _start
_start:
    LDI R0, msg        ; R0 = address of string
    LDI R1, 3          ; R1 = length
    PUSH R1            ; push arg1 (len)
    PUSH R0            ; push arg0 (addr)
    CALL putstr
    LDI R4, 2
    ADD SP, SP, R4     ; clean up 2 arguments
    HALT

msg:
    DB "Hi!"

    .syscall putstr, 19, 2
    .syscall putint, 15, 1
```

## Assembly Syntax Rules

1. Instructions are case-insensitive (but conventionally uppercase)
2. Labels are case-sensitive
3. Comments start with `;` and extend to end of line
4. Registers are specified as `R0` through `R7`, `PC`, `SP`, `FLAGS`
5. Immediate values can be decimal, hexadecimal (`0x`), or binary (`0b`)
6. Labels in jump instructions are resolved at assembly/link time
