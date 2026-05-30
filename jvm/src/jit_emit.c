#include "jvm.h"
#include "jit_x86_64.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#define JIT_DT X64_R15
#define SYSCALL_BASE 0xFFE0
#define SYSCALL_END 0xFFE7

typedef struct { size_t pos; size_t target; } JITPatch;

static void emit_dispatch(X64Buf *b) {
    int rex = REX_W;
    if (x64_hi(JIT_DT)) rex |= 1;
    if (x64_hi(JIT_PC)) rex |= 2;
    x64_emit1(b, (uint8_t)rex);
    x64_emit1(b, 0xFF);
    x64_modrm(b, 0, 4, 4);
    x64_emit1(b, (uint8_t)((3 << 6) | (x64_lo(JIT_PC) << 3) | x64_lo(JIT_DT)));
}

/*
 * Self-contained memory address computation.
 * Computes: result_reg = vm->mem + addr_reg * 16
 * Only clobbers result_reg. Saves/restores RCX internally.
 */
static void emit_mem_addr(X64Buf *b, int result, int addr_reg, int vm_reg, size_t mem_off) {
    int scratch = X64_RCX;
    int need_save = (scratch != result);
    if (need_save) x64_push_r(b, scratch);
    x64_mov_rr(b, scratch, addr_reg);
    x64_emit1(b, 0x48); x64_emit1(b, 0xC1);    /* REX.W + SHL */
    x64_modrm(b, MOD_REG, 4, scratch);            /* SHL scratch, clobbers scratch */
    x64_emit1(b, 4);                               /* shift by 4 = *16 */
    x64_load64(b, result, vm_reg, (int)mem_off);  /* result = vm->mem */
    x64_add_rr(b, result, scratch);                /* result += scratch * 16 */
    if (need_save) x64_pop_r(b, scratch);          /* restore scratch */
}

static int jreg(int jr) {
    switch (jr) {
    case 0: return JIT_R0; case 1: return JIT_R1;
    case 2: return JIT_R2; case 3: return JIT_R3;
    case 4: return JIT_R4; case 5: return JIT_R5;
    case 6: return JIT_R6; case 7: return JIT_R7;
    case 8: return JIT_PC; case 9: return JIT_SP; case 10: return JIT_FL;
    default: return -1;
    }
}

static void save_regs(X64Buf *b, int vm_r, size_t rb) {
    static const int sv[] = {0,1,2,3,4,5,6,7,10};
    for (int ri = 0; ri < 9; ri++) {
        int i = sv[ri];
        int r = jreg(i);
        if (r < 0) continue;
        x64_store64(b, r, vm_r, (int)(rb + (size_t)i * sizeof(var)));
    }
    x64_store64(b, JIT_PC, vm_r, (int)(rb + 8 * sizeof(var)));
    x64_store64(b, JIT_SP, vm_r, (int)(rb + 9 * sizeof(var)));
}

/* Record a JCC rel32 placeholder. Returns index in patches array. */
static size_t record_jcc(X64Buf *b, int cond, JITPatch *patches, int *np) {
    x64_emit1(b, 0x0F);
    x64_emit1(b, (uint8_t)cond);
    size_t idx = *np;
    patches[idx].pos = b->len;
    (*np)++;
    x64_emit4(b, 0);
    return idx;
}

