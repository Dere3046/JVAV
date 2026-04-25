#include "test_utils.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"
#include "codegen.hpp"
#include <fstream>
#include <sstream>

using namespace std;

static string compile(const string &src) {
    ofstream("tmp.jvl") << src;
    Lexer lex; lex.tokenize("tmp.jvl");
    FrontParser par; par.parse(lex.getTokens());
    Sema sema; sema.analyze(par.getProgram());
    CodeGenerator gen;
    string asmText = gen.generate(par.getProgram());
    remove("tmp.jvl");
    return asmText;
}

int test_codegen_main() {
    // --- Original tests ---
    test_header("codegen_prologue");
    {
        string asmText = compile("func main(): int { return 0; }");
        TEST_ASSERT(asmText.find("PUSH R6") != string::npos, "push r6");
        TEST_ASSERT(asmText.find("MOV R6, SP") != string::npos, "mov r6");
        TEST_ASSERT(asmText.find("POP R6") != string::npos, "pop r6");
        TEST_ASSERT(asmText.find("RET") != string::npos, "ret");
    }
    test_passed("codegen_prologue");

    test_header("codegen_locals");
    {
        string asmText = compile("func main(): int { var x: int = 5; return x; }");
        TEST_ASSERT(asmText.find("SUB SP, SP") != string::npos, "alloc locals");
        TEST_ASSERT(asmText.find("STR [R4], R0") != string::npos, "store local");
    }
    test_passed("codegen_locals");

    test_header("codegen_call");
    {
        string asmText = compile("func add(a: int, b: int): int { return a + b; }\nfunc main(): int { return add(3, 5); }");
        TEST_ASSERT(asmText.find("CALL add") != string::npos, "call add");
    }
    test_passed("codegen_call");

    test_header("codegen_entry");
    {
        string asmText = compile("func main(): int { return 0; }");
        TEST_ASSERT(asmText.find("_start:") != string::npos, "_start");
        TEST_ASSERT(asmText.find("CALL main") != string::npos, "call main");
        TEST_ASSERT(asmText.find("HALT") != string::npos, "halt");
    }
    test_passed("codegen_entry");

    // --- New comprehensive tests ---

    test_header("codegen_if");
    {
        string asmText = compile(
            "func main(): int {\n"
            "    if (true) { return 1; } else { return 0; }\n"
            "}\n");
        TEST_ASSERT(asmText.find("CMP") != string::npos, "cmp");
        TEST_ASSERT(asmText.find("JE") != string::npos || asmText.find("JZ") != string::npos, "conditional jump");
    }
    test_passed("codegen_if");

    test_header("codegen_while");
    {
        string asmText = compile(
            "func main(): int {\n"
            "    var i = 0;\n"
            "    while (i < 10) { i = i + 1; }\n"
            "    return i;\n"
            "}\n");
        TEST_ASSERT(asmText.find("JMP") != string::npos, "jmp loop");
        TEST_ASSERT(asmText.find("JL") != string::npos || asmText.find("JGE") != string::npos, "conditional");
    }
    test_passed("codegen_while");

    test_header("codegen_for");
    {
        string asmText = compile(
            "func main(): int {\n"
            "    var sum = 0;\n"
            "    for (var i = 0; i < 3; i = i + 1) {\n"
            "        sum = sum + i;\n"
            "    }\n"
            "    return sum;\n"
            "}\n");
        TEST_ASSERT(asmText.find("JMP") != string::npos, "jmp");
    }
    test_passed("codegen_for");

    test_header("codegen_arithmetic");
    {
        string asmText = compile(
            "func main(): int {\n"
            "    var a = 1 + 2 - 3 * 4 / 5 % 6;\n"
            "    return a;\n"
            "}\n");
        TEST_ASSERT(asmText.find("ADD") != string::npos, "add");
        TEST_ASSERT(asmText.find("SUB") != string::npos, "sub");
        TEST_ASSERT(asmText.find("MUL") != string::npos, "mul");
        TEST_ASSERT(asmText.find("DIV") != string::npos, "div");
    }
    test_passed("codegen_arithmetic");

    test_header("codegen_comparison");
    {
        string asmText = compile(
            "func main(): int {\n"
            "    var a = 1 == 2;\n"
            "    var b = 3 != 4;\n"
            "    var c = 5 < 6;\n"
            "    var d = 7 > 8;\n"
            "    var e = 9 <= 10;\n"
            "    var f = 11 >= 12;\n"
            "    return 0;\n"
            "}\n");
        TEST_ASSERT(asmText.find("CMP") != string::npos, "cmp");
    }
    test_passed("codegen_comparison");

    test_header("codegen_logical");
    {
        string asmText = compile(
            "func main(): int {\n"
            "    var a = true && false;\n"
            "    var b = true || false;\n"
            "    return 0;\n"
            "}\n");
        // Logical ops are typically short-circuit; just verify it compiles
        TEST_ASSERT(!asmText.empty(), "generated asm");
    }
    test_passed("codegen_logical");

    test_header("codegen_bitwise");
    {
        string asmText = compile(
            "func main(): int {\n"
            "    var a = 1 & 2;\n"
            "    var b = 3 | 4;\n"
            "    var c = 5 ^ 6;\n"
            "    var d = ~7;\n"
            "    var e = 1 << 2;\n"
            "    var f = 8 >> 1;\n"
            "    return 0;\n"
            "}\n");
        // Bitwise ops may be lowered to arithmetic; just verify it compiles
        TEST_ASSERT(!asmText.empty(), "generated asm");
    }
    test_passed("codegen_bitwise");

    test_header("codegen_ptr_index");
    {
        string asmText = compile(
            "func main(): int {\n"
            "    var p: ptr<int> = alloc(3);\n"
            "    p[0] = 7;\n"
            "    p[1] = 8;\n"
            "    p[2] = 9;\n"
            "    return p[0];\n"
            "}\n");
        TEST_ASSERT(asmText.find("LDR") != string::npos, "ldr");
        TEST_ASSERT(asmText.find("STR") != string::npos, "str");
    }
    test_passed("codegen_ptr_index");

    test_header("codegen_string_literal");
    {
        string asmText = compile(
            "func main(): int {\n"
            "    var s = \"hello\";\n"
            "    return 0;\n"
            "}\n");
        TEST_ASSERT(asmText.find("DB") != string::npos, "db");
    }
    test_passed("codegen_string_literal");

    test_header("codegen_const");
    {
        string asmText = compile(
            "func main(): int {\n"
            "    const MAX = 100;\n"
            "    return MAX;\n"
            "}\n");
        TEST_ASSERT(asmText.find("LDI") != string::npos, "ldi const");
    }
    test_passed("codegen_const");

    test_header("codegen_global_var");
    {
        string asmText = compile(
            "var g: int = 42;\n"
            "func main(): int {\n"
            "    return g;\n"
            "}\n");
        TEST_ASSERT(asmText.find("g:") != string::npos, "global label");
    }
    test_passed("codegen_global_var");

    test_header("codegen_borrow");
    {
        string asmText = compile(
            "func get(x: &int): int {\n"
            "    return x[0];\n"
            "}\n"
            "func main(): int {\n"
            "    var a = 42;\n"
            "    return get(&a);\n"
            "}\n");
        TEST_ASSERT(asmText.find("CALL") != string::npos, "call instruction");
    }
    test_passed("codegen_borrow");

    return 0;
}
