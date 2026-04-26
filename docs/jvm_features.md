# JVM Virtual Machine Reference

The JVAV Virtual Machine (JVM) is a 128-bit word-addressable executor written in C99. It loads JVAV binary files (`.bin`) and executes them via a fetch-decode-execute loop.

---

## 128-bit Instruction Format

Each instruction occupies exactly 16 bytes (128 bits), stored in little-endian byte order:

```
[ imm_high(32) | imm_low(64) | src2(8) | src1(8) | dst(8) | op(8) ]
   MSB                                                            LSB
```

| Field | Size | Description |
|-------|------|-------------|
| `op` | 8-bit | Opcode |
| `dst` | 8-bit | Destination register index |
| `src1` | 8-bit | Source register 1 |
| `src2` | 8-bit | Source register 2 |
| `imm_low` | 64-bit | Lower immediate bits |
| `imm_high` | 32-bit | Upper immediate bits (sign-extended to 128 bits) |

The immediate value is reconstructed as:
```c
var imm = ((var)(int32_t)instr.imm_high << 64) | instr.imm_low;
```

This allows loading any 96-bit immediate value directly into a register via `LDI`.

---

## Registers

| Index | Name | Purpose |
|-------|------|---------|
| 0–5 | R0–R5 | General purpose |
| 6 | R6 / FP | Frame pointer |
| 7 | R7 | Reserved (temporary) |
| 8 | PC | Program counter |
| 9 | SP | Stack pointer |
| 10 | FLAGS | Zero flag (bit 0) |

The VM maintains 13 registers internally (`NUM_REGS + 3`), including PC, SP, and FLAGS.

---

## Memory Model

- **Addressable unit:** 128-bit word (not byte-addressable)
- **Initial RAM:** 4096 words
- **Maximum RAM:** 2^30 words (~16 GB of 128-bit words = ~256 GB raw)
- **Dynamic expansion:** `realloc` on access to unmapped addresses
- **Word addressing:** Address `N` refers to word `N`, not byte `N * 16`

### Memory Layout

```
Low addresses:
  0          : Program code and data (loaded from .bin)
  heap_base  : Heap start (bump allocator)
  heap_ptr   : Heap end (grows upward)
  ...
  stack guard: Reserved region below stack
  mem_capacity-1 : Stack top (SP initial value)
```

### Stack Behavior

- Grows **downward** (decrement SP on push)
- `SP` points to the **top** element (not one past)
- Initial SP = `mem_capacity - 1`
- Stack guard = 256 words reserved below code/data

### Heap Allocator

Simple bump allocator:
- `heap_base` and `heap_ptr` start at the end of loaded code/data
- `SYS_MALLOC` increments `heap_ptr` by the requested size
- `SYS_FREE` writes a tombstone (`0xDEAD`) but does not reclaim memory
- Heap growth attempts to expand RAM if it would collide with the stack

---

## Syscall Mailbox

The primary I/O and OS interaction mechanism is the **syscall mailbox**, a set of memory-mapped addresses:

| Address | Name | Purpose |
|---------|------|---------|
| `0xFFE0` | `SYSCALL_CMD` | Write command ID to trigger dispatch |
| `0xFFE1` | `SYSCALL_ARG0` | Argument 0 |
| `0xFFE2` | `SYSCALL_ARG1` | Argument 1 |
| `0xFFE3` | `SYSCALL_ARG2` | Argument 2 |
| `0xFFE4` | `SYSCALL_RET` | Return value (read after trigger) |

**Usage flow:**
1. Write arguments to `0xFFE1`–`0xFFE3`
2. Write command ID to `0xFFE0`
3. Read return value from `0xFFE4`

The `.syscall` assembler directive automates this by generating a standard function wrapper.

---

## System Call List

