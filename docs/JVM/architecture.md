# JVAV Virtual Machine Architecture

The JVAV Virtual Machine (JVM) is a 128-bit word-addressable stack-based executor implemented in C99. It interprets JVAV binary files (`.bin`) produced by the `jvavc` assembler and linker, executing them through a fetch-decode-execute loop.

## Overview

The JVM is designed around a fixed 128-bit instruction word architecture. Every instruction occupies exactly 16 bytes, enabling simple and fast instruction decoding. The VM provides a flat memory model with dynamic expansion, a bump allocator for heap memory, and a syscall-based I/O mechanism.

Key characteristics:

- **128-bit words**: All registers, memory addresses, and arithmetic operations operate on 128-bit signed integers
- **Word-addressable memory**: Address `N` refers to word `N`, not byte `N * 16`
- **Flat memory space**: Code, data, heap, and stack share a single address space
- **Dynamic RAM expansion**: Memory grows on demand via `realloc` when unmapped addresses are accessed
- **Syscall mailbox**: I/O and OS interactions use memory-mapped mailbox addresses

## Registers

The JVM exposes 11 architectural registers, implemented as an array of 128-bit values:

| Index | Name | Purpose | Callee-saved |
|-------|------|---------|--------------|
| 0 | R0 | General purpose / argument 0 / return value | No |
| 1 | R1 | General purpose / argument 1 | **Yes** |
| 2 | R2 | General purpose / argument 2 | **Yes** |
| 3 | R3 | General purpose / argument 3 | **Yes** |
| 4 | R4 | General purpose / temporary | No |
| 5 | R5 | General purpose / temporary | No |
| 6 | R6 (FP) | Frame pointer | **Yes** |
| 7 | R7 | Reserved (backend temporary) | No |
| 8 | PC | Program counter | — |
| 9 | SP | Stack pointer | **Yes** |
| 10 | FLAGS | Comparison result flag | No |

### Register conventions

- **R0**: Used for passing argument 0 and return values. Caller-saved.
- **R1–R3**: Used for passing arguments 1–3. Callee-saved.
- **R4–R5**: Used by the assembler backend for expanding pseudo-instructions. May be clobbered by any instruction that uses immediates or label references.
- **R6 (FP)**: Frame pointer. Must be preserved across function calls. Established in the function prologue and restored in the epilogue.
- **R7**: Reserved. Hand-written assembly should avoid using R7 because the backend relies on it for temporary values.
- **PC**: Program counter. Automatically incremented after each instruction. Modified by jump, call, and return instructions.
- **SP**: Stack pointer. Points to the top element of the stack. Initial value is `mem_capacity - 1`.
- **FLAGS**: Set by `CMP` to a tri-state value: 1 (equal), 2 (less), 0 (greater). Used by conditional jumps.

## Instruction Format

Every instruction is exactly 16 bytes (128 bits), stored in little-endian byte order:

```
[ imm_high(32) | imm_low(64) | src2(8) | src1(8) | dst(8) | op(8) ]
   MSB                                                            LSB
```

| Field | Size | Description |
|-------|------|-------------|
| `op` | 8 bits | Opcode identifying the operation |
| `dst` | 8 bits | Destination register index |
| `src1` | 8 bits | First source register index |
| `src2` | 8 bits | Second source register index |
| `imm_low` | 64 bits | Lower 64 bits of immediate value |
| `imm_high` | 32 bits | Upper 32 bits of immediate value (sign-extended to 128 bits) |

The immediate value is reconstructed as:

```c
var imm = ((var)(int32_t)instr.imm_high << 64) | instr.imm_low;
```

This encoding allows any 96-bit signed immediate to be loaded directly into a register via the `LDI` instruction. The sign extension of `imm_high` ensures that 96-bit values with the high bit set are correctly interpreted as negative numbers.

### Opcode listing

