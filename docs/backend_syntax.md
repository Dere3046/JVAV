# JVAV Assembly Syntax

JVAV assembly is an ARM-like textual assembly language that maps directly to the 128-bit JVAV instruction set.

## Registers

| Register | Index | Purpose |
|----------|-------|---------|
| R0–R5 | 0–5 | General purpose / function arguments |
| R6 | 6 | Frame pointer (FP) |
| R7 | 7 | Reserved (backend temporary) |
| PC | 8 | Program counter |
| SP | 9 | Stack pointer |
| FLAGS | 10 | Flags register (ZF, etc.) |

## Instruction Set

| Instruction | Format | Description |
|-------------|--------|-------------|
| `MOV Rd, Rs` | `Rd = Rs` | Register move |
| `LDR Rd, [Rs]` | `Rd = mem[Rs]` | Register indirect load |
| `LDR Rd, [imm]` | `Rd = mem[imm]` | Direct address load (pseudo) |
| `STR [Rs], Rd` | `mem[Rs] = Rd` | Register indirect store |
| `STR [imm], Rd` | `mem[imm] = Rd` | Direct address store (pseudo) |
| `ADD Rd, Rs, Rt` | `Rd = Rs + Rt` | Addition |
| `SUB Rd, Rs, Rt` | `Rd = Rs - Rt` | Subtraction |
| `MUL Rd, Rs, Rt` | `Rd = Rs * Rt` | Multiplication |
| `DIV Rd, Rs, Rt` | `Rd = Rs / Rt` | Division |
| `MOD Rd, Rs, Rt` | `Rd = Rs % Rt` | Modulo |
| `CMP Rs, Rt` | `FLAGS = Rs - Rt` | Compare |
| `JMP label` | `PC = label` | Unconditional jump (pseudo) |
| `JZ label` | `if ZF` jump | Jump if zero |
| `JNZ label` | `if !ZF` jump | Jump if not zero |
| `JE/JNE/JL/JG/JLE/JGE label` | condition | Extended conditional jumps |
| `PUSH Rs` | `mem[--SP] = Rs` | Push |
| `POP Rd` | `Rd = mem[SP++]` | Pop |
| `CALL label` | Save PC, jump | Function call (pseudo) |
| `RET` | `PC = mem[SP++]` | Return |
| `LDI Rd, imm` | `Rd = imm` | Load immediate |
| `HALT` | — | Halt execution |

## Pseudo-instructions & Data

```asm
.global name      ; export symbol
.extern name      ; declare external symbol
LABEL: EQU value  ; constant definition
#include "file"   ; file inclusion
DB b1, b2, ...    ; define byte data (word-aligned)
DW w1, w2, ...    ; define word data
DT t1, t2, ...    ; define 128-bit data
```

## Example

```asm
    .global _start
_start:
    LDI R0, msg
    LDI R1, 14
    LDI R2, 1
    LDI R4, 0
    LDI R6, 0xFFF0      ; putchar port
pstr:
    LDR R7, [R0]
    STR [R6], R7
    ADD R0, R0, R2
    SUB R1, R1, R2
    CMP R1, R4
    JNZ pstr
    HALT
msg: DB 72,101,108,108,111,44,32,87,111,114,108,100,33,10
```