| ID | Name | Args | Return | Description |
|----|------|------|--------|-------------|
| 1 | `SYS_MMAP_FILE` | ARG0=fname_addr, ARG1=map_addr, ARG2=size_words | handle (>0) or 0 | Memory-map a file |
| 2 | `SYS_MUNMAP` | ARG0=handle | 0 or -1 | Unmap a file |
| 3 | `SYS_MSYNC` | ARG0=handle | 0 or -1 | Sync mmap to disk |
| 4 | `SYS_FOPEN` | ARG0=fname_addr, ARG1=mode_addr | fd (>0) or 0 | Open a file |
| 5 | `SYS_FCLOSE` | ARG0=fd | 0 or -1 | Close a file |
| 6 | `SYS_FREAD` | ARG0=fd, ARG1=buf_addr, ARG2=count | bytes read or -1 | Read bytes from file |
| 7 | `SYS_FWRITE` | ARG0=fd, ARG1=buf_addr, ARG2=count | bytes written or -1 | Write bytes to file |
| 8 | `SYS_FSEEK` | ARG0=fd, ARG1=offset, ARG2=whence | pos or -1 | Seek file position |
| 9 | `SYS_FTELL` | ARG0=fd | pos or -1 | Get file position |
| 10 | `SYS_MEMCPY` | ARG0=dst, ARG1=src, ARG2=count | 0 | Copy `count` words |
| 11 | `SYS_MEMSET` | ARG0=dst, ARG1=value, ARG2=count | 0 | Set `count` words |
| 12 | `SYS_MALLOC` | ARG0=size_words | addr or 0 | Allocate heap memory |
| 13 | `SYS_FREE` | ARG0=addr | 0 or -1 | Free heap allocation |
| 14 | `SYS_PUTCHAR` | ARG0=char_code | 0 | Print ASCII character |
| 15 | `SYS_PUTINT` | ARG0=integer | 0 | Print signed decimal |
| 16 | `SYS_GETCHAR` | — | char or -1 | Read character from stdin |
| 17 | `SYS_GETINT` | — | integer | Read integer from stdin |
| 18 | `SYS_EXIT` | ARG0=exit_code | 0 | Set exit code and halt VM |
| 19 | `SYS_PUTSTR` | ARG0=addr, ARG1=len | 0 | Print `len` chars from memory |
| 20 | `SYS_SLEEP` | ARG0=milliseconds | 0 | Sleep for specified duration |

### String Arguments

Syscalls that take file names (`SYS_MMAP_FILE`, `SYS_FOPEN`) expect the string address in VM memory. Since JVAV is word-addressable, each character occupies one word. The VM reads consecutive words and extracts the low byte of each until it forms a valid C string.

### Exit Codes

`SYS_EXIT` sets `vm->exit_code` and immediately stops the VM (`vm->running = 0`). The `jvm` executable returns this code to the host OS. If `SYS_EXIT` is never called, the default exit code is 0.

### Unknown Syscalls

Writing an unrecognized command ID to `0xFFE0` sets `SYSCALL_RET` to `-1` and does nothing else.

---

## Legacy I/O Ports (Backward Compatible)

For hand-written assembly that predates the `.syscall` architecture, the VM provides fallback I/O via direct memory-mapped ports:

| Port | Function |
|------|----------|
| `0xFFF0` | `putchar` — write low byte as ASCII character |
| `0xFFF2` | `putint` — write signed integer as decimal text |

These are handled in `io_write()` as special cases before the syscall mailbox check. New code should use `.syscall` instead.

---

## Execution Loop

```c
void jvm_run(JVM *vm) {
    vm->running = 1;
    while (vm->running) {
        fetch instruction at PC
        decode opcode & operands
        execute
        PC++
    }
}
```

The loop terminates when:
- `HALT` is executed
- `SYS_EXIT` is triggered
- An unrecoverable error occurs (out of memory, invalid access)

---

## File Descriptor Table

The VM maintains an internal file descriptor table for `SYS_FOPEN`/`SYS_FCLOSE`/`SYS_FREAD`/`SYS_FWRITE`:

- Up to `MAX_FD` open files (typically 16)
- FD values returned to the VM are 1-based (`fd + 1`)
- FD 0 is reserved (represents failure)
- Files are opened with standard C `fopen()` modes (`"r"`, `"w"`, `"rb"`, etc.)

---

## Memory-Mapped Files

The VM supports `mmap`-style file mapping via `SYS_MMAP_FILE`:

- Maps a file into a range of VM word addresses
- Read/write to mapped addresses transparently accesses the file
- Multiple non-overlapping mappings are supported (`MAX_MMAP`, typically 16)
- `SYS_MSYNC` flushes changes to disk
- `SYS_MUNMAP` closes the file and releases the slot
