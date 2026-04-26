# JVAV Assembly Syntax Reference

JVAV assembly is an ARM-like textual assembly language that compiles to a fixed 128-bit instruction format via `jvavc`.

---

## Registers

| Register | Index | Purpose | Callee-saved? |
|----------|-------|---------|---------------|
| R0–R3 | 0–3 | General purpose / function arguments | No |
| R4–R5 | 4–5 | General purpose / temporaries | No |
| R6 | 6 | Frame pointer (FP) | **Yes** |
| R7 | 7 | Reserved (backend temporary) | No |
| PC | 8 | Program counter | — |
| SP | 9 | Stack pointer | **Yes** |
| FLAGS | 10 | Flags register (ZF bit 0) | No |

**Register conventions:**
- R0–R3 are used for passing up to 4 function arguments
- R4–R5 are used by the backend for temporary values (e.g., expanding pseudo-instructions)
- R6 must be preserved across calls (standard frame pointer)
- R7 is reserved and should not be used in hand-written assembly

---

## Instruction Set

All instructions are 16 bytes (128 bits). The assembler `jvavc` encodes them directly.

### Arithmetic & Logic

| Instruction | Format | Description |
|-------------|--------|-------------|
| `MOV Rd, Rs` | `Rd = Rs` | Register move |
| `ADD Rd, Rs, Rt` | `Rd = Rs + Rt` | Addition |
| `SUB Rd, Rs, Rt` | `Rd = Rs - Rt` | Subtraction |
| `MUL Rd, Rs, Rt` | `Rd = Rs * Rt` | Multiplication |
| `DIV Rd, Rs, Rt` | `Rd = Rs / Rt` | Signed division |
| `MOD Rd, Rs, Rt` | `Rd = Rs % Rt` | Signed modulo |
| `AND Rd, Rs, Rt` | `Rd = Rs & Rt` | Bitwise AND |
| `OR Rd, Rs, Rt` | `Rd = Rs \| Rt` | Bitwise OR |
| `XOR Rd, Rs, Rt` | `Rd = Rs ^ Rt` | Bitwise XOR |
| `SHL Rd, Rs, Rt` | `Rd = Rs << (Rt & 0x7F)` | Shift left (low 7 bits) |
| `SHR Rd, Rs, Rt` | `Rd = Rs >> (Rt & 0x7F)` | Arithmetic shift right |
| `NOT Rd, Rs` | `Rd = ~Rs` | Bitwise NOT |

### Memory Access

| Instruction | Format | Description |
|-------------|--------|-------------|
| `LDR Rd, [Rs]` | `Rd = mem[Rs]` | Load from register address |
| `STR [Rs], Rd` | `mem[Rs] = Rd` | Store to register address |
| `LDR Rd, [imm]` | `Rd = mem[imm]` | Load from immediate address (**pseudo**) |
| `STR [imm], Rd` | `mem[imm] = Rd` | Store to immediate address (**pseudo**) |

**Pseudo-instruction expansion:** `LDR Rd, [imm]` and `STR [imm], Rd` are expanded by the assembler into:
```asm
LDI Rtemp, imm
LDR Rd, [Rtemp]    ; or STR [Rtemp], Rd
```
The assembler automatically allocates a temporary register (R4 or R5) that does not clash with the source register.

### Control Flow

| Instruction | Format | Description |
|-------------|--------|-------------|
| `CMP Rs, Rt` | `FLAGS = Rs - Rt` | Compare, sets ZF |
| `JMP label` | `PC = label` | Unconditional jump (**pseudo**) |
| `JZ label` | `if ZF` jump | Jump if zero |
| `JNZ label` | `if !ZF` jump | Jump if not zero |
| `JE label` | `if ZF` jump | Jump if equal (same as JZ) |
| `JNE label` | `if !ZF` jump | Jump if not equal |
| `JL label` | `if Rs < Rt` jump | Jump if less (signed) |
| `JG label` | `if Rs > Rt` jump | Jump if greater (signed) |
| `JLE label` | `if Rs <= Rt` jump | Jump if less or equal |
| `JGE label` | `if Rs >= Rt` jump | Jump if greater or equal |

**Pseudo-instruction expansion:** `JMP label` is expanded to `LDI Rtemp, label; JMP Rtemp` (2 instructions). Label jumps (`JZ/JNZ/CALL label`) are expanded to `LDI Rtemp, label; JZ Rtemp` (2 instructions).

### Stack & Functions

