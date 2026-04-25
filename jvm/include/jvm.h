#ifndef JVM_H
#define JVM_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "int128.hpp"

typedef Int128 var;
#define MEM_INITIAL   4096          /* initial RAM words */
#define MEM_MAX       (1LL<<30)     /* max RAM words (~16 GB) */
#define NUM_REGS      11
#define MAX_MMAP      16
#define MAX_FD        16

enum { R0, R1, R2, R3, R4, R5, R6, R7, PC, SP, FLAGS };

enum {
    OP_HALT = 0x00,
    OP_MOV,
    OP_LDR,
    OP_STR,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_CMP,
    OP_JMP,
    OP_JZ,
    OP_JNZ,
    OP_PUSH,
    OP_POP,
    OP_CALL,
    OP_RET,
    OP_LDI,
    OP_JE, OP_JNE, OP_JL, OP_JG, OP_JLE, OP_JGE,
    OP_MOD,
    OP_AND, OP_OR, OP_XOR, OP_SHL, OP_SHR, OP_NOT
};

#pragma pack(push, 1)
typedef struct {
    uint8_t op;
    uint8_t dst;
    uint8_t src1;
    uint8_t src2;
    uint64_t imm_low;
    uint32_t imm_high;
} instruction_t;
#pragma pack(pop)

/* Legacy basic I/O ports */
#define IO_ADDR_PUTCHAR   0xFFF0
#define IO_ADDR_GETCHAR   0xFFF1
#define IO_ADDR_PUTINT    0xFFF2
#define IO_ADDR_GETINT    0xFFF3
#define IO_ADDR_PUTHEX    0xFFF4

/* System-call mailbox */
#define SYSCALL_CMD       0xFFE0
#define SYSCALL_ARG0      0xFFE1
#define SYSCALL_ARG1      0xFFE2
#define SYSCALL_ARG2      0xFFE3
#define SYSCALL_RET       0xFFE4

enum {
    SYS_MMAP_FILE = 1,   /* ARG0=fname_addr, ARG1=map_addr, ARG2=size_words -> RET=handle(>0) or 0 */
    SYS_MUNMAP,          /* ARG0=handle -> RET=0 or -1 */
    SYS_MSYNC,           /* ARG0=handle -> RET=0 or -1 */
    SYS_FOPEN,           /* ARG0=fname_addr, ARG1=mode_addr -> RET=fd(>0) or 0 */
    SYS_FCLOSE,          /* ARG0=fd -> RET=0 or -1 */
    SYS_FREAD,           /* ARG0=fd, ARG1=buf_addr, ARG2=count_bytes -> RET=bytes or -1 */
    SYS_FWRITE,          /* ARG0=fd, ARG1=buf_addr, ARG2=count_bytes -> RET=bytes or -1 */
    SYS_FSEEK,           /* ARG0=fd, ARG1=offset, ARG2=whence -> RET=pos or -1 */
    SYS_FTELL,           /* ARG0=fd -> RET=pos or -1 */
    SYS_MEMCPY,          /* ARG0=dst, ARG1=src, ARG2=count_words */
    SYS_MEMSET,          /* ARG0=dst, ARG1=value, ARG2=count_words */
    SYS_MALLOC,          /* ARG0=size_words -> RET=addr or 0 */
    SYS_FREE             /* ARG0=addr -> RET=0 or -1 */
};

typedef struct {
    int active;
    var start;           /* VM word address */
    var size;            /* number of 128-bit words */
    FILE *file;
    long file_offset;    /* byte offset in file */
} mmap_entry_t;

#define STACK_GUARD 256     /* words reserved below code/data */

typedef struct {
    var *mem;            /* dynamically allocated RAM */
    var mem_capacity;    /* current RAM size in words */
    var reg[NUM_REGS + 3];
    int running;
    var mem_code_end;    /* end of loaded code+data (stack guard starts here) */
    /* heap allocator (bump allocator) */
    var heap_base;
    var heap_ptr;
    /* syscall mailbox */
    var syscall_arg0;
    var syscall_arg1;
    var syscall_arg2;
    var syscall_ret;
    /* memory mapping */
    mmap_entry_t mmap_table[MAX_MMAP];
    /* file descriptor table for raw file ops */
    FILE *fd_table[MAX_FD];
} JVM;

void jvm_init(JVM *vm);
void jvm_free(JVM *vm);
int jvm_load_program(JVM *vm, const char *filename);
void jvm_run(JVM *vm);

#define ZF (vm->reg[FLAGS] & 1)
#endif
