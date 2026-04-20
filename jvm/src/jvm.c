#include "jvm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int ensure_mem(JVM *vm, var addr) {
    if (addr < 0) return -1;
    if (addr < vm->mem_capacity) return 0;
    if (addr >= MEM_MAX) return -1;
    var newcap = vm->mem_capacity;
    while (newcap <= addr) {
        if (newcap > MEM_MAX / 2) { newcap = MEM_MAX; break; }
        newcap *= 2;
    }
    var *p = (var *)realloc(vm->mem, (size_t)newcap * sizeof(var));
    if (!p) return -1;
    memset(p + vm->mem_capacity, 0, (size_t)(newcap - vm->mem_capacity) * sizeof(var));
    vm->mem = p;
    vm->mem_capacity = newcap;
    return 0;
}

/* ---------- I/O ---------- */

static void io_write_legacy(var addr, var val) {
    if (addr == IO_ADDR_PUTCHAR) {
        putchar((int)(val & 0xFF));
        fflush(stdout);
    } else if (addr == IO_ADDR_PUTINT) {
        char buf[64];
        int len = 0;
        var v = val;
        if (v < 0) { putchar('-'); v = -v; }
        do { buf[len++] = '0' + (char)(v % 10); v /= 10; } while (v > 0);
        while (len--) putchar(buf[len]);
        fflush(stdout);
    } else if (addr == IO_ADDR_PUTHEX) {
        printf("0x%016llx%016llx", (unsigned long long)(val >> 64), (unsigned long long)val);
        fflush(stdout);
    }
}

static var io_read_legacy(var addr) {
    if (addr == IO_ADDR_GETCHAR) {
        int c = getchar();
        return (c == EOF) ? -1 : c;
    }
    if (addr == IO_ADDR_GETINT) {
        long long v = 0;
        scanf("%lld", &v);
        return (var)v;
    }
    return 0;
}

/* ---------- mmap ---------- */

static mmap_entry_t* find_mmap(JVM *vm, var addr) {
    for (int i = 0; i < MAX_MMAP; i++) {
        if (vm->mmap_table[i].active && addr >= vm->mmap_table[i].start &&
            addr < vm->mmap_table[i].start + vm->mmap_table[i].size)
            return &vm->mmap_table[i];
    }
    return NULL;
}

static var mmap_read(JVM *vm, mmap_entry_t *ent, var addr) {
    if (!ent->file) return 0;
    long off = ent->file_offset + (long)(addr - ent->start) * sizeof(var);
    fseek(ent->file, off, SEEK_SET);
    var val = 0;
    fread(&val, sizeof(var), 1, ent->file);
    return val;
}

static void mmap_write(JVM *vm, mmap_entry_t *ent, var addr, var val) {
    if (!ent->file) return;
    long off = ent->file_offset + (long)(addr - ent->start) * sizeof(var);
    fseek(ent->file, off, SEEK_SET);
    fwrite(&val, sizeof(var), 1, ent->file);
    fflush(ent->file);
}

/* ---------- syscalls ---------- */

static int read_vm_string(JVM *vm, var addr, char *buf, size_t max) {
    size_t i = 0;
    while (i < max - 1) {
        if (ensure_mem(vm, addr + i) < 0) return -1;
        char c = (char)(vm->mem[(size_t)(addr + i)] & 0xFF);
        buf[i] = c;
        if (c == 0) break;
        i++;
    }
    buf[max - 1] = 0;
    return 0;
}

