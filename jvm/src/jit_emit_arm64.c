#include "jvm.h"
#include "jit_arm64.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#define JIT_DT JIT_DT
#define SYSCALL_BASE 0xFFE0

typedef struct { size_t pos; size_t target; } Patch;

/* ARM64 dispatch: LDR X16, [X12, X8, LSL #3]; BR X16 */
/* Using X16 as scratch for the jump target */
static void emit_dispatch(A64Buf *b) {
    a64_emit(b, 0xF8607900 | a64_rm(JIT_PC) | a64_rn(JIT_DT) | a64_rd(16));
    a64_br(b, 16);
}

static void emit_mem_addr(A64Buf *b, int addr_reg, int vm_reg, size_t mem_off) {
    a64_load64(b, JIT_SCR, vm_reg, (int)mem_off);        /* JIT_SCR = vm->mem */
    a64_mov_rr(b, JIT_SCR, JIT_SCR);                       /* (for LSL below) */
    a64_lsl_rr(b, addr_reg, addr_reg);                        /* needs: shl addr, 4 */
}

/* Compute result_reg = vm->mem + addr_reg * 16 */
/* Uses JIT_SCR (X11) as temp, clobbers it */
static void emit_mem_addr2(A64Buf *b, int result, int addr, int vm_reg, size_t mem_off) {
    a64_load64(b, JIT_SCR, vm_reg, (int)mem_off);
    a64_mov_rr(b, result, addr);
    /* LSL result, result, #4  = multiply by 16 */
    a64_emit(b, 0xD37CF000 | a64_rd(result)); /* LSL Xd, Xd, #4 via shifted register */
    a64_add_rr(b, result, JIT_SCR);
}
/* Actually, ARM64 LSL immediate: UBFM Xd, Xn, #(64-4), #63 */
/* Simpler: use a shift + add sequence */
/* LSL Xd, Xn, #4  →  0xD37CF000 | rn | rd  (UBFM alias) */
/* Wait, the correct encoding for LSL Xd, Xn, #4 is: */
/* LSL Xd, Xn, #4 = UBFM Xd, Xn, #(64-4), #63  =  0xD3400000 | (60<<16) | (63<<10) | rn | rd */
/* Or just: ADD Xd, Xn, Xn, LSL #4 -> Xd = Xn + Xn*16 = Xn*17. No... */
/* Let me just use a different approach: */

static void emit_mem_addr3(A64Buf *b, int result, int addr, int vm_reg, size_t mem_off) {
    /* result = vm->mem + addr * 16 */
    /* Load vm->mem into JIT_SCR */
    a64_load64(b, JIT_SCR, vm_reg, (int)mem_off);
    /* Compute addr * 16 into result */
    a64_mov_rr(b, result, addr);   /* result = addr */
    /* ARM64: LSL Xd, Xn, #4  */
    /* This is encoded as UBFM: Xd = Xn #(64-4), #63 */
    a64_emit(b, 0xD37CF000 | a64_rn(addr) | a64_rd(result));
    /* Add vm->mem */
    a64_add_rr(b, result, JIT_SCR);
}

/* Wait, UBFM encoding: SF(1) N(0) 1 0 0 1 0 0 N(0) immr(6) imms(6) Rn(5) Rd(5) */
/* For Xd = Xn, LSL #4: N=1, immr=60 (64-4), imms=63 */
/* Actually: 0x9340FC00 | rn | rd for LSL Xd, Xn, #4? No...*/
/* Let me just define a proper LSL immediate function. */

static void a64_lsl_imm(A64Buf *b, int dst, int src, int shift) {
    /* ARM64 LSL Xd, Xn, #shift  (shift 1..63) */
    /* Alias of UBFM Xd, Xn, #(64-shift), #63 */
    int immr = (64 - shift) & 0x3F;
    int imms = 63;
    a64_emit(b, 0x93400000 | (immr << 16) | (imms << 10) | a64_rn(src) | a64_rd(dst));
}

