# JVM Features

The JVAV Virtual Machine (JVM) is a 128-bit word-oriented executor written in C99.

## 128-bit Instruction Format

Each instruction is a fixed 16-byte (128-bit) word:

```
[ imm_high(32) | imm_low(64) | src2(8) | src1(8) | dst(8) | op(8) ]
   MSB                                                            LSB
```

- `op` (8-bit): opcode
- `dst`, `src1`, `src2` (8-bit each): register indices
- `imm_low` (64-bit): lower immediate bits
- `imm_high` (32-bit): upper immediate bits (sign-extended)

The immediate value is reconstructed as:
```c
var imm = ((var)(int32_t)instr.imm_high << 64) | instr.imm_low;
```

## Memory Model

- Addressable unit: **128-bit word**
- Initial RAM: 4096 words
- Maximum RAM: ~16 GB (1 << 30 words)
- Dynamic expansion via `realloc` on access

## I/O Ports

Memory-mapped I/O via `STR [port], value`:

| Port | Function |
|------|----------|
| `0xFFF0` | `putchar` |
| `0xFFF2` | `putint` |
| `0xFFF1` | `getchar` (reserved) |
| `0xFFF3` | `getint` (reserved) |

## System Calls

Triggered via `SYSCALL_CMD` mailbox (`0xFFE0`):

| Call | ID | Description |
|------|-----|-------------|
| `SYS_MMAP_FILE` | 1 | Memory-map a file |
| `SYS_MUNMAP` | 2 | Unmap a file |
| `SYS_FOPEN` | 4 | Open a file |
| `SYS_FCLOSE` | 5 | Close a file |
| `SYS_FREAD` | 6 | Read from file |
| `SYS_FWRITE` | 7 | Write to file |
| `SYS_MEMCPY` | 10 | Copy memory |
| `SYS_MEMSET` | 11 | Set memory |

## Stack Convention

- Stack grows **downward**
- `SP` points to the top of the stack
- `FP` (R6) marks the stack frame base
- Function prologue: `PUSH FP; MOV FP, SP; SUB SP, SP, locals`
- Function epilogue: `MOV SP, FP; POP FP; RET`

## Execution Loop

```c
while (vm->running) {
    fetch instruction at PC
    decode opcode & operands
    execute
    PC++
}
```