static void syscall_dispatch(JVM *vm, var cmd) {
    var a0 = vm->syscall_arg0;
    var a1 = vm->syscall_arg1;
    var a2 = vm->syscall_arg2;
    switch ((int)cmd) {
        case SYS_MMAP_FILE: {
            int slot = -1;
            for (int i = 0; i < MAX_MMAP; i++) if (!vm->mmap_table[i].active) { slot = i; break; }
            if (slot < 0) { vm->syscall_ret = 0; break; }
            char fname[1024];
            if (read_vm_string(vm, a0, fname, sizeof(fname)) < 0) { vm->syscall_ret = 0; break; }
            FILE *f = fopen(fname, "rb+");
            if (!f) f = fopen(fname, "rb");
            if (!f) { vm->syscall_ret = 0; break; }
            vm->mmap_table[slot].active = 1;
            vm->mmap_table[slot].start = a1;
            vm->mmap_table[slot].size = a2;
            vm->mmap_table[slot].file = f;
            vm->mmap_table[slot].file_offset = 0;
            vm->syscall_ret = slot + 1;
            break;
        }
        case SYS_MUNMAP: {
            int slot = (int)a0 - 1;
            if (slot < 0 || slot >= MAX_MMAP || !vm->mmap_table[slot].active) {
                vm->syscall_ret = -1; break;
            }
            if (vm->mmap_table[slot].file) fclose(vm->mmap_table[slot].file);
            vm->mmap_table[slot].active = 0;
            vm->syscall_ret = 0;
            break;
        }
        case SYS_MSYNC: {
            int slot = (int)a0 - 1;
            if (slot < 0 || slot >= MAX_MMAP || !vm->mmap_table[slot].active) {
                vm->syscall_ret = -1; break;
            }
            if (vm->mmap_table[slot].file) fflush(vm->mmap_table[slot].file);
            vm->syscall_ret = 0;
            break;
        }
        case SYS_FOPEN: {
            char fname[1024], mode[16];
            if (read_vm_string(vm, a0, fname, sizeof(fname)) < 0 ||
                read_vm_string(vm, a1, mode, sizeof(mode)) < 0) {
                vm->syscall_ret = 0; break;
            }
            int fd = -1;
            for (int i = 0; i < MAX_FD; i++) if (!vm->fd_table[i]) { fd = i; break; }
            if (fd < 0) { vm->syscall_ret = 0; break; }
            FILE *f = fopen(fname, mode);
            if (!f) { vm->syscall_ret = 0; break; }
            vm->fd_table[fd] = f;
            vm->syscall_ret = fd + 1;
            break;
        }
        case SYS_FCLOSE: {
            int fd = (int)a0 - 1;
            if (fd < 0 || fd >= MAX_FD || !vm->fd_table[fd]) { vm->syscall_ret = -1; break; }
            fclose(vm->fd_table[fd]);
            vm->fd_table[fd] = NULL;
            vm->syscall_ret = 0;
            break;
        }
        case SYS_FREAD: {
            int fd = (int)a0 - 1;
            var buf_addr = a1;
            var count = a2;
            if (fd < 0 || fd >= MAX_FD || !vm->fd_table[fd] || buf_addr < 0 || count < 0) {
                vm->syscall_ret = -1; break;
            }
            size_t total = 0;
            for (var i = 0; i < count; i++) {
                var word_addr = buf_addr + i;
                if (ensure_mem(vm, word_addr) < 0) { vm->syscall_ret = -1; return; }
                int c = fgetc(vm->fd_table[fd]);
                if (c == EOF) break;
                vm->mem[(size_t)word_addr] = (var)c;
                total++;
            }
            vm->syscall_ret = total;
            break;
        }
        case SYS_FWRITE: {
            int fd = (int)a0 - 1;
            var buf_addr = a1;
            var count = a2;
            if (fd < 0 || fd >= MAX_FD || !vm->fd_table[fd] || buf_addr < 0 || count < 0) {
                vm->syscall_ret = -1; break;
            }
            size_t total = 0;
            for (var i = 0; i < count; i++) {
                var word_addr = buf_addr + i;
                if (ensure_mem(vm, word_addr) < 0) { vm->syscall_ret = -1; return; }
                unsigned char c = (unsigned char)(vm->mem[(size_t)word_addr] & 0xFF);
                if (fputc(c, vm->fd_table[fd]) == EOF) break;
                total++;
            }
            fflush(vm->fd_table[fd]);
            vm->syscall_ret = total;
            break;
        }
        case SYS_FSEEK: {
            int fd = (int)a0 - 1;
            if (fd < 0 || fd >= MAX_FD || !vm->fd_table[fd]) { vm->syscall_ret = -1; break; }
            vm->syscall_ret = fseek(vm->fd_table[fd], (long)a1, (int)a2);
            break;
        }
        case SYS_FTELL: {
            int fd = (int)a0 - 1;
            if (fd < 0 || fd >= MAX_FD || !vm->fd_table[fd]) { vm->syscall_ret = -1; break; }
            vm->syscall_ret = ftell(vm->fd_table[fd]);
            break;
        }
        case SYS_MEMCPY: {
            var dst = a0, src = a1, cnt = a2;
            if (dst < 0 || src < 0 || cnt < 0) { vm->syscall_ret = -1; break; }
            for (var i = cnt - 1; i >= 0; i--) {
                if (ensure_mem(vm, dst + i) < 0 || ensure_mem(vm, src + i) < 0) { vm->syscall_ret = -1; break; }
                vm->mem[(size_t)(dst + i)] = vm->mem[(size_t)(src + i)];
            }
            vm->syscall_ret = 0;
            break;
        }
        case SYS_MEMSET: {
            var dst = a0, val = a1, cnt = a2;
            if (dst < 0 || cnt < 0) { vm->syscall_ret = -1; break; }
            for (var i = 0; i < cnt; i++) {
                if (ensure_mem(vm, dst + i) < 0) { vm->syscall_ret = -1; break; }
                vm->mem[(size_t)(dst + i)] = val;
            }
            vm->syscall_ret = 0;
            break;
        }
        case SYS_MALLOC: {
            var size = a0;
            if (size <= 0) { vm->syscall_ret = 0; break; }
            var addr = vm->heap_ptr;
            var end = addr + size;
            // Try to grow memory if heap would collide with stack
            if (end >= vm->reg[SP] - STACK_GUARD) {
                var need = end + STACK_GUARD + 1024;
                if (ensure_mem(vm, need) == 0) {
                    vm->reg[SP] = vm->mem_capacity - 1;
                }
            }
            // Final check after potential grow
            if (end >= vm->reg[SP] - STACK_GUARD) {
                fprintf(stderr, "Heap overflow: heap end %lld >= SP guard %lld\n",
                    (long long)end, (long long)(vm->reg[SP] - STACK_GUARD));
                vm->syscall_ret = 0; break;
            }
            if (ensure_mem(vm, end - 1) < 0) {
                vm->syscall_ret = 0; break;
            }
            vm->heap_ptr = end;
            vm->syscall_ret = addr;
            break;
        }
        case SYS_FREE: {
            var addr = a0;
            if (addr < vm->heap_base || addr >= vm->heap_ptr) {
                fprintf(stderr, "Invalid free address %lld\n", (long long)addr);
                vm->syscall_ret = -1; break;
            }
            // Tombstone the first word for debugging
            vm->mem[(size_t)addr] = (var)0xDEAD;
            vm->syscall_ret = 0;
            break;
        }
        default:
            vm->syscall_ret = -1;
            break;
    }
}

