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

    return 0;
}
