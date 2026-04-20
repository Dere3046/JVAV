/* JVAV disassembler — part of the JVAVC toolchain
 *
 * Usage:
 *   disasm file.bin          static disassembly
 *   disasm -t file.bin       dynamic trace (executes in embedded VM)
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* ---------- instruction format (matches jvm.h / encoder.cpp) ---------- */
#pragma pack(push, 1)
typedef struct {
    uint8_t  op;
    uint8_t  dst;
    uint8_t  src1;
    uint8_t  src2;
    uint64_t imm_low;
    uint32_t imm_high;
} Instr;
#pragma pack(pop)

enum {
    OP_HALT = 0x00, OP_MOV,  OP_LDR, OP_STR,
    OP_ADD,  OP_SUB, OP_MUL, OP_DIV,
    OP_CMP,  OP_JMP, OP_JZ,  OP_JNZ,
    OP_PUSH, OP_POP, OP_CALL, OP_RET,
    OP_LDI,
    NUM_OPS
};

#define NUM_REGS 11
enum { R0, R1, R2, R3, R4, R5, R6, R7, PC, SP, FLAGS };

#define ZF (vm->reg[FLAGS] & 1)

static const char *mnemonic[] = {
    "HALT", "MOV", "LDR", "STR",
    "ADD",  "SUB", "MUL", "DIV",
    "CMP",  "JMP", "JZ",  "JNZ",
    "PUSH", "POP", "CALL", "RET",
    "LDI"
};

/* ---------- helpers ---------- */
static const char *reg_name(int r) {
    static char buf[8][4];
    static int idx = 0;
    if (r >= 0 && r <= 7) {
        int i = idx++; if (idx >= 8) idx = 0;
        snprintf(buf[i], sizeof(buf[i]), "R%d", r);
        return buf[i];
    }
    if (r == 8)  return "PC";
    if (r == 9)  return "SP";
    if (r == 10) return "FLAGS";
    return "?";
}

static __int128 sign_ext_imm(int32_t hi, uint64_t lo) {
    return ((__int128)(int32_t)hi << 64) | lo;
}

static void print_imm(__int128 v) {
    if (v == 0) { printf("0"); return; }
    if (v < 0) { printf("-"); v = -v; }
    char buf[64]; int n = 0;
    while (v > 0) { buf[n++] = '0' + (char)(v % 10); v /= 10; }
    while (n--) putchar(buf[n]);
}