int jit_compile(JVM *vm) {
#if !defined(__x86_64__) && !defined(_WIN64)
    /* 32-bit x86 not supported; fall back to interpreter */
    return -1;
#endif
    JVM dummy; memset(&dummy, 0, sizeof(dummy));
    size_t rb = (size_t)&dummy.reg[0] - (size_t)&dummy;
    size_t mem_off = (size_t)&dummy.mem - (size_t)&dummy;
    size_t run_off = (size_t)&dummy.running - (size_t)&dummy;
    size_t exit_off = (size_t)&dummy.exit_code - (size_t)&dummy;

    size_t nwords = (size_t)vm->mem_code_end;
    if (nwords == 0) return -1;

    X64Buf b;
    x64_init(&b);

    size_t *offsets = (size_t*)calloc(nwords, sizeof(size_t));
    if (!offsets) return -1;

    JITPatch patches[512];
    int npatch = 0;
    int vm_r = X64_RBP;
    size_t halt_jmp_pos = 0;  /* position of HALT's JMP to epilogue */

    /* Prologue */
    x64_push_r(&b, X64_RBP); x64_push_r(&b, X64_RBX);
    x64_push_r(&b, X64_R12); x64_push_r(&b, X64_R13);
    x64_push_r(&b, X64_R14); x64_push_r(&b, X64_R15);
    x64_mov_rr(&b, vm_r, X64_RDI);

    for (int i = 0; i <= 10; i++) {
        int r = jreg(i);
        if (r < 0) continue;
        x64_load64(&b, r, vm_r, (int)(rb + (size_t)i * sizeof(var)));
    }

    size_t lea_patch_pos = b.len;
    for (int i = 0; i < 7; i++) x64_emit1(&b, 0x90);

    for (size_t pc = 0; pc < nwords; pc++) {
        offsets[pc] = b.len;

        instruction_t in;
        memcpy(&in, &vm->mem[pc], sizeof(in));

        int op = in.op, dst = in.dst, src1 = in.src1, src2 = in.src2;
        uint64_t imm = ((uint64_t)(int32_t)in.imm_high << 32) | (uint64_t)in.imm_low;
        int rd = jreg(dst), rs1 = jreg(src1), rs2 = jreg(src2);

        switch (op) {
        case 0: {
            x64_xor_rr(&b, X64_RCX, X64_RCX);
            x64_store64(&b, X64_RCX, vm_r, (int)run_off);
            x64_store64(&b, X64_RCX, vm_r, (int)exit_off);
            /* JMP to epilogue (patched later) */
            x64_emit1(&b, 0xE9);
            halt_jmp_pos = b.len;
            x64_emit4(&b, 0);
        } break;

        case 1: if (rd >= 0 && rs1 >= 0) x64_mov_rr(&b, rd, rs1); break;
        case 2: if (rd >= 0 && rs1 >= 0) {
            x64_push_r(&b, X64_R15);
            x64_mov_ri64(&b, X64_R15, 0xFFE0);
            x64_cmp_rr(&b, rs1, X64_R15);
            x64_pop_r(&b, X64_R15);
            size_t pi = record_jcc(&b, X64_JB, patches, &npatch);
            /* Syscall-range LDR */
            save_regs(&b, vm_r, rb);
            x64_mov_rr(&b, X64_RDI, vm_r);
            x64_mov_rr(&b, X64_RSI, rs1);
            x64_movabs_r(&b, X64_R9, (uint64_t)&jit_syscall_read);
            x64_call_r(&b, X64_R9);
            x64_mov_rr(&b, rd, X64_RAX);
            for (int ri = 0; ri <= 10; ri++) {
                int i = ri < 8 ? ri : (ri == 8 ? 8 : (ri == 9 ? 9 : 10));
                int r = jreg(i);
                if (r < 0 || r == rd) continue;
                x64_load64(&b, r, vm_r, (int)(rb + (size_t)i * sizeof(var)));
            }
            x64_add_ri(&b, JIT_PC, 1);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
            continue;
            /* Normal: compute addr, load into rd */
            emit_mem_addr(&b, X64_RDX, rs1, vm_r, mem_off);
            x64_load64(&b, rd, X64_RDX, 0);
        } break;

        case 3: if (rd >= 0 && rs1 >= 0) {
            x64_push_r(&b, X64_R15);
            x64_mov_ri64(&b, X64_R15, 0xFFE0);
            x64_cmp_rr(&b, rd, X64_R15);
            x64_pop_r(&b, X64_R15);
            size_t pi = record_jcc(&b, X64_JB, patches, &npatch);
            /* Syscall-range STR: rd=addr, rs1=val */
            save_regs(&b, vm_r, rb);
            x64_mov_rr(&b, X64_RDI, vm_r);
            x64_mov_rr(&b, X64_RSI, rd);
            x64_mov_rr(&b, X64_RDX, rs1);
            x64_movabs_r(&b, X64_R9, (uint64_t)&jit_syscall_io);
            x64_call_r(&b, X64_R9);
            for (int ri = 0; ri <= 10; ri++) {
                int i = ri < 8 ? ri : (ri == 8 ? 8 : (ri == 9 ? 9 : 10));
                int r = jreg(i);
                if (r < 0) continue;
                x64_load64(&b, r, vm_r, (int)(rb + (size_t)i * sizeof(var)));
            }
            x64_add_ri(&b, JIT_PC, 1);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
            continue;
            /* Normal: save value, compute addr, store */
            x64_push_r(&b, rs1);
            emit_mem_addr(&b, X64_RDX, rd, vm_r, mem_off);
            x64_pop_r(&b, X64_RCX);
            x64_store64(&b, X64_RCX, X64_RDX, 0);
        } break;

        case 4: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            if (rd != rs1) x64_mov_rr(&b, rd, rs1);
            x64_add_rr(&b, rd, rs2);
        } break;

        case 5: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            if (rd != rs1) x64_mov_rr(&b, rd, rs1);
            x64_sub_rr(&b, rd, rs2);
        } break;

        case 6: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            if (rd != rs1) x64_mov_rr(&b, rd, rs1);
            x64_imul_rr(&b, rd, rs2);
        } break;

        case 7: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            x64_mov_rr(&b, X64_RAX, rs1);
            x64_xor_rr(&b, X64_RDX, X64_RDX);
            x64_mov_rr(&b, X64_RCX, rs2);
            x64_emit1(&b, 0x48); x64_emit1(&b, 0xF7);
            x64_modrm(&b, MOD_REG, 7, X64_RCX);
            x64_mov_rr(&b, rd, X64_RAX);
        } break;

        case 8:
            if (rs1 >= 0 && rs2 >= 0) {
                x64_cmp_rr(&b, rs1, rs2);
                x64_xor_rr(&b, JIT_FL, JIT_FL);
                x64_setcc_r(&b, X64_RAX, X64_JE);
                x64_mov_rr(&b, JIT_FL, X64_RAX);
                x64_setcc_r(&b, X64_RAX, X64_JB);
                x64_add_rr(&b, JIT_FL, X64_RAX);
            }
            break;

        case 9: /* JMP rd */
            if (rd >= 0) {
                x64_mov_rr(&b, JIT_PC, rd);
                save_regs(&b, vm_r, rb);
                emit_dispatch(&b);
                continue;
            }
            break;

        case 10: /* JZ / JE: FLAGS == 1 */
            if (rd >= 0) {
                x64_mov_ri64(&b, X64_RAX, 1);
                x64_cmp_rr(&b, JIT_FL, X64_RAX);
                size_t pi = record_jcc(&b, X64_JNE, patches, &npatch);
                x64_mov_rr(&b, JIT_PC, rd);
                save_regs(&b, vm_r, rb);
                emit_dispatch(&b);
                patches[pi].target = b.len;
            }
            break;

        case 11: /* JNZ: FLAGS != 0 → skip if ZF (FLAGS==0) */
            if (rd >= 0) {
                x64_test_rr(&b, JIT_FL, JIT_FL);
                size_t pi = record_jcc(&b, X64_JE, patches, &npatch);
                x64_mov_rr(&b, JIT_PC, rd);
                save_regs(&b, vm_r, rb);
                emit_dispatch(&b);
                patches[pi].target = b.len;
            }
            break;

        case 12: if (rs1 >= 0) {
            x64_push_r(&b, rs1);
            x64_add_ri(&b, JIT_SP, -1);
            emit_mem_addr(&b, X64_RDX, JIT_SP, vm_r, mem_off);
            x64_pop_r(&b, X64_RCX);
            x64_store64(&b, X64_RCX, X64_RDX, 0);
        } break;

        case 13: if (rd >= 0) {
            emit_mem_addr(&b, X64_RDX, JIT_SP, vm_r, mem_off);
            x64_load64(&b, X64_RCX, X64_RDX, 0);
            x64_add_ri(&b, JIT_SP, 1);
            x64_mov_rr(&b, rd, X64_RCX);
        } break;

        case 14: /* CALL */
            if (rd >= 0) {
                x64_add_ri(&b, JIT_SP, -1);
                emit_mem_addr(&b, X64_RDX, JIT_SP, vm_r, mem_off);
                x64_add_ri(&b, JIT_PC, 1);
                x64_store64(&b, JIT_PC, X64_RDX, 0);
                x64_mov_rr(&b, JIT_PC, rd);
                save_regs(&b, vm_r, rb);
                emit_dispatch(&b);
                continue;
            }
            break;

        case 15: /* RET */
            emit_mem_addr(&b, X64_RDX, JIT_SP, vm_r, mem_off);
            x64_load64(&b, JIT_PC, X64_RDX, 0);
            x64_add_ri(&b, JIT_SP, 1);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            continue;

        case 16: if (rd >= 0) x64_mov_ri64(&b, rd, imm); break;

        case 17: /* JE: FLAGS == 1 */
            if (rd >= 0) {
                x64_mov_ri64(&b, X64_RAX, 1);
                x64_cmp_rr(&b, JIT_FL, X64_RAX);
                size_t pi = record_jcc(&b, X64_JNE, patches, &npatch);
                x64_mov_rr(&b, JIT_PC, rd);
                save_regs(&b, vm_r, rb);
                emit_dispatch(&b);
                patches[pi].target = b.len;
            }
            break;

        case 18: /* JNE: FLAGS != 1 → skip if equal */
            if (rd >= 0) {
                x64_mov_ri64(&b, X64_RAX, 1);
                x64_cmp_rr(&b, JIT_FL, X64_RAX);
                size_t pi = record_jcc(&b, X64_JE, patches, &npatch);
                x64_mov_rr(&b, JIT_PC, rd);
                save_regs(&b, vm_r, rb);
                emit_dispatch(&b);
                patches[pi].target = b.len;
            }
            break;

        case 19: /* JL: FLAGS == 2 */
            if (rd >= 0) {
                x64_mov_ri64(&b, X64_RAX, 2);
                x64_cmp_rr(&b, JIT_FL, X64_RAX);
                size_t pi = record_jcc(&b, X64_JNE, patches, &npatch);
                x64_mov_rr(&b, JIT_PC, rd);
                save_regs(&b, vm_r, rb);
                emit_dispatch(&b);
                patches[pi].target = b.len;
            }
            break;

        case 20: /* JG: FLAGS == 0 → skip if FLAGS != 0 */
            if (rd >= 0) {
                x64_test_rr(&b, JIT_FL, JIT_FL);
                size_t pi = record_jcc(&b, X64_JNE, patches, &npatch);
                x64_mov_rr(&b, JIT_PC, rd);
                save_regs(&b, vm_r, rb);
                emit_dispatch(&b);
                patches[pi].target = b.len;
            }
            break;

        case 21: /* JLE: FLAGS != 0 → skip if FLAGS == 0 */
            if (rd >= 0) {
                x64_test_rr(&b, JIT_FL, JIT_FL);
                size_t pi = record_jcc(&b, X64_JE, patches, &npatch);
                x64_mov_rr(&b, JIT_PC, rd);
                save_regs(&b, vm_r, rb);
                emit_dispatch(&b);
                patches[pi].target = b.len;
            }
            break;

        case 22: /* JGE: FLAGS != 2 → skip if FLAGS == 2 */
            if (rd >= 0) {
                x64_mov_ri64(&b, X64_RAX, 2);
                x64_cmp_rr(&b, JIT_FL, X64_RAX);
                size_t pi = record_jcc(&b, X64_JE, patches, &npatch);
                x64_mov_rr(&b, JIT_PC, rd);
                save_regs(&b, vm_r, rb);
                emit_dispatch(&b);
                patches[pi].target = b.len;
            }
            break;

        case 23: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            x64_mov_rr(&b, X64_RAX, rs1);
            x64_xor_rr(&b, X64_RDX, X64_RDX);
            x64_mov_rr(&b, X64_RCX, rs2);
            x64_emit1(&b, 0x48); x64_emit1(&b, 0xF7);
            x64_modrm(&b, MOD_REG, 7, X64_RCX);
            x64_mov_rr(&b, rd, X64_RDX);
        } break;

        case 24: case 25: case 26: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            if (rd != rs1) x64_mov_rr(&b, rd, rs1);
            if (op == 24) x64_and_rr(&b, rd, rs2);
            else if (op == 25) x64_or_rr(&b, rd, rs2);
            else x64_xor_rr(&b, rd, rs2);
        } break;

        case 27: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            x64_mov_rr(&b, X64_RCX, rs2);
            if (rd != rs1) x64_mov_rr(&b, rd, rs1);
            x64_shl_cl(&b, rd);
        } break;

        case 28: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            x64_mov_rr(&b, X64_RCX, rs2);
            if (rd != rs1) x64_mov_rr(&b, rd, rs1);
            x64_shr_cl(&b, rd);
        } break;

        case 29: if (rd >= 0 && rs1 >= 0) {
            if (rd != rs1) x64_mov_rr(&b, rd, rs1);
            x64_not_r(&b, rd);
        } break;

        default: break;
        }

        x64_add_ri(&b, JIT_PC, 1);
        save_regs(&b, vm_r, rb);
        emit_dispatch(&b);
    }  /* end for loop */

    size_t epilogue_pos = b.len;
    /* Epilogue: save regs, restore, return */
    save_regs(&b, vm_r, rb);
    x64_pop_r(&b, X64_R15); x64_pop_r(&b, X64_R14);
    x64_pop_r(&b, X64_R13); x64_pop_r(&b, X64_R12);
    x64_pop_r(&b, X64_RBX); x64_pop_r(&b, X64_RBP);
    x64_ret(&b);

    size_t dt_off = b.len;
    for (size_t i = 0; i < nwords; i++) x64_emit8(&b, 0);

    size_t total = b.len;
