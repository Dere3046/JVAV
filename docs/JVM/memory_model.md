# JVAV Memory Model

The JVAV Virtual Machine uses a flat, word-addressable memory model. All memory addresses refer to 128-bit words, not individual bytes. This design simplifies the architecture but requires awareness of word-level granularity in all memory operations.

## Addressable Unit

The fundamental addressable unit is a **128-bit word**. This means:

- Address `0` refers to the first 128-bit word (bytes 0–15)
- Address `1` refers to the second 128-bit word (bytes 16–31)
- Address `N` refers to word `N` (bytes `N*16` to `N*16+15`)

There is no byte-level addressing. All loads and stores transfer exactly 128 bits.

## Memory Layout

The virtual address space is organized as follows:

```
Low addresses
    |
    v
+--------------------------------------------------+
| 0          : Program code and data (from .bin)    |
| ...                                             |
| heap_base  : Heap start (after code/data)        |
| heap_ptr   : Heap end (grows upward)             |
| ...                                             |
| stack_guard: Reserved region below stack         |
| ...                                             |
| SP         : Stack top (grows downward)          |
| ...                                             |
| mem_capacity-1 : Highest addressable word        |
+--------------------------------------------------+
```

### Regions

**Code and Data Segment**
- Starts at address 0
- Loaded from the `.bin` file
- Contains program instructions and initialized data
- Read-only during execution (enforced by convention, not hardware)
- Size determined by the binary file

**Stack Guard**
- A reserved region of 256 words below the initial stack position
- Prevents accidental stack overflow into the heap
- Located at `mem_code_end` to `mem_code_end + STACK_GUARD - 1`

**Heap**
- Starts at `heap_base = mem_code_end + STACK_GUARD`
- Grows upward (toward higher addresses)
- Managed by a simple bump allocator
- No memory reclamation (freed memory is not reused)

**Stack**
- Starts at `mem_capacity - 1`
- Grows downward (toward lower addresses)
- `SP` points to the top element (the last pushed value)
- Initial `SP = mem_capacity - 1` (empty stack)

## Dynamic Memory Expansion

The VM starts with 4096 words of RAM (`MEM_INITIAL`). When an instruction accesses an address beyond current capacity:

1. The VM checks if the address is within `MEM_MAX` (2^30 words, approximately 16 GB of words or 256 GB of raw bytes)
2. If valid, capacity is doubled repeatedly until it covers the requested address
3. `realloc` is called to resize the memory array
4. Newly allocated words are zero-initialized

This allows programs to use as much memory as needed up to the platform limit, without requiring upfront allocation of the maximum address space.

**Example of expansion**:
```
Initial:  mem_capacity = 4096
Access:   address 5000
Expand:   mem_capacity = 8192
Access:   address 10000
Expand:   mem_capacity = 16384
```

## Stack Behavior

The JVAV stack is a downward-growing stack with these properties:

- **Push**: `SP = SP - 1; mem[SP] = value`
- **Pop**: `value = mem[SP]; SP = SP + 1`
- **Empty condition**: `SP == mem_capacity - 1`
- **Top element**: Always at `mem[SP]`

### Stack Layout at Function Entry

When a function is called, the stack contains:

```
Address (relative to FP):
    FP+0: saved R6 (old frame pointer)
    FP+1: return address (pushed by CALL)
    FP+2: saved R1
    FP+3: saved R2
    FP+4: saved R3
    FP+5: argument 0
    FP+6: argument 1
    FP+7: argument 2
    FP+8: argument 3
    ...
    FP-1: local variable 0
    FP-2: local variable 1
    ...
```

Local variables use negative offsets from the frame pointer (`FP-1`, `FP-2`, etc.). Arguments use positive offsets (`FP+5`, `FP+6`, etc.).

### Stack Overflow Detection

The VM checks for stack overflow on every `PUSH` and `CALL` instruction. If `SP` would drop below `mem_code_end + STACK_GUARD`, the VM prints an error and halts.

Additionally:

- The stack guard region (256 words) provides a buffer between heap and stack
- If the stack grows into the heap region, the bump allocator may detect collision and attempt to expand memory
- If memory cannot be expanded further, the program may crash or produce undefined behavior

## Heap Allocator

The JVAV heap uses a simple bump allocator with these characteristics:

**Allocation** (`SYS_MALLOC` / `alloc()`):
1. Current `heap_ptr` is returned as the allocation address
2. `heap_ptr` is incremented by the requested size
3. If the allocation would exceed available memory, the VM attempts to expand RAM
4. If expansion fails, returns 0 (null)

**Deallocation** (`SYS_FREE` / `free()`):
1. Writes a tombstone value (`0xDEAD`) to the first word of the block
2. Does **not** reclaim memory or merge adjacent free blocks
3. The address is not reused by subsequent allocations

**Limitations**:
- No alignment guarantees
- No fragmentation handling (memory is never reused)
- No size tracking (the allocator does not record block sizes)
- Tombstone detection is not enforced at runtime

## Memory-Mapped Files

The VM supports `mmap`-style file mapping through syscalls:

- `SYS_MMAP_FILE`: Maps a host file into a range of VM word addresses
- `SYS_MUNMAP`: Closes the mapping
- `SYS_MSYNC`: Flushes changes to disk

Up to `MAX_MMAP` (16) concurrent mappings are supported. Read and write operations to mapped addresses transparently access the underlying file.

### Mapping behavior

When a memory access occurs:
1. The VM first checks if the address falls within an active mmap region
2. If so, the operation is redirected to the mapped file
3. Otherwise, the operation accesses regular RAM

This allows files to be treated as if they were loaded into memory, without requiring explicit read/write syscalls.

## Word-Addressable Strings

Because JVAV is word-addressable, strings are stored with one character per 128-bit word:

```
Address + 0: 0x0000...0061  ('a')
Address + 1: 0x0000...0062  ('b')
Address + 2: 0x0000...0063  ('c')
```

This has important implications:

- String length in characters equals the number of words
- No packing: 3 characters use 3 words (48 bytes), not 3 bytes
- The `DB` directive stores each byte in its own word
- Null terminators are explicit: `DB "abc", 0` creates 4 words

Syscalls that operate on strings (`SYS_FOPEN`, `SYS_PUTSTR`, etc.) read consecutive words and extract the low byte of each to form C strings.

## Memory Access Semantics

### Loads

- `LDR Rd, [Rs]`: Reads the 128-bit word at address `Rs` into `Rd`
- If address exceeds capacity, triggers dynamic expansion
- If address is negative or exceeds `MEM_MAX`, behavior is undefined

### Stores

- `STR [Rs], Rd`: Writes the 128-bit value in `Rd` to address `Rs`
- If address exceeds capacity, triggers dynamic expansion
- Stores to `0xFFF0` or `0xFFF2` trigger legacy I/O instead of RAM write
- Stores to `0xFFE0`–`0xFFE4` trigger syscall mailbox operations

### Alignment

Since all accesses are 128-bit word-aligned by definition (address `N` always refers to word `N`), there are no alignment restrictions or penalties.