/* ---------- io_read / io_write with mailbox ---------- */

static var io_read(JVM *vm, var addr) {
    if (addr == SYSCALL_ARG0) return vm->syscall_arg0;
    if (addr == SYSCALL_ARG1) return vm->syscall_arg1;
    if (addr == SYSCALL_ARG2) return vm->syscall_arg2;
    if (addr == SYSCALL_RET)  return vm->syscall_ret;
    if (addr == IO_ADDR_GETCHAR || addr == IO_ADDR_GETINT)
        return io_read_legacy(addr);
    return 0;
}

static void io_write(JVM *vm, var addr, var val) {
    if (addr == SYSCALL_CMD) {
        syscall_dispatch(vm, val);
        return;
    }
    if (addr == SYSCALL_ARG0) { vm->syscall_arg0 = val; return; }
    if (addr == SYSCALL_ARG1) { vm->syscall_arg1 = val; return; }
    if (addr == SYSCALL_ARG2) { vm->syscall_arg2 = val; return; }
    if (addr == IO_ADDR_PUTCHAR || addr == IO_ADDR_PUTINT || addr == IO_ADDR_PUTHEX)
        io_write_legacy(addr, val);
}

/* ---------- public API ---------- */

void jvm_init(JVM *vm) {
    vm->mem = (var *)calloc(MEM_INITIAL, sizeof(var));
    vm->mem_capacity = MEM_INITIAL;
    memset(vm->reg, 0, sizeof(vm->reg));
    vm->reg[SP] = MEM_INITIAL - 1;
    vm->running = 0;
    vm->mem_code_end = 0;
    vm->heap_base = 0;
    vm->heap_ptr = 0;
    memset(vm->mmap_table, 0, sizeof(vm->mmap_table));
    memset(vm->fd_table, 0, sizeof(vm->fd_table));
}

