#include "jit_x86_64.h"
#include <stdlib.h>
#include <string.h>

void x64_init(X64Buf *b) {
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
}

void x64_free(X64Buf *b) {
    free(b->buf);
    b->buf = NULL;
    b->len = b->cap = 0;
}

static void x64_grow(X64Buf *b, size_t need) {
    if (b->len + need <= b->cap) return;
    size_t newcap = b->cap ? b->cap : 256;
    while (newcap < b->len + need) newcap *= 2;
    b->buf = (uint8_t*)realloc(b->buf, newcap);
    b->cap = newcap;
}

void x64_emit1(X64Buf *b, uint8_t v) {
    x64_grow(b, 1);
    b->buf[b->len++] = v;
}

void x64_emit4(X64Buf *b, uint32_t v) {
    x64_grow(b, 4);
    b->buf[b->len++] = v & 0xFF;
    b->buf[b->len++] = (v >> 8) & 0xFF;
    b->buf[b->len++] = (v >> 16) & 0xFF;
    b->buf[b->len++] = (v >> 24) & 0xFF;
}

void x64_emit8(X64Buf *b, uint64_t v) {
    x64_grow(b, 8);
    for (int i = 0; i < 8; i++) {
        b->buf[b->len++] = v & 0xFF;
        v >>= 8;
    }
}

/* REX: [0 1 0 0] [W=8] [R=4] [X=2] [B=1] */
static int x64_rex_needed(int w, int r, int rm) {
    return (w ? 8 : 0) | (x64_hi(r) ? 4 : 0) | (x64_hi(rm) ? 1 : 0);
}

static void x64_emit_rex(X64Buf *b, int w, int r, int rm) {
    int v = x64_rex_needed(w, r, rm);
    if (v) x64_emit1(b, REX | v);
}

void x64_modrm(X64Buf *b, int mod, int reg, int rm) {
    x64_emit1(b, (uint8_t)(mod | (x64_lo(reg) << 3) | x64_lo(rm)));
}

void x64_sib(X64Buf *b, int scale, int idx, int base) {
    x64_emit1(b, (uint8_t)(((scale) << 6) | (x64_lo(idx) << 3) | x64_lo(base)));
}

/* RR: REX + opcode + ModR/M(reg, rm) */
static void x64_emit_rr_op(X64Buf *b, int w, int op, int dst, int src) {
    x64_emit_rex(b, w, dst, src);
    x64_emit1(b, (uint8_t)op);
    x64_modrm(b, MOD_REG, dst, src);
}

/* RM: REX + opcode + ModR/M + [SIB] + [disp] */
static void x64_emit_rm(X64Buf *b, int w, int opcode, int reg, int base, int offset) {
    int rex_bits = x64_rex_needed(w, reg, base);
    if (rex_bits) x64_emit1(b, REX | rex_bits);
    x64_emit1(b, (uint8_t)opcode);
    if (offset == 0 && base != X64_RBP && base != X64_R13) {
        x64_modrm(b, 0, reg, base);
    } else if (offset >= -128 && offset <= 127) {
        x64_modrm(b, MOD_DISP8, reg, base);
        x64_emit1(b, (uint8_t)(offset & 0xFF));
    } else {
        x64_modrm(b, MOD_DISP32, reg, base);
        x64_emit4(b, (uint32_t)offset);
    }
}

/* RMI: REX + opcode + ModR/M + SIB + [disp] */
static void x64_emit_rmi(X64Buf *b, int w, int opcode, int reg, int base, int idx, int scale) {
    int rex_bits = x64_rex_needed(w, reg, base) | (x64_hi(idx) ? 1 : 0);
    if (rex_bits) x64_emit1(b, REX | rex_bits);
    x64_emit1(b, (uint8_t)opcode);
    int s = scale == 8 ? 3 : scale == 4 ? 2 : scale == 2 ? 1 : 0;
    x64_modrm(b, 0, reg, 4);
    x64_sib(b, s, idx, base);
}

/* Register-only unary op: REX + 0xF7 + ModR/M(/, reg) */
static void x64_emit_unary(X64Buf *b, int subop, int reg) {
    x64_emit_rex(b, 1, 0, reg);
    x64_emit1(b, 0xF7);
    x64_modrm(b, MOD_REG, subop, reg);
}

/* ========== Public Instruction API ========== */

void x64_mov_rr(X64Buf *b, int dst, int src) {
    if (dst == src) return;
    x64_emit_rr_op(b, 1, 0x89, src, dst);   // mov src_reg, [dst_reg]
}

void x64_mov_ri32(X64Buf *b, int dst, uint32_t imm) {
    int rex = x64_hi(dst) ? REX_B : 0;
    if (rex) x64_emit1(b, rex);
    x64_emit1(b, 0xB8 | x64_lo(dst));
    x64_emit4(b, imm);
}

void x64_mov_ri64(X64Buf *b, int dst, uint64_t imm) {
    if ((int64_t)imm >= -0x80000000LL && (int64_t)imm <= 0x7FFFFFFFLL) {
        x64_mov_ri32(b, dst, (uint32_t)(int32_t)(int64_t)imm);
    } else {
        x64_emit_rex(b, 1, 0, dst);
        x64_emit1(b, 0xB8 | x64_lo(dst));
        x64_emit8(b, imm);
    }
}

void x64_add_rr(X64Buf *b, int dst, int src) {
    x64_emit_rr_op(b, 1, 0x01, src, dst);
}