static void print_hex_imm(__int128 v) {
    if (v == 0) { printf("0x0"); return; }
    if (v < 0) { printf("-0x"); v = -v; }
    else       { printf("0x"); }
    char buf[32]; int n = 0;
    while (v > 0) {
        int d = (int)(v % 16);
        buf[n++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        v /= 16;
    }
    while (n--) putchar(buf[n]);
}

/* ---------- trace VM (simplified embedded JVM) ---------- */
typedef struct {
    __int128 *mem;
    size_t    cap;
    __int128  reg[NUM_REGS];
    int       running;
} TraceVM;

static int ensure_mem(TraceVM *vm, size_t addr) {
    if (addr < vm->cap) return 0;
    size_t newcap = vm->cap;
    while (newcap <= addr) newcap *= 2;
    __int128 *p = (__int128 *)realloc(vm->mem, newcap * sizeof(__int128));
    if (!p) return -1;
    memset(p + vm->cap, 0, (newcap - vm->cap) * sizeof(__int128));
    vm->mem = p;
    vm->cap = newcap;
    return 0;
}

static int is_io_port(size_t addr) {
    return (addr >= 0xFFF0 && addr <= 0xFFF4) || (addr >= 0xFFE0 && addr <= 0xFFE4);
}

static void trace_run(__int128 *code, size_t ninstr, uint8_t *visited,
                      int *halted_cleanly, size_t *fault_pc) {
    TraceVM vm_ = {0}, *vm = &vm_;
    vm->cap = ninstr + 4096;
    vm->mem = (__int128 *)calloc(vm->cap, sizeof(__int128));
    memcpy(vm->mem, code, ninstr * sizeof(__int128));
    vm->reg[SP] = vm->cap - 1;
    vm->running = 1;
    *halted_cleanly = 0;
    *fault_pc = (size_t)-1;

    int steps = 0, max_steps = 1000000; /* guard against infinite loops */

    while (vm->running && steps++ < max_steps) {
        size_t pc = (size_t)(uint64_t)vm->reg[PC];
        if (pc >= ninstr) {
            *fault_pc = pc;
            break;
        }
        visited[pc] = 1;

        Instr in;
        memcpy(&in, (uint8_t *)&vm->mem[pc], 16);
        __int128 imm = sign_ext_imm((int32_t)in.imm_high, in.imm_low);

        __int128 val1 = 0, val2 = 0, addr = 0;
        if (in.op == OP_LDR || in.op == OP_STR || in.op == OP_ADD ||
            in.op == OP_SUB || in.op == OP_MUL || in.op == OP_DIV ||
            in.op == OP_CMP || in.op == OP_PUSH) {
            val1 = (in.src1 < NUM_REGS) ? vm->reg[in.src1] : 0;
        }
        if (in.op == OP_ADD || in.op == OP_SUB || in.op == OP_MUL ||
            in.op == OP_DIV || in.op == OP_CMP) {
            val2 = (in.src2 < NUM_REGS) ? vm->reg[in.src2] : 0;
        }

        vm->reg[PC]++; /* fetch advance */

        switch (in.op) {
            case OP_HALT: vm->running = 0; *halted_cleanly = 1; break;
            case OP_MOV:
                vm->reg[in.dst] = (in.src1 < NUM_REGS) ? vm->reg[in.src1] : imm;
                break;
            case OP_LDR:
                addr = val1;
                if (addr >= 0 && !is_io_port((size_t)addr)) {
                    if (ensure_mem(vm, (size_t)addr) == 0)
                        vm->reg[in.dst] = vm->mem[(size_t)addr];
                }
                break;
            case OP_STR:
                addr = vm->reg[in.dst];
                if (addr >= 0 && !is_io_port((size_t)addr)) {
                    if (ensure_mem(vm, (size_t)addr) == 0)
                        vm->mem[(size_t)addr] = val1;
                }
                break;
            case OP_ADD: vm->reg[in.dst] = val1 + val2; break;
            case OP_SUB: vm->reg[in.dst] = val1 - val2; break;
            case OP_MUL: vm->reg[in.dst] = val1 * val2; break;
            case OP_DIV:
                if (val2 == 0) { *fault_pc = pc; vm->running = 0; }
                else vm->reg[in.dst] = val1 / val2;
                break;
            case OP_CMP: vm->reg[FLAGS] = (val1 == val2) ? 1 : 0; break;
            case OP_JMP: vm->reg[PC] = vm->reg[in.dst]; break;
            case OP_JZ:  if (ZF) vm->reg[PC] = vm->reg[in.dst]; break;
            case OP_JNZ: if (!ZF) vm->reg[PC] = vm->reg[in.dst]; break;
            case OP_PUSH:
                vm->reg[SP]--;
                if (ensure_mem(vm, (size_t)vm->reg[SP]) == 0)
                    vm->mem[(size_t)vm->reg[SP]] = val1;
                break;
            case OP_POP:
                vm->reg[in.dst] = vm->mem[(size_t)vm->reg[SP]++];
                break;
            case OP_CALL:
                vm->reg[SP]--;
                if (ensure_mem(vm, (size_t)vm->reg[SP]) == 0)
                    vm->mem[(size_t)vm->reg[SP]] = vm->reg[PC];
                vm->reg[PC] = vm->reg[in.dst];
                break;
            case OP_RET:
                vm->reg[PC] = vm->mem[(size_t)vm->reg[SP]++];
                break;
            case OP_LDI:
                vm->reg[in.dst] = imm;
                break;
            default:
                *fault_pc = pc;
                vm->running = 0;
                break;
        }
    }

    if (steps >= max_steps) {
        fprintf(stderr, "[trace] WARNING: step limit reached, possible infinite loop\n");
    }
    free(vm->mem);
}

/* ---------- static disassembly printer ---------- */
static void print_instr(size_t idx, const Instr *in, __int128 imm,
                        int trace_visited, int trace_mode) {
    if (trace_mode) {
        printf(trace_visited ? " >>> " : "     ");
    } else {
        printf("%4zu: ", idx);
    }

    if (in->op >= NUM_OPS) {
        printf("DB   0x");
        for (int j = 15; j >= 0; j--) printf("%02x", ((const uint8_t *)in)[j]);
        printf("\n");
        return;
    }

    printf("%-5s", mnemonic[in->op]);

    switch (in->op) {
        case OP_HALT:
        case OP_RET:
            break;
        case OP_MOV:
            printf(" %s, ", reg_name(in->dst));
            if (in->src1 == 0x80) {
                if (imm >= 0 && imm <= 0xFFFF) print_hex_imm(imm);
                else print_imm(imm);
            } else {
                printf("%s", reg_name(in->src1));
            }
            break;
        case OP_LDR:
            printf(" %s, [%s]", reg_name(in->dst), reg_name(in->src1));
            break;
        case OP_STR:
            printf(" [%s], %s", reg_name(in->dst), reg_name(in->src1));
            break;
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
            printf(" %s, %s, %s", reg_name(in->dst),
                   reg_name(in->src1), reg_name(in->src2));
            break;
        case OP_CMP:
            printf(" %s, %s", reg_name(in->src1), reg_name(in->src2));
            break;
        case OP_JMP:
        case OP_JZ:
        case OP_JNZ:
        case OP_CALL:
            printf(" %s", reg_name(in->dst));
            break;
        case OP_PUSH:
            printf(" %s", reg_name(in->src1));
            break;
        case OP_POP:
            printf(" %s", reg_name(in->dst));
            break;
        case OP_LDI:
            printf(" %s, ", reg_name(in->dst));
            if (imm >= 0 && imm <= 0xFFFF) print_hex_imm(imm);
            else print_imm(imm);
            break;
    }
    printf("\n");
}

/* ---------- main ---------- */
int main(int argc, char *argv[]) {
    int trace_mode = 0;
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "-t") == 0) {
        trace_mode = 1;
        argi++;
    }
    if (argi >= argc) {
        fprintf(stderr, "Usage: %s [-t] <bytecode.bin>\n"
                        "  -t   dynamic trace (embedded VM execution)\n", argv[0]);
        return 1;
    }
    const char *path = argv[argi];

    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz % 16 != 0) {
        fprintf(stderr, "Warning: file size (%ld) is not a multiple of 16 bytes\n", sz);
    }
    size_t n = (size_t)sz / 16;
    size_t rem = (size_t)sz % 16;

    __int128 *code = (__int128 *)calloc(n + (rem ? 1 : 0), sizeof(__int128));
    if (!code) { fclose(f); return 1; }
    fread(code, 1, (size_t)sz, f);
    fclose(f);

    uint8_t *visited = NULL;
    int halted = 0;
    size_t fault_pc = (size_t)-1;

    if (trace_mode) {
        visited = (uint8_t *)calloc(n, 1);
        trace_run(code, n, visited, &halted, &fault_pc);

        size_t exec_count = 0;
        for (size_t i = 0; i < n; i++) if (visited[i]) exec_count++;

        printf("=== Trace Summary ===\n");
        printf("Total instructions: %zu\n", n);
        printf("Executed:           %zu (%.1f%%)\n",
               exec_count, n ? (exec_count * 100.0 / n) : 0.0);
        printf("Halted cleanly:     %s\n", halted ? "yes" : "NO");
        if (fault_pc != (size_t)-1)
            printf("Fault at PC:        %zu\n", fault_pc);
        printf("=====================\n\n");
    }

    for (size_t i = 0; i < n; i++) {
        Instr in;
        memcpy(&in, (uint8_t *)&code[i], 16);
        __int128 imm = sign_ext_imm((int32_t)in.imm_high, in.imm_low);
        print_instr(i, &in, imm, visited ? visited[i] : 0, trace_mode);
    }

    if (rem) {
        if (trace_mode) printf("     ");
        else printf("%4zu: ", n);
        printf("DB   0x");
        for (int j = (int)rem - 1; j >= 0; j--)
            printf("%02x", ((uint8_t *)code)[n * 16 + j]);
        printf("\n");
    }

    free(code);
    free(visited);
    return 0;
}