void jvm_free(JVM *vm) {
    free(vm->mem);
}

int jvm_load_program(JVM *vm, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) { perror("fopen"); return -1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    var words = (sz + sizeof(var) - 1) / sizeof(var);
    if (words > vm->mem_capacity) {
        var newcap = vm->mem_capacity;
        while (newcap < words) { if (newcap > MEM_MAX / 2) { newcap = MEM_MAX; break; } newcap *= 2; }
        var *p = (var *)realloc(vm->mem, (size_t)newcap * sizeof(var));
        if (!p) { fclose(f); fprintf(stderr, "Out of memory for program\n"); return -1; }
        memset(p + vm->mem_capacity, 0, (size_t)(newcap - vm->mem_capacity) * sizeof(var));
        vm->mem = p;
        vm->mem_capacity = newcap;
        vm->reg[SP] = vm->mem_capacity - 1;
    }
    size_t cnt = fread(vm->mem, sizeof(var), (size_t)words, f);
    fclose(f);
    if (cnt == 0 && sz > 0) { fprintf(stderr, "Read error\n"); return -1; }
    vm->mem_code_end = words;
    vm->heap_base = words + STACK_GUARD;
    vm->heap_ptr = vm->heap_base;
    vm->reg[PC] = 0;
    return 0;
}

