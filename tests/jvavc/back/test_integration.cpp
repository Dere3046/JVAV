#include "test_utils.hpp"
#include <cstdlib>
#include <fstream>
#include <cstdio>

int test_integration_main() {
    test_header("integration_fibonacci");
    const char* src = R"(
; Fibonacci test
    LDI R0, 0
    LDI R1, 1
    LDI R2, 10
    LDI R3, 1
    LDI R4, 0
fib:
    STR [0xFFE1], R0
    LDI R7, 15
    STR [0xFFE0], R7
    LDR R7, [0xFFE4]
    LDI R7, 32
    STR [0xFFE1], R7
    LDI R7, 14
    STR [0xFFE0], R7
    LDR R7, [0xFFE4]
    ADD R7, R0, R1
    MOV R0, R1
    MOV R1, R7
    SUB R2, R2, R3
    CMP R2, R4
    JNZ fib
    LDI R7, 10
    STR [0xFFE1], R7
    LDI R7, 14
    STR [0xFFE0], R7
    LDR R7, [0xFFE4]
    HALT
)";
    std::ofstream("test_fib.jvav") << src;
    int ret = system(JVAVC_BACK_EXE " test_fib.jvav test_fib.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_fib.bin > test_fib.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out("test_fib.out");
    std::string output((std::istreambuf_iterator<char>(out)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output.find("0 1 1 2 3 5 8 13 21 34") != std::string::npos, "fibonacci output");
    std::remove("test_fib.jvav");
    std::remove("test_fib.bin");
    std::remove("test_fib.out");
    test_passed("integration_fibonacci");

    test_header("integration_hello");
    src = R"(
    LDI R0, msg
    LDI R1, 14
    LDI R2, 1
    LDI R4, 0
pstr:
    LDR R7, [R0]
    STR [0xFFE1], R7
    LDI R7, 14
    STR [0xFFE0], R7
    LDR R7, [0xFFE4]
    ADD R0, R0, R2
    SUB R1, R1, R2
    CMP R1, R4
    JNZ pstr
    HALT
msg: DB 72,101,108,108,111,44,32,87,111,114,108,100,33,10
)";
    std::ofstream("test_hello.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_hello.jvav test_hello.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_hello.bin > test_hello.out 2>&1");
    std::ifstream out2("test_hello.out");
    std::string output2((std::istreambuf_iterator<char>(out2)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output2 == "Hello, World!\n", "hello output");
    std::remove("test_hello.jvav");
    std::remove("test_hello.bin");
    std::remove("test_hello.out");
    test_passed("integration_hello");

    test_header("integration_heap");
    src = R"(
    ; Test SYS_MALLOC and SYS_FREE
    LDI R0, 3
    STR [0xFFE1], R0      ; ARG0 = 3 words
    LDI R0, 12
    STR [0xFFE0], R0      ; CMD = SYS_MALLOC
    LDR R0, [0xFFE4]      ; R0 = allocated address
    ; Store values
    LDI R1, 7
    STR [R0], R1          ; addr[0] = 7
    LDI R1, 8
    ADD R2, R0, 1
    STR [R2], R1          ; addr[1] = 8
    LDI R1, 9
    ADD R2, R0, 2
    STR [R2], R1          ; addr[2] = 9
    ; Load and print sum
    LDR R1, [R0]
    ADD R2, R0, 1
    LDR R2, [R2]
    ADD R1, R1, R2
    ADD R2, R0, 2
    LDR R2, [R2]
    ADD R1, R1, R2
    STR [0xFFE1], R1      ; arg0 = sum
    LDI R7, 15
    STR [0xFFE0], R7      ; SYS_PUTINT
    LDR R7, [0xFFE4]
    ; Free
    STR [0xFFE1], R0
    LDI R0, 13
    STR [0xFFE0], R0      ; CMD = SYS_FREE
    HALT
)";
    std::ofstream("test_heap.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_heap.jvav test_heap.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_heap.bin > test_heap.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out3("test_heap.out");
    std::string output3((std::istreambuf_iterator<char>(out3)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output3 == "24", "heap output");
    std::remove("test_heap.jvav");
    std::remove("test_heap.bin");
    std::remove("test_heap.out");
    test_passed("integration_heap");

    // ------------------------------------------------------------------
    // NEW TESTS
    // ------------------------------------------------------------------

    test_header("integration_mov");
    src = R"(
    LDI R0, 7
    MOV R1, R0
    LDI R2, 0xFFF2
    STR [R2], R1
    HALT
)";
    std::ofstream("test_mov.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_mov.jvav test_mov.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_mov.bin > test_mov.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_mov("test_mov.out");
    std::string output_mov((std::istreambuf_iterator<char>(out_mov)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_mov == "7", "mov output");
    std::remove("test_mov.jvav");
    std::remove("test_mov.bin");
    std::remove("test_mov.out");
    test_passed("integration_mov");

    test_header("integration_ldi");
    src = R"(
    LDI R0, 42
    LDI R1, 0xFFF2
    STR [R1], R0
    HALT
)";
    std::ofstream("test_ldi.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_ldi.jvav test_ldi.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_ldi.bin > test_ldi.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_ldi("test_ldi.out");
    std::string output_ldi((std::istreambuf_iterator<char>(out_ldi)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_ldi == "42", "ldi output");
    std::remove("test_ldi.jvav");
    std::remove("test_ldi.bin");
    std::remove("test_ldi.out");
    test_passed("integration_ldi");

    test_header("integration_add");
    src = R"(
    LDI R0, 3
    LDI R1, 4
    ADD R2, R0, R1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_add.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_add.jvav test_add.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_add.bin > test_add.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_add("test_add.out");
    std::string output_add((std::istreambuf_iterator<char>(out_add)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_add == "7", "add output");
    std::remove("test_add.jvav");
    std::remove("test_add.bin");
    std::remove("test_add.out");
    test_passed("integration_add");

    test_header("integration_sub");
    src = R"(
    LDI R0, 10
    LDI R1, 3
    SUB R2, R0, R1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_sub.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_sub.jvav test_sub.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_sub.bin > test_sub.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_sub("test_sub.out");
    std::string output_sub((std::istreambuf_iterator<char>(out_sub)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_sub == "7", "sub output");
    std::remove("test_sub.jvav");
    std::remove("test_sub.bin");
    std::remove("test_sub.out");
    test_passed("integration_sub");

    test_header("integration_mul");
    src = R"(
    LDI R0, 6
    LDI R1, 7
    MUL R2, R0, R1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_mul.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_mul.jvav test_mul.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_mul.bin > test_mul.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_mul("test_mul.out");
    std::string output_mul((std::istreambuf_iterator<char>(out_mul)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_mul == "42", "mul output");
    std::remove("test_mul.jvav");
    std::remove("test_mul.bin");
    std::remove("test_mul.out");
    test_passed("integration_mul");

    test_header("integration_div");
    src = R"(
    LDI R0, 20
    LDI R1, 4
    DIV R2, R0, R1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_div.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_div.jvav test_div.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_div.bin > test_div.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_div("test_div.out");
    std::string output_div((std::istreambuf_iterator<char>(out_div)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_div == "5", "div output");
    std::remove("test_div.jvav");
    std::remove("test_div.bin");
    std::remove("test_div.out");
    test_passed("integration_div");

    test_header("integration_jmp");
    src = R"(
    JMP target
    LDI R0, 1
    LDI R1, 0xFFF2
    STR [R1], R0
    HALT
target:
    LDI R0, 11
    LDI R1, 0xFFF2
    STR [R1], R0
    HALT
)";
    std::ofstream("test_jmp.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_jmp.jvav test_jmp.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_jmp.bin > test_jmp.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_jmp("test_jmp.out");
    std::string output_jmp((std::istreambuf_iterator<char>(out_jmp)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_jmp == "11", "jmp output");
    std::remove("test_jmp.jvav");
    std::remove("test_jmp.bin");
    std::remove("test_jmp.out");
    test_passed("integration_jmp");

    test_header("integration_cmp_jz");
    src = R"(
    LDI R0, 0
    LDI R1, 0
    CMP R0, R1
    JZ is_zero
    LDI R2, 1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
is_zero:
    LDI R2, 2
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_cmp_jz.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_cmp_jz.jvav test_cmp_jz.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_cmp_jz.bin > test_cmp_jz.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_cmp_jz("test_cmp_jz.out");
    std::string output_cmp_jz((std::istreambuf_iterator<char>(out_cmp_jz)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_cmp_jz == "2", "cmp_jz output");
    std::remove("test_cmp_jz.jvav");
    std::remove("test_cmp_jz.bin");
    std::remove("test_cmp_jz.out");
    test_passed("integration_cmp_jz");

    test_header("integration_cmp_jnz");
    src = R"(
    LDI R0, 1
    LDI R1, 0
    CMP R0, R1
    JNZ not_zero
    LDI R2, 1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
not_zero:
    LDI R2, 3
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_cmp_jnz.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_cmp_jnz.jvav test_cmp_jnz.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_cmp_jnz.bin > test_cmp_jnz.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_cmp_jnz("test_cmp_jnz.out");
    std::string output_cmp_jnz((std::istreambuf_iterator<char>(out_cmp_jnz)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_cmp_jnz == "3", "cmp_jnz output");
    std::remove("test_cmp_jnz.jvav");
    std::remove("test_cmp_jnz.bin");
    std::remove("test_cmp_jnz.out");
    test_passed("integration_cmp_jnz");

    test_header("integration_cmp_je");
    src = R"(
    LDI R0, 5
    LDI R1, 5
    CMP R0, R1
    JE equal
    LDI R2, 1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
equal:
    LDI R2, 4
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_cmp_je.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_cmp_je.jvav test_cmp_je.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_cmp_je.bin > test_cmp_je.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_cmp_je("test_cmp_je.out");
    std::string output_cmp_je((std::istreambuf_iterator<char>(out_cmp_je)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_cmp_je == "4", "cmp_je output");
    std::remove("test_cmp_je.jvav");
    std::remove("test_cmp_je.bin");
    std::remove("test_cmp_je.out");
    test_passed("integration_cmp_je");

    test_header("integration_cmp_jne");
    src = R"(
    LDI R0, 5
    LDI R1, 3
    CMP R0, R1
    JNE not_equal
    LDI R2, 1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
not_equal:
    LDI R2, 5
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_cmp_jne.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_cmp_jne.jvav test_cmp_jne.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_cmp_jne.bin > test_cmp_jne.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_cmp_jne("test_cmp_jne.out");
    std::string output_cmp_jne((std::istreambuf_iterator<char>(out_cmp_jne)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_cmp_jne == "5", "cmp_jne output");
    std::remove("test_cmp_jne.jvav");
    std::remove("test_cmp_jne.bin");
    std::remove("test_cmp_jne.out");
    test_passed("integration_cmp_jne");

    test_header("integration_cmp_jl");
    src = R"(
    LDI R0, 2
    LDI R1, 5
    CMP R0, R1
    JL less
    LDI R2, 1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
less:
    LDI R2, 6
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_cmp_jl.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_cmp_jl.jvav test_cmp_jl.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_cmp_jl.bin > test_cmp_jl.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_cmp_jl("test_cmp_jl.out");
    std::string output_cmp_jl((std::istreambuf_iterator<char>(out_cmp_jl)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_cmp_jl == "6", "cmp_jl output");
    std::remove("test_cmp_jl.jvav");
    std::remove("test_cmp_jl.bin");
    std::remove("test_cmp_jl.out");
    test_passed("integration_cmp_jl");

    test_header("integration_cmp_jg");
    src = R"(
    LDI R0, 7
    LDI R1, 3
    CMP R0, R1
    JG greater
    LDI R2, 1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
greater:
    LDI R2, 7
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_cmp_jg.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_cmp_jg.jvav test_cmp_jg.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_cmp_jg.bin > test_cmp_jg.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_cmp_jg("test_cmp_jg.out");
    std::string output_cmp_jg((std::istreambuf_iterator<char>(out_cmp_jg)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_cmp_jg == "7", "cmp_jg output");
    std::remove("test_cmp_jg.jvav");
    std::remove("test_cmp_jg.bin");
    std::remove("test_cmp_jg.out");
    test_passed("integration_cmp_jg");

    test_header("integration_cmp_jle");
    src = R"(
    LDI R0, 3
    LDI R1, 5
    CMP R0, R1
    JLE le
    LDI R2, 1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
le:
    LDI R2, 8
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_cmp_jle.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_cmp_jle.jvav test_cmp_jle.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_cmp_jle.bin > test_cmp_jle.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_cmp_jle("test_cmp_jle.out");
    std::string output_cmp_jle((std::istreambuf_iterator<char>(out_cmp_jle)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_cmp_jle == "8", "cmp_jle output");
    std::remove("test_cmp_jle.jvav");
    std::remove("test_cmp_jle.bin");
    std::remove("test_cmp_jle.out");
    test_passed("integration_cmp_jle");

    test_header("integration_cmp_jge");
    src = R"(
    LDI R0, 9
    LDI R1, 5
    CMP R0, R1
    JGE ge
    LDI R2, 1
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
ge:
    LDI R2, 9
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_cmp_jge.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_cmp_jge.jvav test_cmp_jge.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_cmp_jge.bin > test_cmp_jge.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_cmp_jge("test_cmp_jge.out");
    std::string output_cmp_jge((std::istreambuf_iterator<char>(out_cmp_jge)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_cmp_jge == "9", "cmp_jge output");
    std::remove("test_cmp_jge.jvav");
    std::remove("test_cmp_jge.bin");
    std::remove("test_cmp_jge.out");
    test_passed("integration_cmp_jge");

    test_header("integration_push_pop");
    src = R"(
    LDI R0, 5
    LDI R1, 6
    PUSH R0
    PUSH R1
    POP R2
    POP R3
    LDI R4, 0xFFF2
    STR [R4], R2
    STR [R4], R3
    HALT
)";
    std::ofstream("test_push_pop.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_push_pop.jvav test_push_pop.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_push_pop.bin > test_push_pop.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_push_pop("test_push_pop.out");
    std::string output_push_pop((std::istreambuf_iterator<char>(out_push_pop)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_push_pop == "65", "push_pop output");
    std::remove("test_push_pop.jvav");
    std::remove("test_push_pop.bin");
    std::remove("test_push_pop.out");
    test_passed("integration_push_pop");

    test_header("integration_call_ret");
    src = R"(
    CALL func
    LDI R2, 0xFFF2
    STR [R2], R0
    HALT
func:
    LDI R0, 42
    RET
)";
    std::ofstream("test_call_ret.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_call_ret.jvav test_call_ret.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_call_ret.bin > test_call_ret.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_call_ret("test_call_ret.out");
    std::string output_call_ret((std::istreambuf_iterator<char>(out_call_ret)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_call_ret == "42", "call_ret output");
    std::remove("test_call_ret.jvav");
    std::remove("test_call_ret.bin");
    std::remove("test_call_ret.out");
    test_passed("integration_call_ret");

    test_header("integration_ldr_str_direct");
    src = R"(
    LDI R1, 77
    STR [0x100], R1
    LDR R2, [0x100]
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_ldr_str_direct.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_ldr_str_direct.jvav test_ldr_str_direct.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_ldr_str_direct.bin > test_ldr_str_direct.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_ldr_str_direct("test_ldr_str_direct.out");
    std::string output_ldr_str_direct((std::istreambuf_iterator<char>(out_ldr_str_direct)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_ldr_str_direct == "77", "ldr_str_direct output");
    std::remove("test_ldr_str_direct.jvav");
    std::remove("test_ldr_str_direct.bin");
    std::remove("test_ldr_str_direct.out");
    test_passed("integration_ldr_str_direct");

    test_header("integration_ldr_str_indirect");
    src = R"(
    LDI R0, 0x100
    LDI R1, 88
    STR [R0], R1
    LDR R2, [R0]
    LDI R3, 0xFFF2
    STR [R3], R2
    HALT
)";
    std::ofstream("test_ldr_str_indirect.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_ldr_str_indirect.jvav test_ldr_str_indirect.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_ldr_str_indirect.bin > test_ldr_str_indirect.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_ldr_str_indirect("test_ldr_str_indirect.out");
    std::string output_ldr_str_indirect((std::istreambuf_iterator<char>(out_ldr_str_indirect)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_ldr_str_indirect == "88", "ldr_str_indirect output");
    std::remove("test_ldr_str_indirect.jvav");
    std::remove("test_ldr_str_indirect.bin");
    std::remove("test_ldr_str_indirect.out");
    test_passed("integration_ldr_str_indirect");

    test_header("integration_equ");
    src = R"(
    VAL EQU 33
    LDI R0, VAL
    LDI R1, 0xFFF2
    STR [R1], R0
    HALT
)";
    std::ofstream("test_equ.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_equ.jvav test_equ.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_equ.bin > test_equ.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_equ("test_equ.out");
    std::string output_equ((std::istreambuf_iterator<char>(out_equ)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_equ == "33", "equ output");
    std::remove("test_equ.jvav");
    std::remove("test_equ.bin");
    std::remove("test_equ.out");
    test_passed("integration_equ");

    test_header("integration_db");
    src = R"(
    LDI R0, data
    LDR R1, [R0]
    LDI R2, 0xFFF0
    STR [R2], R1
    HALT
data: DB 65
)";
    std::ofstream("test_db.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_db.jvav test_db.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_db.bin > test_db.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_db("test_db.out");
    std::string output_db((std::istreambuf_iterator<char>(out_db)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_db == "A", "db output");
    std::remove("test_db.jvav");
    std::remove("test_db.bin");
    std::remove("test_db.out");
    test_passed("integration_db");

    test_header("integration_include");
    {
        std::ofstream("test_inc_header.jvav") << "INC_VAL EQU 55\n";
        src = R"(
#include "test_inc_header.jvav"
    LDI R0, INC_VAL
    LDI R1, 0xFFF2
    STR [R1], R0
    HALT
)";
        std::ofstream("test_include.jvav") << src;
        ret = system(JVAVC_BACK_EXE " test_include.jvav test_include.bin");
        TEST_ASSERT(ret == 0, "compilation failed");
        ret = system(JVM_EXE " test_include.bin > test_include.out 2>&1");
        TEST_ASSERT(ret == 0, "execution failed");
        std::ifstream out_include("test_include.out");
        std::string output_include((std::istreambuf_iterator<char>(out_include)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output_include == "55", "include output");
        std::remove("test_inc_header.jvav");
        std::remove("test_include.jvav");
        std::remove("test_include.bin");
        std::remove("test_include.out");
    }
    test_passed("integration_include");

    test_header("integration_linking");
    {
        std::ofstream("test_link_lib.jvav") << R"(
.global getval
getval:
    LDI R0, 99
    RET
)";
        std::ofstream("test_link_main.jvav") << R"(
.extern getval
.global _start
_start:
    CALL getval
    LDI R1, 0xFFF2
    STR [R1], R0
    HALT
)";
        ret = system(JVAVC_BACK_EXE " test_link_main.jvav test_link_lib.jvav -o test_link.bin");
        TEST_ASSERT(ret == 0, "link compilation failed");
        ret = system(JVM_EXE " test_link.bin > test_link.out 2>&1");
        TEST_ASSERT(ret == 0, "execution failed");
        std::ifstream out_link("test_link.out");
        std::string output_link((std::istreambuf_iterator<char>(out_link)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output_link == "99", "link output");
        std::remove("test_link_lib.jvav");
        std::remove("test_link_main.jvav");
        std::remove("test_link.bin");
        std::remove("test_link.out");
    }
    test_passed("integration_linking");

    test_header("integration_stack");
    src = R"(
    LDI R0, 1
    LDI R1, 2
    LDI R2, 3
    PUSH R0
    PUSH R1
    PUSH R2
    POP R3
    POP R4
    POP R5
    LDI R6, 0xFFF2
    STR [R6], R3
    STR [R6], R4
    STR [R6], R5
    HALT
)";
    std::ofstream("test_stack.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_stack.jvav test_stack.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_stack.bin > test_stack.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_stack("test_stack.out");
    std::string output_stack((std::istreambuf_iterator<char>(out_stack)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_stack == "321", "stack output");
    std::remove("test_stack.jvav");
    std::remove("test_stack.bin");
    std::remove("test_stack.out");
    test_passed("integration_stack");

    test_header("integration_bitops");
    src = R"(
    LDI R0, 0x0F
    LDI R1, 0xF0
    AND R2, R0, R1
    LDI R5, 0xFFF2
    STR [R5], R2
    LDI R6, 0xFFF0
    LDI R7, 10
    STR [R6], R7

    OR R2, R0, R1
    STR [R5], R2
    STR [R6], R7

    LDI R1, 0xFF
    XOR R2, R0, R1
    STR [R5], R2
    STR [R6], R7

    LDI R0, 1
    LDI R1, 3
    SHL R2, R0, R1
    STR [R5], R2
    STR [R6], R7

    LDI R0, 16
    LDI R1, 2
    SHR R2, R0, R1
    STR [R5], R2
    STR [R6], R7

    LDI R0, 0
    NOT R2, R0
    STR [R5], R2
    STR [R6], R7

    HALT
)";
    std::ofstream("test_bitops.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_bitops.jvav test_bitops.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system(JVM_EXE " test_bitops.bin > test_bitops.out 2>&1");
    TEST_ASSERT(ret == 0, "execution failed");
    std::ifstream out_bitops("test_bitops.out");
    std::string output_bitops((std::istreambuf_iterator<char>(out_bitops)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output_bitops == "0\n255\n240\n8\n4\n-1\n", "bitops output");
    std::remove("test_bitops.jvav");
    std::remove("test_bitops.bin");
    std::remove("test_bitops.out");
    test_passed("integration_bitops");

    /* ---------- .syscall edge cases ---------- */

    test_header("integration_syscall_arg0");
    src = R"(
    .global _start
_start:
    HALT
    .syscall getchar, 16, 0
)";
    std::ofstream("test_sc0.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_sc0.jvav test_sc0.bin");
    TEST_ASSERT(ret == 0, "syscall arg0 compile failed");
    std::remove("test_sc0.jvav");
    std::remove("test_sc0.bin");
    test_passed("integration_syscall_arg0");

    test_header("integration_syscall_arg1");
    src = R"(
    .global _start
_start:
    LDI R0, 123
    PUSH R0
    LDI R4, 0
    ADD R4, SP, R4
    LDR R0, [R4]
    CALL putint
    LDI R4, 1
    ADD SP, SP, R4
    HALT
    .syscall putint, 15, 1
)";
    std::ofstream("test_sc1.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_sc1.jvav test_sc1.bin");
    TEST_ASSERT(ret == 0, "syscall arg1 compile failed");
    ret = system(JVM_EXE " test_sc1.bin > test_sc1.out 2>&1");
    TEST_ASSERT(ret == 0, "syscall arg1 execution failed");
    {
        std::ifstream out_sc1("test_sc1.out");
        std::string output_sc1((std::istreambuf_iterator<char>(out_sc1)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output_sc1 == "123", "syscall arg1 output");
    }
    std::remove("test_sc1.jvav");
    std::remove("test_sc1.bin");
    std::remove("test_sc1.out");
    test_passed("integration_syscall_arg1");

    test_header("integration_syscall_arg2");
    src = R"(
    .global _start
_start:
    LDI R0, 77
    PUSH R0
    CALL putint
    LDI R4, 1
    ADD SP, SP, R4
    HALT
    .syscall putint, 15, 1
    .syscall getchar, 16, 0
    .syscall alloc, 12, 1
)";
    std::ofstream("test_sc2.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_sc2.jvav test_sc2.bin");
    TEST_ASSERT(ret == 0, "syscall arg2 compile failed");
    ret = system(JVM_EXE " test_sc2.bin > test_sc2.out 2>&1");
    TEST_ASSERT(ret == 0, "syscall arg2 execution failed");
    {
        std::ifstream out_sc2("test_sc2.out");
        std::string output_sc2((std::istreambuf_iterator<char>(out_sc2)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output_sc2 == "77", "syscall arg2 output");
    }
    std::remove("test_sc2.jvav");
    std::remove("test_sc2.bin");
    std::remove("test_sc2.out");
    test_passed("integration_syscall_arg2");

    test_header("integration_syscall_arg3");
    src = R"(
    .global _start
_start:
    LDI R0, 65
    PUSH R0
    CALL putchar
    LDI R4, 1
    ADD SP, SP, R4
    LDI R0, 88
    PUSH R0
    CALL putint
    LDI R4, 1
    ADD SP, SP, R4
    HALT
    .syscall putchar, 14, 1
    .syscall putint, 15, 1
    .syscall getchar, 16, 0
    .syscall getint, 17, 0
)";
    std::ofstream("test_sc3.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_sc3.jvav test_sc3.bin");
    TEST_ASSERT(ret == 0, "syscall arg3 compile failed");
    ret = system(JVM_EXE " test_sc3.bin > test_sc3.out 2>&1");
    TEST_ASSERT(ret == 0, "syscall arg3 execution failed");
    {
        std::ifstream out_sc3("test_sc3.out");
        std::string output_sc3((std::istreambuf_iterator<char>(out_sc3)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output_sc3 == "A88", "syscall arg3 output");
    }
    std::remove("test_sc3.jvav");
    std::remove("test_sc3.bin");
    std::remove("test_sc3.out");
    test_passed("integration_syscall_arg3");

    test_header("integration_syscall_bad_arg_count");
    src = R"(
    .syscall bad, 1, -1
    HALT
)";
    std::ofstream("test_sc_bad.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_sc_bad.jvav test_sc_bad.bin > test_sc_bad.out 2>&1");
    TEST_ASSERT(ret != 0, "syscall bad arg_count should fail");
    {
        std::ifstream out_bad("test_sc_bad.out");
        std::string msg((std::istreambuf_iterator<char>(out_bad)), std::istreambuf_iterator<char>());
        TEST_ASSERT(msg.find("arg_count") != std::string::npos || msg.find("Bad") != std::string::npos, "expected arg_count error");
    }
    std::remove("test_sc_bad.jvav");
    std::remove("test_sc_bad.bin");
    std::remove("test_sc_bad.out");
    test_passed("integration_syscall_bad_arg_count");

    test_header("integration_syscall_too_many_args");
    src = R"(
    .syscall bad, 1, 4
    HALT
)";
    std::ofstream("test_sc_4.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_sc_4.jvav test_sc_4.bin > test_sc_4.out 2>&1");
    TEST_ASSERT(ret != 0, "syscall too many args should fail");
    {
        std::ifstream out_4("test_sc_4.out");
        std::string msg((std::istreambuf_iterator<char>(out_4)), std::istreambuf_iterator<char>());
        TEST_ASSERT(msg.find("0..3") != std::string::npos || msg.find("arg_count") != std::string::npos, "expected 0..3 error");
    }
    std::remove("test_sc_4.jvav");
    std::remove("test_sc_4.bin");
    std::remove("test_sc_4.out");
    test_passed("integration_syscall_too_many_args");

    test_header("integration_syscall_with_user_func");
    src = R"(
    .global _start
_start:
    LDI R0, 99
    PUSH R0
    CALL putint
    LDI R4, 1
    ADD SP, SP, R4
    HALT
    .syscall putint, 15, 1
)";
    std::ofstream("test_sc_mix.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_sc_mix.jvav test_sc_mix.bin");
    TEST_ASSERT(ret == 0, "syscall mix compile failed");
    ret = system(JVM_EXE " test_sc_mix.bin > test_sc_mix.out 2>&1");
    TEST_ASSERT(ret == 0, "syscall mix execution failed");
    {
        std::ifstream out_mix("test_sc_mix.out");
        std::string output_mix((std::istreambuf_iterator<char>(out_mix)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output_mix == "99", "syscall mix output");
    }
    std::remove("test_sc_mix.jvav");
    std::remove("test_sc_mix.bin");
    std::remove("test_sc_mix.out");
    test_passed("integration_syscall_with_user_func");

    test_header("integration_syscall_negative");
    src = R"(
    .global _start
_start:
    LDI R0, 42
    SUB R0, R4, R0
    PUSH R0
    CALL putint
    LDI R4, 1
    ADD SP, SP, R4
    HALT
    .syscall putint, 15, 1
)";
    std::ofstream("test_sc_neg.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_sc_neg.jvav test_sc_neg.bin");
    TEST_ASSERT(ret == 0, "syscall negative compile failed");
    ret = system(JVM_EXE " test_sc_neg.bin > test_sc_neg.out 2>&1");
    TEST_ASSERT(ret == 0, "syscall negative execution failed");
    {
        std::ifstream out_neg("test_sc_neg.out");
        std::string output_neg((std::istreambuf_iterator<char>(out_neg)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output_neg == "-42", "syscall negative output");
    }
    std::remove("test_sc_neg.jvav");
    std::remove("test_sc_neg.bin");
    std::remove("test_sc_neg.out");
    test_passed("integration_syscall_negative");

    test_header("integration_syscall_zero");
    src = R"(
    .global _start
_start:
    LDI R0, 0
    PUSH R0
    CALL putint
    LDI R4, 1
    ADD SP, SP, R4
    HALT
    .syscall putint, 15, 1
)";
    std::ofstream("test_sc_zero.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_sc_zero.jvav test_sc_zero.bin");
    TEST_ASSERT(ret == 0, "syscall zero compile failed");
    ret = system(JVM_EXE " test_sc_zero.bin > test_sc_zero.out 2>&1");
    TEST_ASSERT(ret == 0, "syscall zero execution failed");
    {
        std::ifstream out_zero("test_sc_zero.out");
        std::string output_zero((std::istreambuf_iterator<char>(out_zero)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output_zero == "0", "syscall zero output");
    }
    std::remove("test_sc_zero.jvav");
    std::remove("test_sc_zero.bin");
    std::remove("test_sc_zero.out");
    test_passed("integration_syscall_zero");

    test_header("integration_syscall_seq");
    src = R"(
    .global _start
_start:
    LDI R0, 65
    PUSH R0
    CALL putchar
    LDI R4, 1
    ADD SP, SP, R4
    LDI R0, 1
    PUSH R0
    CALL putint
    LDI R4, 1
    ADD SP, SP, R4
    LDI R0, 66
    PUSH R0
    CALL putchar
    LDI R4, 1
    ADD SP, SP, R4
    HALT
    .syscall putchar, 14, 1
    .syscall putint, 15, 1
)";
    std::ofstream("test_sc_seq.jvav") << src;
    ret = system(JVAVC_BACK_EXE " test_sc_seq.jvav test_sc_seq.bin");
    TEST_ASSERT(ret == 0, "syscall seq compile failed");
    ret = system(JVM_EXE " test_sc_seq.bin > test_sc_seq.out 2>&1");
    TEST_ASSERT(ret == 0, "syscall seq execution failed");
    {
        std::ifstream out_seq("test_sc_seq.out");
        std::string output_seq((std::istreambuf_iterator<char>(out_seq)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output_seq == "A1B", "syscall seq output");
    }
    std::remove("test_sc_seq.jvav");
    std::remove("test_sc_seq.bin");
    std::remove("test_sc_seq.out");
    test_passed("integration_syscall_seq");

    return 0;
}