static void emit_mem_addr4(A64Buf *b, int result, int addr, int vm_reg, size_t mem_off) {
    a64_load64(b, JIT_SCR, vm_reg, (int)mem_off);
    a64_lsl_imm(b, result, addr, 4);
    a64_add_rr(b, result, JIT_SCR);
}

#define emit_mem_addr emit_mem_addr4

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

static void save_regs(A64Buf *b, int vm_r, size_t rb) {
    static const int sv[] = {0,1,2,3,4,5,6,7,10};
    for (int ri = 0; ri < 9; ri++) {
        int i = sv[ri];
        int r = jreg(i);
        if (r < 0) continue;
        a64_store64(b, r, vm_r, (int)(rb + (size_t)i * sizeof(var)));
    }
    a64_store64(b, JIT_PC, vm_r, (int)(rb + 8 * sizeof(var)));
    a64_store64(b, JIT_SP, vm_r, (int)(rb + 9 * sizeof(var)));
}

/* Record a B.cond placeholder. The cond is ARM64 condition code (A64_EQ, A64_NE, etc). */
static size_t record_bcond(A64Buf *b, int cond, Patch *patches, int *np) {
    size_t idx = *np;
    patches[idx].pos = b->len;
    a64_emit(b, 0x54000000 | (cond & 0xF));
    (*np)++;
    return idx;
}

/* ARM64 signed division helper */
static void emit_sdiv(A64Buf *b, int rd, int rn, int rm) {
    a64_emit(b, 0x9AC00C00 | a64_rm(rm) | a64_rn(rn) | a64_rd(rd));
}