| Opcode | Value | Instruction |
|--------|-------|-------------|
| OP_HALT | 0x00 | HALT |
| OP_MOV | 0x01 | MOV |
| OP_LDR | 0x02 | LDR |
| OP_STR | 0x03 | STR |
| OP_ADD | 0x04 | ADD |
| OP_SUB | 0x05 | SUB |
| OP_MUL | 0x06 | MUL |
| OP_DIV | 0x07 | DIV |
| OP_CMP | 0x08 | CMP |
| OP_JMP | 0x09 | JMP |
| OP_JZ | 0x0A | JZ |
| OP_JNZ | 0x0B | JNZ |
| OP_PUSH | 0x0C | PUSH |
| OP_POP | 0x0D | POP |
| OP_CALL | 0x0E | CALL |
| OP_RET | 0x0F | RET |
| OP_LDI | 0x10 | LDI |
| OP_JE | 0x11 | JE |
| OP_JNE | 0x12 | JNE |
| OP_JL | 0x13 | JL |
| OP_JG | 0x14 | JG |
| OP_JLE | 0x15 | JLE |
| OP_JGE | 0x16 | JGE |
| OP_MOD | 0x17 | MOD |
| OP_AND | 0x18 | AND |
| OP_OR | 0x19 | OR |
| OP_XOR | 0x1A | XOR |
| OP_SHL | 0x1B | SHL |
| OP_SHR | 0x1C | SHR |
| OP_NOT | 0x1D | NOT |

## Execution Loop

The JVM executes programs through a simple fetch-decode-execute loop:

```c
void jvm_run(JVM *vm) {
    vm->running = 1;
    while (vm->running) {
        instruction_t instr = *(instruction_t*)&vm->mem[PC];
        decode_and_execute(vm, instr);
        PC++;
    }
}
```

Execution terminates when one of the following conditions occurs:

1. `HALT` instruction is executed
2. `SYS_EXIT` syscall sets `vm->running = 0`
3. An unrecoverable error occurs (out of memory, invalid memory access)

The PC is incremented after each instruction before the instruction executes its own modifications. For jump instructions, the PC is overwritten after the increment, effectively jumping to the target address.

## Program Loading

When `jvm_load_program` is called with a `.bin` file:

1. The file is opened and its size is determined
2. Memory is allocated to hold the program (dynamic expansion if needed)
3. The binary is loaded starting at address 0
4. `mem_code_end` is set to the end of the loaded code/data
5. `heap_base` and `heap_ptr` are initialized to `mem_code_end + STACK_GUARD`
6. `SP` is initialized to `mem_capacity - 1`
7. `PC` is set to 0 (execution starts at address 0)

The entry point `_start` must be placed at address 0. In JVL programs, the frontend automatically generates a `_start` stub that calls `main()` and then halts.

## Memory-Mapped I/O Ports

In addition to the syscall mailbox, the VM supports legacy direct memory-mapped I/O ports for backward compatibility:

| Port | Function |
|------|----------|
| `0xFFF0` | `putchar` — write low byte as ASCII character |
| `0xFFF2` | `putint` — write signed integer as decimal text |

When `io_write` detects a store to these addresses, it performs the corresponding I/O operation instead of writing to RAM. These ports are handled as special cases before the syscall mailbox check. New code should use `.syscall` directives instead.

## Implementation Details

The JVM is implemented in two source files:

- `jvm/src/main.c`: Command-line interface, argument parsing, file loading
- `jvm/src/jvm.c`: VM initialization, memory management, execution loop, syscall dispatch

The VM structure (`JVM`) maintains:

- `mem`: Dynamically allocated RAM array
- `mem_capacity`: Current RAM size in words
- `reg[14]`: Register file (11 architectural + 3 internal)
- `running`: Execution state flag
- `mem_code_end`: End of loaded program
- `heap_base` / `heap_ptr`: Bump allocator state
- `syscall_arg0`–`syscall_arg2` / `syscall_ret`: Syscall mailbox cache
- `mmap_table[16]`: Memory-mapped file entries
- `fd_table[16]`: Open file descriptors
- `exit_code`: Program exit code

## Performance Characteristics

- **Pure interpreter**: No JIT compilation. Each instruction is decoded from the 16-byte format at runtime.
- **No branch prediction**: Every conditional jump performs a full compare-and-branch.
- **Word-level access**: All memory operations are 128-bit aligned. No byte-level addressing overhead.
- **Dynamic expansion cost**: Accessing an address beyond current capacity triggers `realloc` and zeroing of new memory.
- **Syscall overhead**: Each syscall involves mailbox writes plus host OS calls. File I/O is particularly expensive relative to arithmetic.
