#include "jit_arm64.h"
#include <stdlib.h>
#include <string.h>

void a64_init(A64Buf *b) { b->buf = NULL; b->len = 0; b->cap = 0; }
void a64_free(A64Buf *b) { free(b->buf); b->buf = NULL; b->len = b->cap = 0; }

static void a64_grow(A64Buf *b, size_t need) {
    if (b->len + need <= b->cap) return;
    size_t nc = b->cap ? b->cap : 256;
    while (nc < b->len + need) nc *= 2;
    b->buf = (uint8_t*)realloc(b->buf, nc);
    b->cap = nc;
}

void a64_emit(A64Buf *b, uint32_t insn) {
    a64_grow(b, 4);
    for (int i = 0; i < 4; i++) {
        b->buf[b->len++] = insn & 0xFF;
        insn >>= 8;
    }
}

void a64_emit8(A64Buf *b, uint64_t v) {
    a64_grow(b, 8);
    for (int i = 0; i < 8; i++) {
        b->buf[b->len++] = v & 0xFF;
        v >>= 8;
    }
}

void a64_mov_rr(A64Buf *b, int dst, int src) {
    if (dst == src) return;
    a64_emit(b, 0xAA000000 | a64_rm(src) | a64_rd(dst));
}

void a64_mov_ri(A64Buf *b, int dst, uint64_t imm) {
    int nz = 0;
    for (int s = 0; s < 64; s += 16) {
        uint16_t c = (imm >> s) & 0xFFFF;
        if (c) {
            a64_emit(b, (s == 0 ? 0xD2800000 : 0xF2800000)
                     | ((uint32_t)c << 5) | ((uint32_t)(s/16) << 21) | a64_rd(dst));
            nz = 1;
        } else if (!nz && s == 0) {
            a64_emit(b, 0xD2800000 | a64_rd(dst)); /* MOV Xd, #0 */
        }
    }
}

void a64_add_rr(A64Buf *b, int dst, int src) {
    a64_emit(b, 0x8B000000 | a64_rm(src) | a64_rn(dst) | a64_rd(dst));
}
void a64_sub_rr(A64Buf *b, int dst, int src) {
    a64_emit(b, 0xCB000000 | a64_rm(src) | a64_rn(dst) | a64_rd(dst));
}
void a64_mul_rr(A64Buf *b, int dst, int src) {
    a64_emit(b, 0x9B007C00 | a64_rm(src) | a64_rn(dst) | a64_rd(dst));
}
void a64_and_rr(A64Buf *b, int dst, int src) {
    a64_emit(b, 0x8A000000 | a64_rm(src) | a64_rn(dst) | a64_rd(dst));
}
void a64_or_rr(A64Buf *b, int dst, int src) {
    a64_emit(b, 0xAA000000 | a64_rm(src) | a64_rn(dst) | a64_rd(dst));
}
void a64_xor_rr(A64Buf *b, int dst, int src) {
    a64_emit(b, 0xCA000000 | a64_rm(src) | a64_rn(dst) | a64_rd(dst));
}
void a64_lsl_rr(A64Buf *b, int dst, int src) {
    a64_emit(b, 0x9AC02000 | a64_rm(src) | a64_rn(dst) | a64_rd(dst));
}
void a64_lsr_rr(A64Buf *b, int dst, int src) {
    a64_emit(b, 0x9AC02400 | a64_rm(src) | a64_rn(dst) | a64_rd(dst));
}
void a64_not_r(A64Buf *b, int dst) {
    a64_emit(b, 0xAA200000 | a64_rm(dst) | a64_rd(dst));
}
void a64_cmp_rr(A64Buf *b, int a, int b_reg) {
    a64_emit(b, 0xEB00001F | a64_rm(b_reg) | a64_rn(a));
}

