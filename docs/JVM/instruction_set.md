# JVAV Instruction Set Reference

The JVAV instruction set consists of 31 opcodes (0x00–0x1D), operating on 128-bit signed integer values. All arithmetic, logical, and control flow operations are performed register-to-register, with memory access provided by explicit load/store instructions.

## Instruction encoding

Every instruction is exactly 16 bytes:

```
[ imm_high(32) | imm_low(64) | src2(8) | src1(8) | dst(8) | op(8) ]
```

For instructions that do not use an immediate, the `imm_low` and `imm_high` fields are ignored.

## Arithmetic Instructions

### ADD

```
ADD Rd, Rs, Rt
```

**Operation**: `Rd = Rs + Rt`

Performs 128-bit signed integer addition. Overflow wraps around according to two's complement arithmetic.

**Example**:
```asm
ADD R0, R1, R2    ; R0 = R1 + R2
```

### SUB

```
SUB Rd, Rs, Rt
```

**Operation**: `Rd = Rs - Rt`

Performs 128-bit signed integer subtraction.

**Example**:
```asm
SUB R0, R1, R2    ; R0 = R1 - R2
```

### MUL

```
MUL Rd, Rs, Rt
```

**Operation**: `Rd = Rs * Rt`

Performs 128-bit signed integer multiplication. The low 128 bits of the product are stored in `Rd`.

**Example**:
```asm
MUL R0, R1, R2    ; R0 = R1 * R2
```

### DIV

```
DIV Rd, Rs, Rt
```

**Operation**: `Rd = Rs / Rt`

Performs 128-bit signed integer division. Division by zero behavior is undefined.

**Example**:
```asm
DIV R0, R1, R2    ; R0 = R1 / R2
```

### MOD

```
MOD Rd, Rs, Rt
```

**Operation**: `Rd = Rs % Rt`

Performs 128-bit signed integer modulo (remainder). The result has the same sign as the dividend.

**Example**:
```asm
MOD R0, R1, R2    ; R0 = R1 % R2
```

## Logical Instructions

### AND

```
AND Rd, Rs, Rt
```

**Operation**: `Rd = Rs & Rt`

Bitwise AND of two 128-bit values.

### OR

```
OR Rd, Rs, Rt
```

**Operation**: `Rd = Rs | Rt`

Bitwise OR of two 128-bit values.

### XOR

```
XOR Rd, Rs, Rt
```

**Operation**: `Rd = Rs ^ Rt`

Bitwise XOR of two 128-bit values.

### NOT

```
NOT Rd, Rs
```

**Operation**: `Rd = ~Rs`

Bitwise NOT (complement) of a 128-bit value. Unlike other logical operations, NOT takes only one source register.

### SHL

```
SHL Rd, Rs, Rt
```

**Operation**: `Rd = Rs << (Rt & 0x7F)`

Shift left. Only the low 7 bits of `Rt` are used as the shift amount (0–127). Bits shifted out are discarded; zeros are shifted in from the right.

### SHR

```
SHR Rd, Rs, Rt
```

**Operation**: `Rd = Rs >> (Rt & 0x7F)`

Arithmetic shift right. Only the low 7 bits of `Rt` are used as the shift amount. The sign bit is replicated (preserves sign for signed integers).

## Data Movement

### MOV

```
MOV Rd, Rs
```

**Operation**: `Rd = Rs`

Copies the value of one register to another.

**Example**:
```asm
MOV R0, R1    ; R0 = R1
```

### LDI

```
LDI Rd, imm
```

**Operation**: `Rd = imm`

Loads a 96-bit immediate value into a register. The immediate is encoded in the instruction's `imm_low` (64 bits) and `imm_high` (32 bits, sign-extended) fields.

**Example**:
```asm
LDI R0, 42
LDI R1, 0x123456789ABCDEF
```

## Memory Access

### LDR

```
LDR Rd, [Rs]
```

**Operation**: `Rd = mem[Rs]`

Loads a 128-bit word from memory at the address contained in `Rs` into `Rd`.

**Example**:
```asm
LDR R0, [R1]    ; R0 = memory[R1]
```

### STR

```
STR [Rs], Rd
```

**Operation**: `mem[Rs] = Rd`

Stores the 128-bit value in `Rd` to memory at the address contained in `Rs`.

**Example**:
```asm
STR [R1], R0    ; memory[R1] = R0
```

## Comparison

### CMP

```
CMP Rs, Rt
```

**Operation**: `FLAGS = (Rs == Rt) ? 1 : (Rs < Rt) ? 2 : 0`

Compares two registers. Sets FLAGS to a tri-state value:
- `1` if `Rs == Rt`
- `2` if `Rs < Rt` (signed)
- `0` if `Rs > Rt` (signed)

The result of the subtraction is not stored.

Conditional jumps use FLAGS as follows:
- `JZ` / `JE`: branch if FLAGS == 1
- `JNZ` / `JNE`: branch if FLAGS != 1
- `JL`: branch if FLAGS == 2
- `JG`: branch if FLAGS == 0
- `JLE`: branch if FLAGS == 1 or FLAGS == 2
- `JGE`: branch if FLAGS == 1 or FLAGS == 0

**Example**:
```asm
CMP R0, R1
JE equal_label
```

## Control Flow

### JMP

```
JMP Rs
```

**Operation**: `PC = Rs`

Unconditional jump to the address contained in `Rs`.

**Note**: The assembler provides `JMP label` as a pseudo-instruction, which expands to `LDI Rtemp, label; JMP Rtemp`.

### JZ

```
JZ Rs
```

**Operation**: `if (FLAGS == 1) PC = Rs`

Jump if zero/equal. Used after `CMP` to branch when operands were equal. Equivalent to `JE`.

