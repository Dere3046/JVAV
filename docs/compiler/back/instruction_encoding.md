# JVAV Instruction Encoding

The JVAV backend assembler (`jvavc`) translates textual assembly instructions into a fixed 16-byte (128-bit) binary format. This document describes the encoding process in detail.

## Binary Format

Each instruction is exactly 16 bytes, stored in little-endian byte order:

```
Byte offset:
  0:  op      (8 bits)
  1:  dst     (8 bits)
  2:  src1    (8 bits)
  3:  src2    (8 bits)
  4-11: imm_low  (64 bits, little-endian)
  12-15: imm_high (32 bits, little-endian)
```

### Field descriptions

| Field | Size | Byte Offset | Description |
|-------|------|-------------|-------------|
| `op` | 8 bits | 0 | Opcode identifier |
| `dst` | 8 bits | 1 | Destination register index |
| `src1` | 8 bits | 2 | First source register index |
| `src2` | 8 bits | 3 | Second source register index |
| `imm_low` | 64 bits | 4–11 | Lower immediate bits |
| `imm_high` | 32 bits | 12–15 | Upper immediate bits |

## Immediate Value Reconstruction

The VM reconstructs the 128-bit immediate as:

```c
var imm = ((var)(int32_t)instr.imm_high << 64) | instr.imm_low;
```

The `imm_high` field is treated as a signed 32-bit integer and sign-extended to 128 bits before being shifted left by 64. This allows encoding any 96-bit signed integer directly.

**Example**:
```
imm_low  = 0x000000000000002A  (42)
imm_high = 0x00000000
Result:   0x0000000000000000000000000000002A  (42)

imm_low  = 0xFFFFFFFFFFFFFFFF
imm_high = 0xFFFFFFFF
Result:   0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF  (-1, sign-extended)
```

## Register Encoding

Registers are encoded as 8-bit indices:

| Register | Index | Encoding |
|----------|-------|----------|
| R0 | 0 | `0x00` |
| R1 | 1 | `0x01` |
| ... | ... | ... |
| R7 | 7 | `0x07` |
| PC | 8 | `0x08` |
| SP | 9 | `0x09` |
| FLAGS | 10 | `0x0A` |

## Opcode Values

| Instruction | Opcode (hex) | Opcode (dec) |
|-------------|--------------|--------------|
| HALT | `0x00` | 0 |
| MOV | `0x01` | 1 |
| LDR | `0x02` | 2 |
| STR | `0x03` | 3 |
| ADD | `0x04` | 4 |
| SUB | `0x05` | 5 |
| MUL | `0x06` | 6 |
| DIV | `0x07` | 7 |
| CMP | `0x08` | 8 |
| JMP | `0x09` | 9 |
| JZ | `0x0A` | 10 |
| JNZ | `0x0B` | 11 |
| PUSH | `0x0C` | 12 |
| POP | `0x0D` | 13 |
| CALL | `0x0E` | 14 |
| RET | `0x0F` | 15 |
| LDI | `0x10` | 16 |
| JE | `0x11` | 17 |
| JNE | `0x12` | 18 |
| JL | `0x13` | 19 |
| JG | `0x14` | 20 |
| JLE | `0x15` | 21 |
| JGE | `0x16` | 22 |
| MOD | `0x17` | 23 |
| AND | `0x18` | 24 |
| OR | `0x19` | 25 |
| XOR | `0x1A` | 26 |
| SHL | `0x1B` | 27 |
| SHR | `0x1C` | 28 |
| NOT | `0x1D` | 29 |

## Encoding Examples

### ADD R0, R1, R2

```
op   = 0x04  (ADD)
dst  = 0x00  (R0)
src1 = 0x01  (R1)
src2 = 0x02  (R2)
imm_low  = 0x0000000000000000
imm_high = 0x00000000

Binary (hex): 04 00 01 02 00 00 00 00 00 00 00 00 00 00 00 00
```

