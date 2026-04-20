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
    LDI R5, 0xFFF2
    LDI R6, 0xFFF0
fib:
    STR [R5], R0
    LDI R7, 32
    STR [R6], R7
    ADD R7, R0, R1
    MOV R0, R1
    MOV R1, R7
    SUB R2, R2, R3
    CMP R2, R4
    JNZ fib
    LDI R7, 10
    STR [R6], R7
    HALT
)";
    std::ofstream("test_fib.jvav") << src;
    int ret = system("jvavc\\back\\jvavc.exe test_fib.jvav test_fib.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system("jvm\\jvm.exe test_fib.bin > test_fib.out 2>&1");
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
    LDI R6, 0xFFF0
pstr:
    LDR R7, [R0]
    STR [R6], R7
    ADD R0, R0, R2
    SUB R1, R1, R2
    CMP R1, R4
    JNZ pstr
    HALT
msg: DB 72,101,108,108,111,44,32,87,111,114,108,100,33,10
)";
    std::ofstream("test_hello.jvav") << src;
    ret = system("jvavc\\back\\jvavc.exe test_hello.jvav test_hello.bin");
    TEST_ASSERT(ret == 0, "compilation failed");
    ret = system("jvm\\jvm.exe test_hello.bin > test_hello.out 2>&1");
    std::ifstream out2("test_hello.out");
    std::string output2((std::istreambuf_iterator<char>(out2)), std::istreambuf_iterator<char>());
    TEST_ASSERT(output2 == "Hello, World!\n", "hello output");
    std::remove("test_hello.jvav");
    std::remove("test_hello.bin");
    std::remove("test_hello.out");
    test_passed("integration_hello");

    return 0;
}