void jvm_run(JVM *vm) {
    vm->running = 1;
    while (vm->running) {
        var raw;
        if (vm->reg[PC] >= 0 && vm->reg[PC] < vm->mem_capacity) {
            raw = vm->mem[(size_t)vm->reg[PC]];
        } else {
            if (ensure_mem(vm, vm->reg[PC]) < 0) {
                fprintf(stderr, "PC out of bounds and cannot grow\n");
                vm->running = 0; break;
            }
            raw = vm->mem[(size_t)vm->reg[PC]];
        }
        vm->reg[PC]++;

        instruction_t instr;
        memcpy(&instr, &raw, 16);

        uint8_t op = instr.op;
        uint8_t dst = instr.dst;
        uint8_t src1 = instr.src1;
        uint8_t src2 = instr.src2;
        var imm = ((var)(int32_t)instr.imm_high << 64) | instr.imm_low;

        var val1 = 0, val2 = 0, addr = 0;
        if (op == OP_LDR || op == OP_STR || op == OP_ADD || op == OP_SUB ||
            op == OP_MUL || op == OP_DIV || op == OP_CMP || op == OP_PUSH) {
            val1 = (src1 < NUM_REGS) ? vm->reg[src1] : 0;
        }
        if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV || op == OP_CMP) {
            val2 = (src2 < NUM_REGS) ? vm->reg[src2] : 0;
        }

        switch (op) {
            case OP_HALT: vm->running = 0; break;
            case OP_MOV: vm->reg[dst] = (src1 < NUM_REGS) ? vm->reg[src1] : imm; break;
            case OP_LDR:
                addr = val1;
                if (addr >= 0) {
                    mmap_entry_t *ent = find_mmap(vm, addr);
                    if (ent) {
                        vm->reg[dst] = mmap_read(vm, ent, addr);
                    } else if ((addr >= IO_ADDR_PUTCHAR && addr <= IO_ADDR_PUTHEX) ||
                               (addr >= SYSCALL_CMD && addr <= SYSCALL_RET)) {
                        vm->reg[dst] = io_read(vm, addr);
                    } else {
                        if (ensure_mem(vm, addr) == 0)
                            vm->reg[dst] = vm->mem[(size_t)addr];
                    }
                }
                break;
            case OP_STR:
                addr = vm->reg[dst];
                if (addr >= 0) {
                    mmap_entry_t *ent = find_mmap(vm, addr);
                    if (ent) {
                        mmap_write(vm, ent, addr, val1);
                    } else if ((addr >= IO_ADDR_PUTCHAR && addr <= IO_ADDR_PUTHEX) ||
                               (addr >= SYSCALL_CMD && addr <= SYSCALL_RET)) {
                        io_write(vm, addr, val1);
                    } else {
                        if (ensure_mem(vm, addr) == 0)
                            vm->mem[(size_t)addr] = val1;
                    }
                }
                break;
            case OP_ADD: vm->reg[dst] = val1 + val2; break;
            case OP_SUB: vm->reg[dst] = val1 - val2; break;
            case OP_MUL: vm->reg[dst] = val1 * val2; break;
            case OP_DIV:
                if (val2 == 0) { fprintf(stderr, "Div by zero\n"); vm->running = 0; }
                else vm->reg[dst] = val1 / val2;
                break;
            case OP_CMP: vm->reg[FLAGS] = (val1 == val2) ? 1 : (val1 < val2) ? 2 : 0; break;
            case OP_JMP: vm->reg[PC] = vm->reg[dst]; break;
            case OP_JZ: if (ZF) vm->reg[PC] = vm->reg[dst]; break;
            case OP_JNZ: if (!ZF) vm->reg[PC] = vm->reg[dst]; break;
            case OP_JE: if (vm->reg[FLAGS] == 1) vm->reg[PC] = vm->reg[dst]; break;
            case OP_JNE: if (vm->reg[FLAGS] != 1) vm->reg[PC] = vm->reg[dst]; break;
            case OP_JL: if (vm->reg[FLAGS] == 2) vm->reg[PC] = vm->reg[dst]; break;
            case OP_JG: if (vm->reg[FLAGS] == 0) vm->reg[PC] = vm->reg[dst]; break;
            case OP_JLE: if (vm->reg[FLAGS] == 1 || vm->reg[FLAGS] == 2) vm->reg[PC] = vm->reg[dst]; break;
            case OP_JGE: if (vm->reg[FLAGS] == 1 || vm->reg[FLAGS] == 0) vm->reg[PC] = vm->reg[dst]; break;
            case OP_PUSH:
                vm->reg[SP]--;
                if (vm->reg[SP] < vm->mem_code_end + STACK_GUARD) {
                    fprintf(stderr, "Stack overflow at SP=%lld\n", (long long)vm->reg[SP]);
                    vm->running = 0; break;
                }
                if (ensure_mem(vm, vm->reg[SP]) == 0)
                    vm->mem[(size_t)vm->reg[SP]] = val1;
                break;
            case OP_POP: vm->reg[dst] = vm->mem[(size_t)vm->reg[SP]++]; break;
            case OP_CALL:
                vm->reg[SP]--;
                if (vm->reg[SP] < vm->mem_code_end + STACK_GUARD) {
                    fprintf(stderr, "Stack overflow at SP=%lld\n", (long long)vm->reg[SP]);
                    vm->running = 0; break;
                }
                if (ensure_mem(vm, vm->reg[SP]) == 0)
                    vm->mem[(size_t)vm->reg[SP]] = vm->reg[PC];
                vm->reg[PC] = vm->reg[dst];
                break;
            case OP_RET: vm->reg[PC] = vm->mem[(size_t)vm->reg[SP]++]; break;
            case OP_LDI: vm->reg[dst] = imm; break;
            default: fprintf(stderr, "Unknown opcode 0x%02X\n", op); vm->running = 0;
        }
    }
}
