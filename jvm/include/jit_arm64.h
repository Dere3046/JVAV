#ifndef JIT_ARM64_H
#define JIT_ARM64_H

#include <stdint.h>
#include <stddef.h>

enum {
    A64_R0, A64_R1, A64_R2, A64_R3, A64_R4, A64_R5, A64_R6, A64_R7,
    A64_R8, A64_R9, A64_R10, A64_R11, A64_R12, A64_R13, A64_R14, A64_R15,
    A64_R16, A64_R17, A64_R18, A64_R19, A64_R20, A64_R21, A64_R22, A64_R23,
    A64_R24, A64_R25, A64_R26, A64_R27, A64_R28, A64_R29, A64_R30
};

/*
 * JIT register mapping:
 *   X0-X7  → JIT_R0..JIT_R7   (JVM R0-R7)
 *   X8     → JIT_PC
 *   X9     → JIT_SP
 *   X10    → JIT_FL
 *   X11    → JIT_SCRATCH       (temp for address computation)
 *   X12    → JIT_DT            (dispatch table base)
 *   X19    → JIT_VM            (JVM struct pointer, callee-saved)
 */
enum {
    JIT_R0=A64_R0,  JIT_R1=A64_R1,  JIT_R2=A64_R2,  JIT_R3=A64_R3,
    JIT_R4=A64_R4,  JIT_R5=A64_R5,  JIT_R6=A64_R6,  JIT_R7=A64_R7,
    JIT_PC=A64_R8,  JIT_SP=A64_R9,  JIT_FL=A64_R10,
    JIT_SCR=A64_R11, JIT_DT=A64_R12, JIT_VM=A64_R19
};

typedef struct { uint8_t *buf; size_t len; size_t cap; } A64Buf;

void a64_init(A64Buf *b);
void a64_free(A64Buf *b);
void a64_emit(A64Buf *b, uint32_t insn);
void a64_emit8(A64Buf *b, uint64_t v);

/* Register field helpers */
static inline uint32_t a64_rd(int r)  { return (uint32_t)(r & 0x1F); }
static inline uint32_t a64_rn(int r)  { return (uint32_t)(r & 0x1F) << 5; }
static inline uint32_t a64_rm(int r)  { return (uint32_t)(r & 0x1F) << 16; }

/* Instructions */
void a64_mov_rr(A64Buf *b, int dst, int src);
void a64_mov_ri(A64Buf *b, int dst, uint64_t imm);
void a64_add_rr(A64Buf *b, int dst, int src);
void a64_sub_rr(A64Buf *b, int dst, int src);
void a64_mul_rr(A64Buf *b, int dst, int src);
void a64_and_rr(A64Buf *b, int dst, int src);
void a64_or_rr(A64Buf *b, int dst, int src);
void a64_xor_rr(A64Buf *b, int dst, int src);
void a64_lsl_rr(A64Buf *b, int dst, int src);
void a64_lsr_rr(A64Buf *b, int dst, int src);
void a64_not_r(A64Buf *b, int dst);
void a64_cmp_rr(A64Buf *b, int a, int b_reg);
void a64_add_ri(A64Buf *b, int dst, int imm);

void a64_push_r(A64Buf *b, int reg);
void a64_pop_r(A64Buf *b, int reg);
void a64_ret(A64Buf *b);
void a64_call_r(A64Buf *b, int reg);

void a64_load64(A64Buf *b, int dst, int base, int offset);
void a64_store64(A64Buf *b, int src, int base, int offset);
void a64_movabs(A64Buf *b, int dst, uint64_t addr);
void a64_cset(A64Buf *b, int dst, int cond);
void a64_br(A64Buf *b, int reg);
void a64_bcond_rel(A64Buf *b, int32_t offset, int cond);

/* ARM64 condition codes for B.cond / CSET */
#define A64_EQ  0x0
#define A64_NE  0x1
#define A64_LO  0x3
#define A64_HS  0x2
#define A64_LT  0xB
#define A64_GE  0xA

#endif
