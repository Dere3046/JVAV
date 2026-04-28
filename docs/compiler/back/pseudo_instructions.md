# JVAV Pseudo-Instructions

Pseudo-instructions are assembly syntax conveniences that the assembler expands into one or more real instructions. They make assembly code more readable and easier to write.

## List of Pseudo-Instructions

### JMP label

**Syntax**: `JMP label`

**Expansion**:
```asm
LDI Rtemp, label
JMP Rtemp
```

**Description**: Unconditional jump to a label. The assembler loads the label's address into a temporary register (R4 or R5) and then performs a register-based jump.

**Example**:
```asm
    JMP loop
; Expands to:
;   LDI R4, loop
;   JMP R4

loop:
    ...
```

---

### CALL label

**Syntax**: `CALL label`

**Expansion**:
```asm
LDI Rtemp, label
CALL Rtemp
```

**Description**: Function call to a label. Loads the label's address into a temporary register, then performs a register-based call.

**Example**:
```asm
    CALL factorial
; Expands to:
;   LDI R4, factorial
;   CALL R4
```

---

### JZ label / JNZ label / JE label / JNE label / JL label / JG label / JLE label / JGE label

**Syntax**: `JZ label`, `JNZ label`, etc.

**Expansion**:
```asm
LDI Rtemp, label
JZ Rtemp     ; or JNZ, JE, JNE, JL, JG, JLE, JGE
```

**Description**: Conditional jumps to a label. Loads the label's address into a temporary register, then performs the conditional jump.

**Example**:
```asm
    CMP R0, R1
    JE equal_label
; Expands to:
;   LDI R4, equal_label
;   JE R4

equal_label:
    ...
```

---

### LDR Rd, [imm]

**Syntax**: `LDR Rd, [immediate]`

**Expansion**:
```asm
LDI Rtemp, immediate
LDR Rd, [Rtemp]
```

**Description**: Load from an immediate address. The `LDR` instruction only supports register addressing in hardware, so the assembler loads the immediate into a temporary register first.

**Example**:
```asm
    LDR R0, [0x1000]
; Expands to:
;   LDI R4, 0x1000
;   LDR R0, [R4]
```

---

### STR [imm], Rd

**Syntax**: `STR [immediate], Rd`

**Expansion**:
```asm
LDI Rtemp, immediate
STR [Rtemp], Rd
```

**Description**: Store to an immediate address. Like `LDR`, the hardware only supports register addressing, so the immediate is loaded into a temporary first.

**Example**:
```asm
    STR [0x2000], R0
; Expands to:
;   LDI R4, 0x2000
;   STR [R4], R0
```

## Temporary Register Selection

The assembler automatically selects R4 or R5 as the temporary register. The selection algorithm ensures:

1. The temporary register does not clash with the destination register
2. R7 is never used (it is reserved for backend use)
3. If both R4 and R5 would clash, the assembler chooses the one that doesn't

**Example**:
```asm
LDR R4, [0x1000]   ; Uses R5 as temp (R4 is destination)
LDR R5, [0x1000]   ; Uses R4 as temp (R5 is destination)
LDR R0, [0x1000]   ; Uses R4 as temp (default)
```

## Size Impact

Each pseudo-instruction expands to 2 real instructions (32 bytes):

| Pseudo | Real Instructions | Size |
|--------|------------------|------|
| `JMP label` | `LDI Rtemp, label; JMP Rtemp` | 32 bytes |
| `CALL label` | `LDI Rtemp, label; CALL Rtemp` | 32 bytes |
| `JZ label` | `LDI Rtemp, label; JZ Rtemp` | 32 bytes |
| `LDR Rd, [imm]` | `LDI Rtemp, imm; LDR Rd, [Rtemp]` | 32 bytes |
| `STR [imm], Rd` | `LDI Rtemp, imm; STR [Rtemp], Rd` | 32 bytes |

When calculating code size or offsets, account for the expansion. A label jump is always 2 instructions (32 bytes), not 1.

## Address Calculation

Because pseudo-instructions expand to multiple real instructions, label addresses must be computed after expansion:

```asm
        JMP target      ; 2 instructions (address 0-1)
        NOP             ; 1 instruction (address 2)
target:                 ; address 3
```

The assembler's `computeAddresses()` method walks through all instructions (including expanded pseudo-instructions) and assigns final addresses to labels.

## When to Use Pseudo-Instructions

Use pseudo-instructions for:
- Readability (labels are more meaningful than raw addresses)
- Convenience (avoid manual temporary register management)
- Portability (labels are resolved at assembly time)

Avoid pseudo-instructions when:
- Writing performance-critical code (2x instruction count)
- Space is constrained (32 bytes vs 16 bytes)
- You need precise control over register allocation

## Manual Expansion

For maximum control, you can manually expand pseudo-instructions:

```asm
; Pseudo version
    JMP loop

; Manual expansion
    LDI R4, loop
    JMP R4
```

Manual expansion is rarely necessary but may be useful when:
- R4 and R5 are both occupied
- You want to use a specific temporary register
- You're optimizing critical code paths
