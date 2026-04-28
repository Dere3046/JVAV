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

struct CodegenCase {
    const char* name;
    const char* src;
};

static CodegenCase codegenCases[] = {
    {"codegen_prologue",
     "func main(): int { return 0; }"},

    {"codegen_locals",
     "func main(): int { var x: int = 5; return x; }"},

    {"codegen_call",
     "func add(a: int, b: int): int { return a + b; }\n"
     "func main(): int { return add(3, 5); }"},

    {"codegen_entry",
     "func main(): int { return 0; }"},

    {"codegen_if",
     "func main(): int {\n"
     "    if (true) { return 1; } else { return 0; }\n"
     "}\n"},

    {"codegen_while",
     "func main(): int {\n"
     "    var i = 0;\n"
     "    while (i < 10) { i = i + 1; }\n"
     "    return i;\n"
     "}\n"},

    {"codegen_for",
     "func main(): int {\n"
     "    var sum = 0;\n"
     "    for (var i = 0; i < 3; i = i + 1) {\n"
     "        sum = sum + i;\n"
     "    }\n"
     "    return sum;\n"
     "}\n"},

    {"codegen_arithmetic",
     "func main(): int {\n"
     "    var a = 1 + 2 - 3 * 4 / 5 % 6;\n"
     "    return a;\n"
     "}\n"},

    {"codegen_comparison",
     "func main(): int {\n"
     "    var a = 1 == 2;\n"
     "    var b = 3 != 4;\n"
     "    var c = 5 < 6;\n"
     "    var d = 7 > 8;\n"
     "    var e = 9 <= 10;\n"
     "    var f = 11 >= 12;\n"
     "    return 0;\n"
     "}\n"},

    {"codegen_logical",
     "func main(): int {\n"
     "    var a = true && false;\n"
     "    var b = true || false;\n"
     "    return 0;\n"
     "}\n"},

    {"codegen_bitwise",
     "func main(): int {\n"
     "    var a = 1 & 2;\n"
     "    var b = 3 | 4;\n"
     "    var c = 5 ^ 6;\n"
     "    var d = ~7;\n"
     "    var e = 1 << 2;\n"
     "    var f = 8 >> 1;\n"
     "    return 0;\n"
     "}\n"},

    {"codegen_ptr_index",
     "func main(): int {\n"
     "    var p: ptr<int> = alloc(3);\n"
     "    p[0] = 7;\n"
     "    p[1] = 8;\n"
     "    p[2] = 9;\n"
     "    return p[0];\n"
     "}\n"},

    {"codegen_string_literal",
     "func main(): int {\n"
     "    var s = \"hello\";\n"
     "    return 0;\n"
     "}\n"},

    {"codegen_const",
     "func main(): int {\n"
     "    const MAX = 100;\n"
     "    return MAX;\n"
     "}\n"},

    {"codegen_global_var",
     "var g: int = 42;\n"
     "func main(): int {\n"
     "    return g;\n"
     "}\n"},

    {"codegen_borrow",
     "func get(x: &int): int {\n"
     "    return x[0];\n"
     "}\n"
     "func main(): int {\n"
     "    var a = 42;\n"
     "    return get(&a);\n"
     "}\n"},
};

int test_codegen_main() {
    for (const auto& c : codegenCases) {
        test_header(c.name);
        string asmText = compile(c.src);
        TEST_ASSERT_SNAPSHOT_EQ(asmText, c.name);
        test_passed(c.name);
    }
    return 0;
}