### LDI R0, 42

```
op   = 0x10  (LDI)
dst  = 0x00  (R0)
src1 = 0x00
src2 = 0x00
imm_low  = 0x000000000000002A
imm_high = 0x00000000

Binary (hex): 10 00 00 00 2A 00 00 00 00 00 00 00 00 00 00 00
```

### PUSH R0

```
op   = 0x0C  (PUSH)
dst  = 0x00
src1 = 0x00  (R0)
src2 = 0x00
imm_low  = 0x0000000000000000
imm_high = 0x00000000

Binary (hex): 0C 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

### CMP R0, R1

```
op   = 0x08  (CMP)
dst  = 0x00
src1 = 0x00  (R0)
src2 = 0x01  (R1)
imm_low  = 0x0000000000000000
imm_high = 0x00000000

Binary (hex): 08 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00
```

## Pseudo-Instruction Expansion

The assembler expands pseudo-instructions into real instructions before encoding:

### JMP label

```asm
JMP label
```

Expands to:
```asm
LDI Rtemp, label
JMP Rtemp
```

`Rtemp` is R4 or R5 (chosen to avoid clashing with the source register).

### CALL label

```asm
CALL label
```

Expands to:
```asm
LDI Rtemp, label
CALL Rtemp
```

### JZ label / JNZ label / JE label / etc.

```asm
JZ label
```

Expands to:
```asm
LDI Rtemp, label
JZ Rtemp
```

### LDR Rd, [imm]

```asm
LDR R0, [0x1000]
```

Expands to:
```asm
LDI R4, 0x1000
LDR R0, [R4]
```

### STR [imm], Rd

```asm
STR [0x1000], R0
```

Expands to:
```asm
LDI R4, 0x1000
STR [R4], R0
```

## Data Encoding

### DB (Define Byte)

Each byte is stored in the low byte of a 128-bit word:

```asm
DB "A", "B", 0
```

Encoded as:
```
Word 0: 0x00000000000000000000000000000041
Word 1: 0x00000000000000000000000000000042
Word 2: 0x00000000000000000000000000000000
```

### DW (Define Word)

Each 16-bit value occupies one 128-bit word:

```asm
DW 100, 200
```

Encoded as:
```
Word 0: 0x00000000000000000000000000000064  (100)
Word 1: 0x000000000000000000000000000000C8  (200)
```

### DT (Define 128-bit Word)

Each value is stored as a full 128-bit word:

```asm
DT 0x123456789ABCDEF
```

Encoded as:
```
Word 0: 0x0000000000000000000123456789ABCDEF
```

## Label Resolution

Labels are resolved in two phases:

1. **Assembly phase**: Each file is parsed independently. Labels within a file are resolved to addresses relative to the file's base address (typically 0).
2. **Link phase**: When multiple files are linked, each file's labels are offset by the file's placement address, and cross-file references are resolved using `.global` / `.extern` symbol tables.

## Multi-File Encoding

When linking multiple `.jvav` files:

1. Each file is parsed and encoded independently
2. The linker concatenates the encoded binaries
3. Labels are adjusted by each file's base address
4. Undefined labels are resolved against `.global` symbols from other files

**Example**:
```
File A (base=0):  100 bytes
File B (base=100): 200 bytes

Label `foo` in File A: address 10
Label `bar` in File B: address 20 (relative), 120 (absolute)
```

## Binary File Format

The output `.bin` file is a raw sequence of 128-bit words:

- Instructions occupy 16 bytes each
- Data definitions occupy 16 bytes per value
- The file is loaded into VM memory starting at address 0
- No header or metadata is present

**Example binary layout**:
```
Address 0:    [Instruction 1]  (16 bytes)
Address 1:    [Instruction 2]  (16 bytes)
...
Address N:    [Data word 1]    (16 bytes)
Address N+1:  [Data word 2]    (16 bytes)
```