static void a64_sub_ri(A64Buf *b, int dst, int imm) {
    if (imm == 0) return;
    if (imm < 0) { a64_add_ri(b, dst, -imm); return; }
    if ((unsigned)imm <= 0xFFF) {
        a64_emit(b, 0xD1000000 | ((uint32_t)imm << 10) | a64_rn(dst) | a64_rd(dst));
    } else if ((unsigned)imm <= 0xFFFFFF && (imm & 0xFFF) == 0) {
        a64_emit(b, 0xD1000000 | (1 << 22) | ((uint32_t)(imm >> 12) << 10) | a64_rn(dst) | a64_rd(dst));
    } else {
        a64_sub_ri(b, dst, imm & 0xFFF);
        a64_sub_ri(b, dst, imm & ~0xFFF);
    }
}

void a64_add_ri(A64Buf *b, int dst, int imm) {
    if (imm == 0) return;
    if (imm < 0) { a64_sub_ri(b, dst, -imm); return; }
    if ((unsigned)imm <= 0xFFF) {
        a64_emit(b, 0x91000000 | ((uint32_t)imm << 10) | a64_rn(dst) | a64_rd(dst));
    } else if ((unsigned)imm <= 0xFFFFFF && (imm & 0xFFF) == 0) {
        a64_emit(b, 0x91000000 | (1 << 22) | ((uint32_t)(imm >> 12) << 10) | a64_rn(dst) | a64_rd(dst));
    } else {
        a64_add_ri(b, dst, imm & 0xFFF);
        a64_add_ri(b, dst, imm & ~0xFFF);
    }
}

void a64_push_r(A64Buf *b, int reg) {
    a64_emit(b, 0xD10043FF); /* SUB SP, SP, #16 */
    a64_emit(b, 0xF90003E0 | a64_rd(reg)); /* STR Xt, [SP] */
}

void a64_pop_r(A64Buf *b, int reg) {
    a64_emit(b, 0xF94003E0 | a64_rd(reg)); /* LDR Xt, [SP] */
    a64_emit(b, 0x910043FF);                /* ADD SP, SP, #16 */
}

void a64_ret(A64Buf *b) { a64_emit(b, 0xD65F03C0); }
void a64_call_r(A64Buf *b, int reg) { a64_emit(b, 0xD61F0000 | a64_rn(reg)); }

void a64_load64(A64Buf *b, int dst, int base, int offset) {
    if (offset >= 0 && offset <= 32760 && (offset & 7) == 0) {
        a64_emit(b, 0xF9400000 | ((uint32_t)(offset >> 3) << 10) | a64_rn(base) | a64_rd(dst));
    } else {
        /* Use temp register */
        a64_mov_rr(b, JIT_SCR, base);
        a64_add_ri(b, JIT_SCR, offset);
        a64_emit(b, 0xF9400000 | a64_rn(JIT_SCR) | a64_rd(dst));
    }
}

void a64_store64(A64Buf *b, int src, int base, int offset) {
    if (offset >= 0 && offset <= 32760 && (offset & 7) == 0) {
        a64_emit(b, 0xF9000000 | ((uint32_t)(offset >> 3) << 10) | a64_rn(base) | a64_rd(src));
    } else {
        a64_mov_rr(b, JIT_SCR, base);
        a64_add_ri(b, JIT_SCR, offset);
        a64_emit(b, 0xF9000000 | a64_rn(JIT_SCR) | a64_rd(src));
    }
}

void a64_movabs(A64Buf *b, int dst, uint64_t addr) {
    a64_mov_ri(b, dst, addr);
}

void a64_cset(A64Buf *b, int dst, int cond) {
    a64_emit(b, 0x9A800000 | ((uint32_t)cond << 16) | a64_rd(dst));
}

void a64_br(A64Buf *b, int reg) {
    a64_emit(b, 0xD61F0000 | a64_rn(reg));
}

void a64_bcond_rel(A64Buf *b, int32_t offset, int cond) {
    uint32_t imm19 = ((uint32_t)offset >> 2) & 0x7FFFF;
    a64_emit(b, 0x54000000 | (imm19 << 5) | (cond & 0xF));
}