void x64_sub_rr(X64Buf *b, int dst, int src) {
    x64_emit_rr_op(b, 1, 0x29, src, dst);
}

void x64_add_ri(X64Buf *b, int dst, int imm) {
    x64_emit_rex(b, 1, 0, dst);
    if (imm >= -128 && imm <= 127) {
        x64_emit1(b, 0x83);
        x64_modrm(b, MOD_REG, 0, dst);
        x64_emit1(b, (uint8_t)(imm & 0xFF));
    } else {
        x64_emit1(b, 0x81);
        x64_modrm(b, MOD_REG, 0, dst);
        x64_emit4(b, (uint32_t)imm);
    }
}

void x64_imul_rr(X64Buf *b, int dst, int src) {
    x64_emit_rex(b, 1, dst, src);
    x64_emit1(b, 0x0F);
    x64_emit1(b, 0xAF);
    x64_modrm(b, MOD_REG, dst, src);
}

void x64_and_rr(X64Buf *b, int dst, int src) {
    x64_emit_rr_op(b, 1, 0x21, src, dst);
}

void x64_or_rr(X64Buf *b, int dst, int src) {
    x64_emit_rr_op(b, 1, 0x09, src, dst);
}

void x64_xor_rr(X64Buf *b, int dst, int src) {
    x64_emit_rr_op(b, 1, 0x31, src, dst);
}

void x64_shl_cl(X64Buf *b, int dst) {
    x64_emit_rex(b, 1, 0, dst);
    x64_emit1(b, 0xD3);
    x64_modrm(b, MOD_REG, 4, dst);
}

void x64_shr_cl(X64Buf *b, int dst) {
    x64_emit_rex(b, 1, 0, dst);
    x64_emit1(b, 0xD3);
    x64_modrm(b, MOD_REG, 5, dst);
}

void x64_not_r(X64Buf *b, int dst) {
    x64_emit_unary(b, 2, dst);
}

void x64_neg_r(X64Buf *b, int dst) {
    x64_emit_unary(b, 3, dst);
}

void x64_cmp_rr(X64Buf *b, int a, int src) {
    x64_emit_rr_op(b, 1, 0x39, src, a);
}

void x64_test_rr(X64Buf *b, int a, int src) {
    x64_emit_rr_op(b, 1, 0x85, src, a);
}

void x64_push_r(X64Buf *b, int reg) {
    if (x64_hi(reg)) { x64_emit1(b, REX_B); }
    x64_emit1(b, 0x50 | x64_lo(reg));
}

void x64_pop_r(X64Buf *b, int reg) {
    if (x64_hi(reg)) { x64_emit1(b, REX_B); }
    x64_emit1(b, 0x58 | x64_lo(reg));
}

void x64_ret(X64Buf *b) {
    x64_emit1(b, 0xC3);
}

void x64_call_r(X64Buf *b, int reg) {
    if (x64_hi(reg)) x64_emit1(b, REX_B);
    x64_emit1(b, 0xFF);
    x64_modrm(b, MOD_REG, 2, reg);
}

void x64_jmp_rel(X64Buf *b, int delta) {
    x64_emit1(b, 0xE9);
    x64_emit4(b, (uint32_t)(int32_t)delta);
}

void x64_jmp_r(X64Buf *b, int reg) {
    if (x64_hi(reg)) x64_emit1(b, REX_B);
    x64_emit1(b, 0xFF);
    x64_modrm(b, MOD_REG, 4, reg);
}

void x64_jcc_rel(X64Buf *b, int delta, int cond) {
    x64_emit1(b, 0x0F);
    x64_emit1(b, (uint8_t)cond);
    x64_emit4(b, (uint32_t)(int32_t)delta);
}

void x64_setcc_r(X64Buf *b, int dst, int cond) {
    x64_emit_rex(b, 0, 0, dst);
    x64_emit1(b, 0x0F);
    x64_emit1(b, (uint8_t)cond);
    x64_modrm(b, MOD_REG, 0, dst);
}

void x64_load64(X64Buf *b, int dst, int base, int offset) {
    x64_emit_rm(b, 1, 0x8B, dst, base, offset);
}

void x64_store64(X64Buf *b, int src, int base, int offset) {
    x64_emit_rm(b, 1, 0x89, src, base, offset);
}

void x64_load64_idx(X64Buf *b, int dst, int base, int idx, int scale) {
    x64_emit_rmi(b, 1, 0x8B, dst, base, idx, scale);
}

void x64_store64_idx(X64Buf *b, int src, int base, int idx, int scale) {
    x64_emit_rmi(b, 1, 0x89, src, base, idx, scale);
}

void x64_movabs_r(X64Buf *b, int dst, uint64_t addr) {
    x64_emit_rex(b, 1, 0, dst);
    x64_emit1(b, 0xB8 | x64_lo(dst));
    x64_emit8(b, addr);
}

void x64_store_imm32(X64Buf *b, int base, int offset, uint32_t imm) {
    x64_emit_rex(b, 0, 0, base);
    x64_emit1(b, 0xC7);
    if (offset == 0 && base != X64_RBP && base != X64_R13) {
        x64_modrm(b, 0, 0, base);
    } else if (offset >= -128 && offset <= 127) {
        x64_modrm(b, MOD_DISP8, 0, base);
        x64_emit1(b, (uint8_t)(offset & 0xFF));
    } else {
        x64_modrm(b, MOD_DISP32, 0, base);
        x64_emit4(b, (uint32_t)offset);
    }
    x64_emit4(b, imm);
}