int jit_compile(JVM *vm) {
#if !defined(__aarch64__) && !defined(_M_ARM64)
    return -1;
#endif
    JVM dummy; memset(&dummy, 0, sizeof(dummy));
    size_t rb = (size_t)&dummy.reg[0] - (size_t)&dummy;
    size_t mem_off = (size_t)&dummy.mem - (size_t)&dummy;
    size_t run_off = (size_t)&dummy.running - (size_t)&dummy;
    size_t exit_off = (size_t)&dummy.exit_code - (size_t)&dummy;

    size_t nwords = (size_t)vm->mem_code_end;
    if (nwords == 0) return -1;

    A64Buf b;
    a64_init(&b);

    size_t *offsets = (size_t*)calloc(nwords, sizeof(size_t));
    if (!offsets) return -1;

    Patch patches[512];
    int npatch = 0;
    int vm_r = JIT_VM; /* X19 = JVM struct pointer */
    size_t halt_jmp_pos = 0;

    /* ===== Prologue ===== */
    /* Save callee-saved registers X19-X28 */
    /* STP X19, X20, [SP, #-80]!  (pre-index, save 2, dec SP by 80) */
    /* LDP for restore: LDP X19, X20, [SP], #80 */
    a64_emit(&b, 0xA9BE53F3); /* STP X19, X20, [SP, #-80]! */
    a64_emit(&b, 0xADBF73F5); /* wait, need proper encoding for STP */
    /* STP: 1010 1001 0 00 imm7  Rn  Rt2 */
    /* STP X19, X20, [SP, #-80]!: imm7 = -80/8 = -10 = 1110110 (2s comp) */
    /* Let me just use individual STR for simplicity */
    
    /* SUB SP, SP, #80 */
    
    /* Use the add_ri/sub_ri functions for proper encoding */
    a64_add_ri(&b, 31, -80); /* SP -= 80 */
    /* Store X19-X28 to [SP + offset] */
    /* STR X19, [SP] */
    a64_store64(&b, A64_R19, 31, 0);
    /* STR X20, [SP, #8] */ a64_store64(&b, A64_R20, 31, 8);
    a64_store64(&b, A64_R21, 31, 16);
    a64_store64(&b, A64_R22, 31, 24);
    a64_store64(&b, A64_R23, 31, 32);
    a64_store64(&b, A64_R24, 31, 40);
    a64_store64(&b, A64_R25, 31, 48);
    a64_store64(&b, A64_R26, 31, 56);
    a64_store64(&b, A64_R27, 31, 64);
    a64_store64(&b, A64_R28, 31, 72);

    /* JVM* is in X0 (first arg). Save to X19 */
    a64_mov_rr(&b, vm_r, A64_R0);

    /* Load JVM registers from struct */
    for (int i = 0; i <= 10; i++) {
        int r = jreg(i);
        if (r < 0) continue;
        a64_load64(&b, r, vm_r, (int)(rb + (size_t)i * sizeof(var)));
    }

    /* Placeholder for loading dispatch table address into JIT_DT */
    /* We'll patch this later with the actual address */
    size_t dt_addr_patch = b.len;
    a64_mov_ri(&b, JIT_DT, 0);  /* MOV X12, #0 - placeholder */

    /* ===== Instructions ===== */
    for (size_t pc = 0; pc < nwords; pc++) {
        offsets[pc] = b.len;

        instruction_t in;
        memcpy(&in, &vm->mem[pc], sizeof(in));

        int op = in.op, dst = in.dst, src1 = in.src1, src2 = in.src2;
        uint64_t imm = ((uint64_t)(int32_t)in.imm_high << 32) | (uint64_t)in.imm_low;
        int rd = jreg(dst), rs1 = jreg(src1), rs2 = jreg(src2);

        switch (op) {
        /*** HALT ***/
        case 0: {
            a64_mov_ri(&b, A64_R0, 0);
            a64_store64(&b, A64_R0, vm_r, (int)run_off);
            a64_store64(&b, A64_R0, vm_r, (int)exit_off);
            a64_emit(&b, 0x14000000); /* B #0 (placeholder, patched later) */
            halt_jmp_pos = b.len - 4;
        } break;

        /*** MOV ***/
        case 1: if (rd >= 0 && rs1 >= 0) a64_mov_rr(&b, rd, rs1); break;

        /*** LDR ***/
        case 2: if (rd >= 0 && rs1 >= 0) {
            a64_mov_ri(&b, JIT_SCR, 0xFFE0);
            a64_cmp_rr(&b, rs1, JIT_SCR);
            size_t pi = record_bcond(&b, A64_LO, patches, &npatch);
            /* Syscall path */
            save_regs(&b, vm_r, rb);
            a64_mov_rr(&b, A64_R0, vm_r);
            a64_mov_rr(&b, A64_R1, rs1);
            a64_movabs(&b, A64_R16, (uint64_t)&jit_syscall_read);
            a64_call_r(&b, A64_R16);
            a64_mov_rr(&b, rd, A64_R0);
            for (int ri = 0; ri <= 10; ri++) {
                int i = ri < 8 ? ri : (ri == 8 ? 8 : (ri == 9 ? 9 : 10));
                int r = jreg(i);
                if (r < 0 || r == rd) continue;
                a64_load64(&b, r, vm_r, (int)(rb + (size_t)i * sizeof(var)));
            }
            a64_add_ri(&b, JIT_PC, 1);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
            continue;
            /* Normal path */
            emit_mem_addr(&b, JIT_SCR, rs1, vm_r, mem_off);
            a64_load64(&b, rd, JIT_SCR, 0);
        } break;

        /*** STR ***/
        case 3: if (rd >= 0 && rs1 >= 0) {
            a64_mov_ri(&b, JIT_SCR, 0xFFE0);
            a64_cmp_rr(&b, rd, JIT_SCR);
            size_t pi = record_bcond(&b, A64_LO, patches, &npatch);
            /* Syscall path */
            save_regs(&b, vm_r, rb);
            a64_mov_rr(&b, A64_R0, vm_r);
            a64_mov_rr(&b, A64_R1, rd);
            a64_mov_rr(&b, A64_R2, rs1);
            a64_movabs(&b, A64_R16, (uint64_t)&jit_syscall_io);
            a64_call_r(&b, A64_R16);
            for (int ri = 0; ri <= 10; ri++) {
                int i = ri < 8 ? ri : (ri == 8 ? 8 : (ri == 9 ? 9 : 10));
                int r = jreg(i);
                if (r < 0) continue;
                a64_load64(&b, r, vm_r, (int)(rb + (size_t)i * sizeof(var)));
            }
            a64_add_ri(&b, JIT_PC, 1);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
            continue;
            /* Normal path */
            a64_push_r(&b, rs1);
            emit_mem_addr(&b, JIT_SCR, rd, vm_r, mem_off);
            a64_pop_r(&b, A64_R2);
            a64_store64(&b, A64_R2, JIT_SCR, 0);
        } break;

        /*** ADD/SUB/MUL ***/
        case 4: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            if (rd != rs1) a64_mov_rr(&b, rd, rs1);
            a64_add_rr(&b, rd, rs2);
        } break;
        case 5: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            if (rd != rs1) a64_mov_rr(&b, rd, rs1);
            a64_sub_rr(&b, rd, rs2);
        } break;
        case 6: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            if (rd != rs1) a64_mov_rr(&b, rd, rs1);
            a64_mul_rr(&b, rd, rs2);
        } break;

        /*** DIV: rd = rs1 / rs2 ***/
        case 7: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            emit_sdiv(&b, rd, rs1, rs2);
        } break;

        /*** CMP ***/
        case 8: if (rs1 >= 0 && rs2 >= 0) {
            a64_cmp_rr(&b, rs1, rs2);
            /* JVAV FLAGS: 0=greater, 1=equal, 2=less */
            a64_cset(&b, JIT_FL, A64_EQ);  /* JIT_FL = 1 if equal */
            a64_cset(&b, JIT_SCR, A64_LT); /* scratch = 1 if less */
            a64_add_rr(&b, JIT_FL, JIT_SCR); /* +1 if less */
            a64_add_rr(&b, JIT_FL, JIT_SCR); /* +1 if less → total +2 for less */
        } break;

        /*** JMP ***/
        case 9: if (rd >= 0) {
            a64_mov_rr(&b, JIT_PC, rd);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            continue;
        } break;

        /*** Conditional jumps: compare FLAGS then skip-forward if not matching ***/
        case 10: if (rd >= 0) { /* JZ: FLAGS == 1 */
            a64_mov_ri(&b, A64_R0, 1);
            a64_cmp_rr(&b, JIT_FL, A64_R0);
            size_t pi = record_bcond(&b, A64_NE, patches, &npatch);
            a64_mov_rr(&b, JIT_PC, rd);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
        } break;

        case 11: if (rd >= 0) { /* JNZ: FLAGS != 0 */
            a64_cmp_rr(&b, JIT_FL, JIT_FL); /* compare with self */
            size_t pi = record_bcond(&b, A64_EQ, patches, &npatch);
            a64_mov_rr(&b, JIT_PC, rd);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
        } break;

        /*** PUSH ***/
        case 12: if (rs1 >= 0) {
            a64_push_r(&b, rs1);
            a64_add_ri(&b, JIT_SP, -1);
            emit_mem_addr(&b, JIT_SCR, JIT_SP, vm_r, mem_off);
            a64_pop_r(&b, A64_R1);
            a64_store64(&b, A64_R1, JIT_SCR, 0);
        } break;

        /*** POP ***/
        case 13: if (rd >= 0) {
            emit_mem_addr(&b, JIT_SCR, JIT_SP, vm_r, mem_off);
            a64_load64(&b, A64_R1, JIT_SCR, 0);
            a64_add_ri(&b, JIT_SP, 1);
            a64_mov_rr(&b, rd, A64_R1);
        } break;

        /*** CALL ***/
        case 14: if (rd >= 0) {
            a64_add_ri(&b, JIT_SP, -1);
            emit_mem_addr(&b, JIT_SCR, JIT_SP, vm_r, mem_off);
            a64_add_ri(&b, JIT_PC, 1);
            a64_store64(&b, JIT_PC, JIT_SCR, 0);
            a64_mov_rr(&b, JIT_PC, rd);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            continue;
        } break;

        /*** RET ***/
        case 15: {
            emit_mem_addr(&b, JIT_SCR, JIT_SP, vm_r, mem_off);
            a64_load64(&b, JIT_PC, JIT_SCR, 0);
            a64_add_ri(&b, JIT_SP, 1);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            continue;
        }

        /*** LDI ***/
        case 16: if (rd >= 0) a64_mov_ri(&b, rd, imm); break;

        /*** JE: FLAGS == 1 ***/
        case 17: if (rd >= 0) {
            a64_mov_ri(&b, A64_R0, 1);
            a64_cmp_rr(&b, JIT_FL, A64_R0);
            size_t pi = record_bcond(&b, A64_NE, patches, &npatch);
            a64_mov_rr(&b, JIT_PC, rd);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
        } break;

        /*** JNE: FLAGS != 1 ***/
        case 18: if (rd >= 0) {
            a64_mov_ri(&b, A64_R0, 1);
            a64_cmp_rr(&b, JIT_FL, A64_R0);
            size_t pi = record_bcond(&b, A64_EQ, patches, &npatch);
            a64_mov_rr(&b, JIT_PC, rd);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
        } break;

        /*** JL: FLAGS == 2 ***/
        case 19: if (rd >= 0) {
            a64_mov_ri(&b, A64_R0, 2);
            a64_cmp_rr(&b, JIT_FL, A64_R0);
            size_t pi = record_bcond(&b, A64_NE, patches, &npatch);
            a64_mov_rr(&b, JIT_PC, rd);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
        } break;

        /*** JG: FLAGS == 0 ***/
        case 20: if (rd >= 0) {
            a64_cmp_rr(&b, JIT_FL, JIT_FL); /* compare with self = always 0 */
            size_t pi = record_bcond(&b, A64_NE, patches, &npatch);
            a64_mov_rr(&b, JIT_PC, rd);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
        } break;

        /*** JLE: FLAGS != 0 ***/
        case 21: if (rd >= 0) {
            a64_cmp_rr(&b, JIT_FL, JIT_FL);
            size_t pi = record_bcond(&b, A64_EQ, patches, &npatch);
            a64_mov_rr(&b, JIT_PC, rd);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
        } break;

        /*** JGE: FLAGS != 2 ***/
        case 22: if (rd >= 0) {
            a64_mov_ri(&b, A64_R0, 2);
            a64_cmp_rr(&b, JIT_FL, A64_R0);
            size_t pi = record_bcond(&b, A64_EQ, patches, &npatch);
            a64_mov_rr(&b, JIT_PC, rd);
            save_regs(&b, vm_r, rb);
            emit_dispatch(&b);
            patches[pi].target = b.len;
        } break;

        /*** MOD: rd = rs1 % rs2 ***/
        case 23: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            /* ARM64: SDIV tmp, rs1, rs2; MSUB rd, tmp, rs2, rs1 */
            /* MSUB: 1 0 0 1 1 0 1 1 0 0 Rm 0 0 Ra Rn Rd */
            /* First compute division */
            a64_emit(&b, 0x9AC00C00 | a64_rm(rs2) | a64_rn(rs1) | a64_rd(JIT_SCR));
            /* Then MSUB: Xd = Xn - Xm * Xa */
            /* MSUB Xd, Xn, Xm, Xa = 0x1B008000 | rm | ra | rn | rd */
            a64_emit(&b, 0x1B008000 | a64_rm(rs2) | ((uint32_t)(JIT_SCR & 0x1F) << 10) | a64_rn(rs1) | a64_rd(rd));
        } break;

        /*** AND/OR/XOR ***/
        case 24: case 25: case 26: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            if (rd != rs1) a64_mov_rr(&b, rd, rs1);
            if (op == 24) a64_and_rr(&b, rd, rs2);
            else if (op == 25) a64_or_rr(&b, rd, rs2);
            else a64_xor_rr(&b, rd, rs2);
        } break;

        /*** SHL ***/
        case 27: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            if (rd != rs1) a64_mov_rr(&b, rd, rs1);
            a64_lsl_rr(&b, rd, rs2);
        } break;

        /*** SHR ***/
        case 28: if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            if (rd != rs1) a64_mov_rr(&b, rd, rs1);
            a64_lsr_rr(&b, rd, rs2);
        } break;

        /*** NOT ***/
        case 29: if (rd >= 0 && rs1 >= 0) {
            if (rd != rs1) a64_mov_rr(&b, rd, rs1);
            a64_not_r(&b, rd);
        } break;

        default: break;
        }

        /* Fall-through dispatch */
        a64_add_ri(&b, JIT_PC, 1);
        save_regs(&b, vm_r, rb);
        emit_dispatch(&b);
    }

    /* ===== Epilogue ===== */
    size_t epilogue_pos = b.len;
    save_regs(&b, vm_r, rb);

    /* Restore callee-saved */
    a64_load64(&b, A64_R19, 31, 0);   a64_load64(&b, A64_R20, 31, 8);
    a64_load64(&b, A64_R21, 31, 16);  a64_load64(&b, A64_R22, 31, 24);
    a64_load64(&b, A64_R23, 31, 32);  a64_load64(&b, A64_R24, 31, 40);
    a64_load64(&b, A64_R25, 31, 48);  a64_load64(&b, A64_R26, 31, 56);
    a64_load64(&b, A64_R27, 31, 64);  a64_load64(&b, A64_R28, 31, 72);
    a64_add_ri(&b, 31, 80); /* ADD SP, SP, #80 */

    a64_ret(&b);

    /* ===== Dispatch table ===== */
    size_t dt_off = b.len;
    for (size_t i = 0; i < nwords; i++) a64_emit8(&b, 0);

    /* ===== Allocate executable memory ===== */
    size_t total = b.len;