| Instruction | Format | Description |
|-------------|--------|-------------|
| `PUSH Rs` | `mem[--SP] = Rs` | Push register onto stack |
| `POP Rd` | `Rd = mem[SP++]` | Pop register from stack |
| `CALL label` | Save PC, jump | Function call (**pseudo**, 2 instructions) |
| `RET` | `PC = mem[SP++]` | Return from function |
| `LDI Rd, imm` | `Rd = imm` | Load 128-bit immediate |
| `HALT` | — | Stop execution |

---

## Directives

### Symbol Directives

```asm
.global name       ; Export symbol for linking
.extern name       ; Declare external symbol (defined in another file)
LABEL: EQU value   ; Constant definition (replaced by value during assembly)
```

### File Inclusion

```asm
#include "file.jvav"   ; Include another assembly file inline
```

### Data Definition

```asm
DB b1, b2, ...     ; Define byte data (each byte occupies one 128-bit word)
DW w1, w2, ...     ; Define 16-bit word data (each occupies one 128-bit word)
DT t1, t2, ...     ; Define 128-bit word data
```

**Important:** JVAV is **word-addressable**. `DB "abc"` stores 3 words, not a packed 3-byte value. Each character is stored in the low byte of its own 128-bit word:
```
addr+0: 0x00000000000000000000000000000061  ('a')
addr+1: 0x00000000000000000000000000000062  ('b')
addr+2: 0x00000000000000000000000000000063  ('c')
```

### System Call Directive

```asm
.syscall name, cmd_id, arg_count
```

Generates a callable function wrapper that:
1. Sets up a standard stack frame (PUSH FP; MOV FP, SP)
2. Saves register arguments R0–R3 to FP+2..FP+5
3. Loads arguments from the stack frame into the VM syscall mailbox
4. Writes `cmd_id` to mailbox `0xFFE0`
5. Reads the return value from `0xFFE4`
6. Tears down the frame and returns

**Constraints:**
- `arg_count` must be in range `0..3`
- The generated function is indistinguishable from a user function at the call site

Example:
```asm
    .syscall putint, 15, 1    ; 1-arg syscall: putint(x)
    .syscall putstr, 19, 2    ; 2-arg syscall: putstr(addr, len)
    .syscall exit, 18, 1      ; 1-arg syscall: exit(code)
```

---

## Function Calling Convention

### Caller responsibilities

1. Evaluate arguments left-to-right, `PUSH R0` each
2. Load up to 4 arguments from stack into `R0`–`R3` (reversed offset)
3. `CALL func`
4. After `CALL`, clean up arguments: `ADD SP, SP, N`

### Callee responsibilities

**Prologue:**
```asm
PUSH R6          ; save old frame pointer
MOV  R6, SP      ; establish new frame pointer
; optionally allocate locals: SUB SP, SP, locals
```

**Argument access:** Arguments are at `FP+2` (arg0), `FP+3` (arg1), etc.

**Epilogue:**
```asm
MOV SP, R6       ; deallocate locals
POP R6           ; restore old frame pointer
RET
```

### Stack layout at function entry

```
Address (relative to FP):
  FP+0: saved R6 (old frame pointer)
  FP+1: return address (pushed by CALL)
  FP+2: argument 0
  FP+3: argument 1
  FP+4: argument 2
  FP+5: argument 3
  ...
```

Local variables use **negative offsets** from FP (`FP-1`, `FP-2`, ...).

---

## Multi-file Linking

`jvavc` supports linking multiple `.jvav` files into a single binary:

```bash
jvavc main.jvav lib.jvav -o out.bin
```

Rules:
- One file must define `_start` (the entry point at address 0)
- Symbols marked `.global` are exported
- Symbols referenced but not defined trigger an error unless `.extern` is used
- `#include` is textual inclusion (happens at parse time)
- Multi-file linking is symbol-based (happens at link time)

---

## Entry Point

Every program must have a `_start` label:

```asm
    .global _start
_start:
    CALL main
    HALT
```

The `_start` label must be placed at address 0 so that execution begins there. When using the JVL frontend, `_start` is automatically generated before `main()`.

---

## Complete Example

```asm
; Print "Hi!" using the putstr syscall
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

---

## Legacy I/O Ports (Backward Compatible)

The VM still supports direct memory-mapped I/O for hand-written assembly:

| Port | Function |
|------|----------|
| `0xFFF0` | `putchar` — write low byte as ASCII character |
| `0xFFF2` | `putint` — write signed integer as decimal text |

These are implemented as fallbacks in the VM's `io_write` function. New code should use `.syscall` instead.
