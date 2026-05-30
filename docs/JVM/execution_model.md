# JVAV Execution Model

This document describes the execution model of the JVAV Virtual Machine, including the fetch-decode-execute cycle, instruction dispatch, and runtime behavior.

## The Execution Loop

The JVM uses a JIT-first execution strategy with interpreter fallback:

```c
void jvm_run(JVM *vm) {
    if (JVAV_JIT is set) {
        jit_compile(vm);
        vm->jit_entry(vm);  // run JIT-compiled code
        return;
    }
    // fallback interpreter ...
```
    vm->running = 1;
    while (vm->running) {
        // Fetch
        instruction_t instr = *(instruction_t*)&vm->mem[vm->reg[PC]];
        
        // Decode and execute
        switch (instr.op) {
            case OP_MOV:  vm->reg[instr.dst] = vm->reg[instr.src1]; break;
            case OP_ADD:  vm->reg[instr.dst] = vm->reg[instr.src1] + vm->reg[instr.src2]; break;
            // ... other opcodes
            case OP_HALT: vm->running = 0; break;
        }
        
        // Advance PC
        vm->reg[PC]++;
    }
}
```

Each iteration performs four steps:

1. **Fetch**: Read the 16-byte instruction at the current PC
2. **Decode**: Extract opcode, register indices, and immediate fields
3. **Execute**: Perform the operation
4. **Advance**: Increment PC by 1 (to the next word)

### Program Counter Behavior

The PC is incremented **before** checking for control flow modifications. Jump instructions overwrite PC after the increment, effectively replacing the incremented value with the target address.

**Example of a jump**:
```
Address 10: JZ R5    ; PC becomes 11 during fetch
                       ; If FLAGS == 1 (equal), PC is overwritten with R5
                       ; Otherwise, execution continues at 11
```

### Termination Conditions

The execution loop terminates when any of these conditions occur:

1. `HALT` instruction: Sets `vm->running = 0` directly
2. `SYS_EXIT` syscall: Sets `vm->exit_code` and `vm->running = 0`
3. Memory error: `ensure_mem()` fails with a negative address or `MEM_MAX` exceeded

After termination, `jvm_run` returns and the host program exits with `vm->exit_code`.

## Instruction Dispatch

The VM uses a `switch` statement for opcode dispatch. Each case handles one opcode, performing the operation and updating registers or memory as needed.

### Register Access

All registers are accessed through the `vm->reg[]` array:

```c
vm->reg[R0] = vm->reg[R1] + vm->reg[R2];  // ADD R0, R1, R2
```

The FLAGS register is updated only by `CMP`:

```c
case OP_CMP:
    vm->reg[FLAGS] = (val1 == val2) ? 1 : (val1 < val2) ? 2 : 0;
    break;
```

### Memory Access

Memory is accessed through `vm->mem[]` with bounds checking:

```c
case OP_LDR:
    if (ensure_mem(vm, vm->reg[instr.src1]) < 0) { /* error */ }
    vm->reg[instr.dst] = vm->mem[(size_t)(long long)vm->reg[instr.src1]];
    break;
```

`ensure_mem()` dynamically expands RAM if the address exceeds current capacity.

## Syscall Dispatch

When a store to `0xFFE0` (SYSCALL_CMD) is detected, the VM invokes `syscall_dispatch`:

```c
static void syscall_dispatch(JVM *vm, var cmd) {
    var a0 = vm->syscall_arg0;
    var a1 = vm->syscall_arg1;
    var a2 = vm->syscall_arg2;
    switch ((int)cmd) {
        case SYS_PUTINT:  /* print a0 */ break;
        case SYS_MALLOC:  /* allocate a0 words */ break;
        // ... other syscalls
        default: vm->syscall_ret = -1; break;
    }
}
```

The syscall handler reads arguments from the mailbox cache variables (which are updated when stores to `0xFFE1`–`0xFFE3` occur) and writes the return value to `vm->syscall_ret` (which is read when `0xFFE4` is loaded).

## Startup Sequence

When `jvm` is invoked with a `.bin` file:

1. `jvm_init()`: Allocate initial RAM (4096 words), zero all registers
2. `jvm_load_program()`: Read `.bin` file into memory starting at address 0
3. Set `mem_code_end` to the end of loaded data
4. Set `heap_base` and `heap_ptr` to `mem_code_end + STACK_GUARD`
5. Set `SP = mem_capacity - 1`
6. Set `PC = 0`
7. `jvm_run()`: Enter the execution loop
8. After termination, print "HALT\n" and exit with `exit_code`

## Runtime State Inspection

The `disasm` tool provides two modes for inspecting execution:

### Static disassembly

```bash
disasm program.bin
```

Decodes and prints all instructions in the binary file without executing them.

### Trace mode

```bash
disasm -t program.bin
```

Executes the program while printing each instruction and register state before execution. Useful for debugging.

## Common Runtime Issues

### Infinite loops

If a program enters an infinite loop without I/O, it will consume 100% CPU until manually terminated. Use trace mode to identify the loop.

### Stack overflow

Deep recursion or excessive stack allocation can cause the stack to grow into the heap region. The VM detects this on `PUSH` and `CALL` by checking whether `SP` drops below `mem_code_end + STACK_GUARD`. If so, it prints an error and halts. However, unchecked memory accesses (e.g., via `LDR`/`STR` with computed addresses) can still corrupt data.

### Use-after-free

While the MimiWorld ownership system prevents use-after-free at compile time for JVL programs, hand-written assembly can still access freed memory. The VM writes `0xDEAD` as a tombstone, which may produce unexpected values but does not trap.

### Division by zero

`DIV` and `MOD` with a zero divisor produce undefined behavior. The VM does not check for division by zero.