#ifdef _WIN32
    void *mem = VirtualAlloc(NULL, total, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) { free(offsets); x64_free(&b); return -1; }
#else
    void *mem = mmap(NULL, total, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { free(offsets); x64_free(&b); return -1; }
#endif
    memcpy(mem, b.buf, total);

    uint64_t base = (uint64_t)mem;
    uint64_t *dt = (uint64_t*)((uint8_t*)mem + dt_off);
    for (size_t i = 0; i < nwords; i++) dt[i] = base + offsets[i];

    /* Apply deferred patches */
    for (int i = 0; i < npatch; i++) {
        int32_t off = (int32_t)(patches[i].target - (patches[i].pos + 4));
        *(int32_t*)((uint8_t*)mem + patches[i].pos) = off;
    }

    /* Patch HALT's JMP to epilogue */
    if (halt_jmp_pos) {
        int32_t off = (int32_t)(epilogue_pos - (halt_jmp_pos + 4));
        *(int32_t*)((uint8_t*)mem + halt_jmp_pos) = off;
    }

    /* Patch LEA */
    int64_t rel = (int64_t)dt_off - (int64_t)(lea_patch_pos + 7);
    uint8_t *p = (uint8_t*)mem + lea_patch_pos;
    p[0] = REX_W | (x64_hi(JIT_DT) ? 4 : 0);
    p[1] = 0x8D;
    p[2] = (uint8_t)((0 << 6) | (x64_lo(JIT_DT) << 3) | 5);
    *(int32_t*)(p + 3) = (int32_t)rel;

#ifdef _WIN32
    DWORD old;
    VirtualProtect(mem, total, PAGE_EXECUTE_READ, &old);
#else
    mprotect(mem, total, PROT_READ | PROT_EXEC);
#endif
    vm->jit_entry = (void (*)(JVM*))mem;
    vm->jit_code = mem;
    vm->jit_size = total;

    free(offsets);
    x64_free(&b);
    return 0;
}

void jit_release(JVM *vm) {
    if (vm->jit_code) {
#ifdef _WIN32
        VirtualFree(vm->jit_code, 0, MEM_RELEASE);
#else
        munmap(vm->jit_code, vm->jit_size);
#endif
        vm->jit_code = NULL;
        vm->jit_entry = NULL;
    }
}