#ifdef _WIN32
    void *mem = VirtualAlloc(NULL, total, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) { free(offsets); a64_free(&b); return -1; }
#else
    void *mem = mmap(NULL, total, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { free(offsets); a64_free(&b); return -1; }
#endif
    memcpy(mem, b.buf, total);

    uint64_t base = (uint64_t)mem;
    uint64_t *dt = (uint64_t*)((uint8_t*)mem + dt_off);
    for (size_t i = 0; i < nwords; i++) dt[i] = base + offsets[i];

    /* Patch deferred B.cond instructions */
    for (int i = 0; i < npatch; i++) {
        int32_t diff = (int32_t)(patches[i].target - patches[i].pos);
        uint32_t *insn = (uint32_t*)((uint8_t*)mem + patches[i].pos);
        *insn |= ((diff >> 2) & 0x7FFFF) << 5;
    }

    /* Patch HALT's B to epilogue */
    if (halt_jmp_pos) {
        int32_t diff = (int32_t)(epilogue_pos - halt_jmp_pos);
        uint32_t *insn = (uint32_t*)((uint8_t*)mem + halt_jmp_pos);
        *insn = 0x14000000 | ((diff >> 2) & 0x3FFFFFF);
    }

    /* Patch dispatch table address into the placeholder */
    uint64_t dt_addr = base + dt_off;
    uint8_t *patch_p = (uint8_t*)mem + dt_addr_patch;
    A64Buf tmp;
    a64_init(&tmp);
    a64_mov_ri(&tmp, JIT_DT, dt_addr);
    memcpy(patch_p, tmp.buf, tmp.len);
    a64_free(&tmp);

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
    a64_free(&b);
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