**Note**: The assembler provides `JZ label` as a pseudo-instruction.

### JNZ

```
JNZ Rs
```

**Operation**: `if (FLAGS != 1) PC = Rs`

Jump if not equal. Used after `CMP` to branch when operands were not equal. Equivalent to `JNE`.

### JE

```
JE Rs
```

**Operation**: `if (ZF) PC = Rs`

Jump if equal. Semantically identical to `JZ`.

### JNE

```
JNE Rs
```

**Operation**: `if (!ZF) PC = Rs`

Jump if not equal. Semantically identical to `JNZ`.

### JL

```
JL Rs
```

**Operation**: `if (FLAGS == 2) PC = Rs`

Jump if less (signed). Branches if the first operand of the preceding `CMP` was less than the second.

### JG

```
JG Rs
```

**Operation**: `if (FLAGS == 0) PC = Rs`

Jump if greater (signed). Branches if the first operand of the preceding `CMP` was greater than the second.

### JLE

```
JLE Rs
```

**Operation**: `if (FLAGS == 1 || FLAGS == 2) PC = Rs`

Jump if less or equal (signed).

### JGE

```
JGE Rs
```

**Operation**: `if (FLAGS == 1 || FLAGS == 0) PC = Rs`

Jump if greater or equal (signed).

## Stack Operations

### PUSH

```
PUSH Rs
```

**Operation**: `mem[--SP] = Rs`

Decrements the stack pointer by 1, then stores the register value at the new stack top. The stack grows downward (toward lower addresses).

**Example**:
```asm
PUSH R0    ; SP = SP - 1; mem[SP] = R0
```

### POP

```
POP Rd
```

**Operation**: `Rd = mem[SP++]`

Loads the value at the stack top into `Rd`, then increments the stack pointer by 1.

**Example**:
```asm
POP R0    ; R0 = mem[SP]; SP = SP + 1
```

## Function Call

### CALL

```
CALL Rs
```

**Operation**: `mem[--SP] = PC; PC = Rs`

Pushes the return address (current PC) onto the stack, then jumps to the address in `Rs`.

**Note**: The assembler provides `CALL label` as a pseudo-instruction, which expands to `LDI Rtemp, label; CALL Rtemp`.

### RET

```
RET
```

**Operation**: `PC = mem[SP++]`

Pops the return address from the stack into PC, returning control to the caller.

## Program Control

### HALT

```
HALT
```

**Operation**: `vm->running = 0`

Stops the virtual machine. The execution loop terminates and control returns to the host program.

## Complete Instruction Summary

| Instruction | Opcode | Operands | Description |
|-------------|--------|----------|-------------|
| HALT | 0x00 | — | Stop execution |
| MOV | 0x01 | Rd, Rs | Register move |
| LDR | 0x02 | Rd, [Rs] | Load from memory |
| STR | 0x03 | [Rs], Rd | Store to memory |
| ADD | 0x04 | Rd, Rs, Rt | Addition |
| SUB | 0x05 | Rd, Rs, Rt | Subtraction |
| MUL | 0x06 | Rd, Rs, Rt | Multiplication |
| DIV | 0x07 | Rd, Rs, Rt | Signed division |
| CMP | 0x08 | Rs, Rt | Compare (sets FLAGS) |
| JMP | 0x09 | Rs | Unconditional jump |
| JZ | 0x0A | Rs | Jump if zero |
| JNZ | 0x0B | Rs | Jump if not zero |
| PUSH | 0x0C | Rs | Push onto stack |
| POP | 0x0D | Rd | Pop from stack |
| CALL | 0x0E | Rs | Function call |
| RET | 0x0F | — | Return from function |
| LDI | 0x10 | Rd, imm | Load immediate |
| JE | 0x11 | Rs | Jump if equal |
| JNE | 0x12 | Rs | Jump if not equal |
| JL | 0x13 | Rs | Jump if less |
| JG | 0x14 | Rs | Jump if greater |
| JLE | 0x15 | Rs | Jump if less or equal |
| JGE | 0x16 | Rs | Jump if greater or equal |
| MOD | 0x17 | Rd, Rs, Rt | Modulo |
| AND | 0x18 | Rd, Rs, Rt | Bitwise AND |
| OR | 0x19 | Rd, Rs, Rt | Bitwise OR |
| XOR | 0x1A | Rd, Rs, Rt | Bitwise XOR |
| SHL | 0x1B | Rd, Rs, Rt | Shift left |
| SHR | 0x1C | Rd, Rs, Rt | Shift right (arithmetic) |
| NOT | 0x1D | Rd, Rs | Bitwise NOT |

## Instruction Execution Details

### Division and modulo

`DIV` and `MOD` perform signed operations. The sign of the result follows C99 conventions:

- `DIV`: Quotient is truncated toward zero
- `MOD`: Result has the same sign as the dividend

Division by zero produces undefined behavior.

### Shift operations

`SHL` and `SHR` use only the low 7 bits of the shift amount register (`Rt & 0x7F`). This limits shifts to 0–127 positions. Shifting by 128 or more is equivalent to shifting by `amount % 128`.

`SHR` is an arithmetic right shift: the sign bit (bit 127) is replicated into the vacated high bits.

### Comparison and flags

The `CMP` instruction compares two registers and sets FLAGS to a tri-state value: 1 if equal, 2 if less-than, 0 if greater-than. The actual subtraction result is discarded.

Conditional jumps read FLAGS to determine the branch condition.

### Memory access bounds

`LDR` and `STR` trigger dynamic memory expansion if the accessed address exceeds current capacity. If the address is negative or exceeds `MEM_MAX` (2^30 words), the access fails and may terminate the VM.
