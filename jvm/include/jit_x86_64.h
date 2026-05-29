#ifndef JIT_X86_64_H
#define JIT_X86_64_H

#include <stdint.h>
#include <stddef.h>

/* x86-64 REX prefixes */
#define REX_W   0x48
#define REX_R   0x44
#define REX_X   0x42
#define REX_B   0x41
#define REX     0x40

/* ModR/M modes */
#define MOD_REG 0xC0
#define MOD_DISP8  0x40
#define MOD_DISP32 0x80

/* x86-64 registers */
enum {
    X64_RAX, X64_RCX, X64_RDX, X64_RBX,
    X64_RSP, X64_RBP, X64_RSI, X64_RDI,
    X64_R8,  X64_R9,  X64_R10, X64_R11,
    X64_R12, X64_R13, X64_R14, X64_R15
};

/* JVM reg → x86-64 reg mapping */
enum {
    JIT_R0 = X64_RAX, JIT_R1 = X64_RCX, JIT_R2 = X64_RDX, JIT_R3 = X64_RBX,
    JIT_R4 = X64_R8,  JIT_R5 = X64_R9,  JIT_R6 = X64_R10, JIT_R7 = X64_R11,
    JIT_PC = X64_R12, JIT_SP = X64_R13, JIT_FL = X64_R14,
    JIT_TMP = X64_R15   /* scratch register */
};

/* JVAV instruction structure from jvm.h */
typedef struct {
    uint8_t op;
    uint8_t dst;
    uint8_t src1;
    uint8_t src2;
    uint64_t imm_low;
    uint32_t imm_high;
} __attribute__((packed)) JITInstr;

/* Code buffer */
typedef struct {
    uint8_t *buf;
    size_t len;
    size_t cap;
} X64Buf;

void x64_init(X64Buf *b);
void x64_free(X64Buf *b);

/* Low-level emission */
void x64_emit1(X64Buf *b, uint8_t v);
void x64_emit4(X64Buf *b, uint32_t v);
void x64_emit8(X64Buf *b, uint64_t v);
void x64_rex(X64Buf *b, int w, int r, int x, int base);
void x64_modrm(X64Buf *b, int mod, int reg, int rm);
void x64_rex_modrm(X64Buf *b, int w, int r, int rm);

/* REX helpers */
static inline int x64_hi(int r) { return r > 7 ? 1 : 0; }
static inline int x64_lo(int r) { return r & 7; }

/* Instruction emission (64-bit mode) */
void x64_mov_rr(X64Buf *b, int dst, int src);
void x64_mov_ri64(X64Buf *b, int dst, uint64_t imm);
void x64_mov_ri32(X64Buf *b, int dst, uint32_t imm);
void x64_add_rr(X64Buf *b, int dst, int src);
void x64_sub_rr(X64Buf *b, int dst, int src);
void x64_add_ri(X64Buf *b, int dst, int imm);
void x64_imul_rr(X64Buf *b, int dst, int src);
void x64_and_rr(X64Buf *b, int dst, int src);
void x64_or_rr(X64Buf *b, int dst, int src);
void x64_xor_rr(X64Buf *b, int dst, int src);
void x64_shl_cl(X64Buf *b, int dst);
void x64_shr_cl(X64Buf *b, int dst);
void x64_not_r(X64Buf *b, int dst);
void x64_neg_r(X64Buf *b, int dst);
void x64_cmp_rr(X64Buf *b, int a, int src);
void x64_test_rr(X64Buf *b, int a, int src);
void x64_push_r(X64Buf *b, int reg);
void x64_pop_r(X64Buf *b, int reg);
void x64_ret(X64Buf *b);
void x64_call_r(X64Buf *b, int reg);

void x64_jmp_rel(X64Buf *b, int delta);
void x64_jmp_r(X64Buf *b, int reg);

/* Conditional jumps: cond = 0x84(je), 0x85(jne), 0x8c(jl), 0x8f(jg), etc */
void x64_jcc_rel(X64Buf *b, int delta, int cond);
void x64_setcc_r(X64Buf *b, int dst, int cond);

/* Memory operations: 64-bit, base addressing */
void x64_load64(X64Buf *b, int dst, int base, int offset);
void x64_store64(X64Buf *b, int src, int base, int offset);
void x64_load64_idx(X64Buf *b, int dst, int base, int idx, int scale);
void x64_store64_idx(X64Buf *b, int src, int base, int idx, int scale);
void x64_store_imm32(X64Buf *b, int base, int offset, uint32_t imm);

/* Mov with absolute address (for calling C functions) */
void x64_movabs_r(X64Buf *b, int dst, uint64_t addr);

/* x86-64 conditional codes (Jcc / CMOVcc / SETcc) */
#define X64_JO   0x80
#define X64_JNO  0x81
#define X64_JB   0x82  /* JC, JNAE */
#define X64_JAE  0x83  /* JNB, JNC */
#define X64_JE   0x84
#define X64_JNE  0x85
#define X64_JBE  0x86
#define X64_JA   0x87
#define X64_JS   0x88
#define X64_JNS  0x89
#define X64_JP   0x8A
#define X64_JNP  0x8B
#define X64_JL   0x8C
#define X64_JGE  0x8D
#define X64_JLE  0x8E
#define X64_JG   0x8F

#endif
