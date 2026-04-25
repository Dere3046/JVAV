#include "test_utils.hpp"
#include <cstdlib>
#include <fstream>
#include <cstdio>

using namespace std;

static int run_cmd(const char *cmd) {
    return system(cmd);
}

static bool read_output(const string &filename, string &out) {
    ifstream f(filename);
    if (!f) return false;
    out = string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    return true;
}

int test_integration_main() {
    // --- Original tests ---
    test_header("integration_putint");
    {
        const char* src = R"(
func main(): int {
    putint(42);
    return 0;
}
)";
        std::ofstream("test_putint.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " test_putint.jvl test_putint.jvav");
        TEST_ASSERT(ret == 0, "front compile failed");
        ret = run_cmd(JVAVC_BACK_EXE " test_putint.jvav test_putint.bin");
        TEST_ASSERT(ret == 0, "back compile failed");
        ret = run_cmd(JVM_EXE " test_putint.bin > test_putint.out 2>&1");
        TEST_ASSERT(ret == 0, "execution failed");
        std::ifstream out("test_putint.out");
        std::string output((std::istreambuf_iterator<char>(out)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output == "42", "output");
        std::remove("test_putint.jvl");
        std::remove("test_putint.jvav");
        std::remove("test_putint.bin");
        std::remove("test_putint.out");
    }
    test_passed("integration_putint");

    test_header("integration_import");
    {
        const char* lib = R"(
func add(a: int, b: int): int {
    return a + b;
}
)";
        const char* main = R"(
import "test_imp_lib.jvl";

func main(): int {
    var z: int = add(3, 5);
    putint(z);
    return 0;
}
)";
        std::ofstream("test_imp_lib.jvl") << lib;
        std::ofstream("test_imp_main.jvl") << main;
        int ret = run_cmd(JVAVC_FRONT_EXE " test_imp_main.jvl test_imp_main.jvav");
        TEST_ASSERT(ret == 0, "front compile failed");
        ret = run_cmd(JVAVC_BACK_EXE " test_imp_main.jvav test_imp_main.bin");
        TEST_ASSERT(ret == 0, "back compile failed");
        ret = run_cmd(JVM_EXE " test_imp_main.bin > test_imp_main.out 2>&1");
        TEST_ASSERT(ret == 0, "execution failed");
        std::ifstream out("test_imp_main.out");
        std::string output((std::istreambuf_iterator<char>(out)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output == "8", "output");
        std::remove("test_imp_lib.jvl");
        std::remove("test_imp_main.jvl");
        std::remove("test_imp_main.jvav");
        std::remove("test_imp_main.bin");
        std::remove("test_imp_main.out");
    }
    test_passed("integration_import");

    test_header("integration_alloc");
    {
        const char* src = R"(
func main(): int {
    var p: ptr<int> = alloc(1);
    p[0] = 42;
    putint(p[0]);
    free(p);
    return 0;
}
)";
        std::ofstream("test_alloc.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " test_alloc.jvl test_alloc.jvav");
        TEST_ASSERT(ret == 0, "front compile failed");
        ret = run_cmd(JVAVC_BACK_EXE " test_alloc.jvav test_alloc.bin");
        TEST_ASSERT(ret == 0, "back compile failed");
        ret = run_cmd(JVM_EXE " test_alloc.bin > test_alloc.out 2>&1");
        TEST_ASSERT(ret == 0, "execution failed");
        std::ifstream out("test_alloc.out");
        std::string output((std::istreambuf_iterator<char>(out)), std::istreambuf_iterator<char>());
        TEST_ASSERT(output == "42", "alloc output");
        std::remove("test_alloc.jvl");
        std::remove("test_alloc.jvav");
        std::remove("test_alloc.bin");
        std::remove("test_alloc.out");
    }
    test_passed("integration_alloc");

    // --- New comprehensive tests ---

    test_header("integration_arithmetic");
    {
        const char* src = R"(
func main(): int {
    putint(1 + 2);
    putint(5 - 3);
    putint(2 * 3);
    putint(8 / 2);
    putint(7 % 3);
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "32641", "arithmetic: 1+2=3, 5-3=2, 2*3=6, 8/2=4, 7%3=1");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_arithmetic");

    test_header("integration_if_else");
    {
        const char* src = R"(
func main(): int {
    var x = 5;
    if (x > 3) {
        putint(1);
    } else {
        putint(0);
    }
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "1", "if true");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_if_else");

    test_header("integration_while");
    {
        const char* src = R"(
func main(): int {
    var i = 0;
    while (i < 5) {
        putint(i);
        i = i + 1;
    }
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "01234", "while loop");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_while");

    test_header("integration_for");
    {
        const char* src = R"(
func main(): int {
    var sum = 0;
    for (var i = 0; i < 5; i = i + 1) {
        sum = sum + i;
    }
    putint(sum);
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "10", "for loop sum=0+1+2+3+4");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_for");

    test_header("integration_function_call");
    {
        const char* src = R"(
func add(a: int, b: int): int {
    return a + b;
}

func main(): int {
    putint(add(10, 20));
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "30", "func call");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_function_call");

    test_header("integration_ptr_heap");
    {
        const char* src = R"(
func main(): int {
    var p: ptr<int> = alloc(3);
    p[0] = 7;
    p[1] = 8;
    p[2] = 9;
    putint(p[0] + p[1] + p[2]);
    free(p);
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "24", "ptr heap");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_ptr_heap");

    test_header("integration_char");
    {
        const char* src = R"(
func main(): int {
    putchar(65);
    putchar(66);
    putchar(67);
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "ABC", "char output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_char");

    test_header("integration_bool");
    {
        const char* src = R"(
func main(): int {
    var t = true;
    var f = false;
    if (t) { putint(1); }
    if (f) { putint(0); }
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "1", "bool true only");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_bool");

    test_header("integration_comparison");
    {
        const char* src = R"(
func main(): int {
    if (1 == 1) { putint(1); }
    if (1 != 2) { putint(2); }
    if (1 < 2)  { putint(3); }
    if (2 > 1)  { putint(4); }
    if (1 <= 1) { putint(5); }
    if (1 >= 1) { putint(6); }
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "123456", "comparisons");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_comparison");

    test_header("integration_nested_function");
    {
        const char* src = R"(
func factorial(n: int): int {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

func main(): int {
    putint(factorial(5));
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "120", "factorial 5");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_nested_function");

    test_header("integration_global_var");
    {
        const char* src = R"(
var g: int = 99;

func main(): int {
    putint(g);
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "99", "global var");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_global_var");

    test_header("integration_crlf_source");
    {
        // Test that CRLF source files compile correctly
        const char* src = "func main(): int {\r\n    putint(123);\r\n    return 0;\r\n}\r\n";
        ofstream("f.jvl", ios::binary) << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front crlf");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back crlf");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run crlf");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "123", "crlf output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_crlf_source");

    // --- Standard library tests ---

    test_header("integration_stdlib_io");
    {
        const char* src = R"(
import "std/io.jvl";

func main(): int {
    println(42);
    println(7);
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "42\n7\n", "println output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_stdlib_io");

    test_header("integration_stdlib_math");
    {
        const char* src = R"(
import "std/math.jvl";

func main(): int {
    putint(abs(-5));
    putint(max(3, 7));
    putint(min(3, 7));
    putint(clamp(10, 0, 5));
    putint(pow(2, 3));
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "57358", "math output: abs=5 max=7 min=3 clamp=5 pow=8");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_stdlib_math");

    test_header("integration_stdlib_string");
    {
        const char* src = R"(
import "std/string.jvl";

func main(): int {
    var s = "hi";
    str_putn(s, 2);
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        TEST_ASSERT(out == "hi", "str_putn output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_stdlib_string");

    test_header("integration_stdlib_not_found");
    {
        const char* src = R"(
import "std/nonexistent.jvl";
func main(): int { return 0; }
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        TEST_ASSERT(ret != 0, "should fail on missing stdlib");
        string err;
        TEST_ASSERT(read_output("f.err", err), "read error output");
        TEST_ASSERT(err.find("cannot find standard library") != string::npos, "missing stdlib error message");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_stdlib_not_found");

    test_header("integration_version_jvlc");
    {
        int ret = run_cmd(JVAVC_FRONT_EXE " -v > version.out 2>&1");
        TEST_ASSERT(ret == 0, "jvlc -v exit code");
        string out;
        TEST_ASSERT(read_output("version.out", out), "read version output");
        TEST_ASSERT(out.find("jvlc 0.2.5") != string::npos, "jvlc version string");
        std::remove("version.out");
    }
    test_passed("integration_version_jvlc");

    test_header("integration_version_jvavc");
    {
        int ret = run_cmd(JVAVC_BACK_EXE " -v > version.out 2>&1");
        TEST_ASSERT(ret == 0, "jvavc -v exit code");
        string out;
        TEST_ASSERT(read_output("version.out", out), "read version output");
        TEST_ASSERT(out.find("jvavc 0.2.5") != string::npos, "jvavc version string");
        std::remove("version.out");
    }
    test_passed("integration_version_jvavc");

    test_header("integration_version_jvm");
    {
        int ret = run_cmd(JVM_EXE " -v > version.out 2>&1");
        TEST_ASSERT(ret == 0, "jvm -v exit code");
        string out;
        TEST_ASSERT(read_output("version.out", out), "read version output");
        TEST_ASSERT(out.find("jvm 0.2.5") != string::npos, "jvm version string");
        std::remove("version.out");
    }
    test_passed("integration_version_jvm");

    test_header("integration_version_disasm");
    {
        int ret = run_cmd(DISASM_EXE " -v > version.out 2>&1");
        TEST_ASSERT(ret == 0, "disasm -v exit code");
        string out;
        TEST_ASSERT(read_output("version.out", out), "read version output");
        TEST_ASSERT(out.find("disasm 0.2.5") != string::npos, "disasm version string");
        std::remove("version.out");
    }
    test_passed("integration_version_disasm");

    test_header("integration_bitwise");
    {
        const char* src = R"(
func main(): int {
    putint(0x0F & 0xF0);
    putint(0x0F | 0xF0);
    putint(0x0F ^ 0xFF);
    putint(1 << 3);
    putint(16 >> 2);
    putint(~0);
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("f.out", out), "read");
        // putint prints without separators; 0|255|240|8|4|-1 -> "025524084-1"
        TEST_ASSERT(out == "025524084-1", "bitwise output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_bitwise");

    test_header("integration_diag_format");
    {
        const char* src = R"(
func main(): int {
    var z = undefined_var;
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        TEST_ASSERT(ret != 0, "should fail on undefined variable");
        string err;
        TEST_ASSERT(read_output("f.err", err), "read error output");
        // Rust-style diagnostics: error[...], location arrow, source line, underline caret
        TEST_ASSERT(err.find("error[") != string::npos, "error code tag");
        TEST_ASSERT(err.find("-->") != string::npos, "location arrow");
        TEST_ASSERT(err.find("|") != string::npos, "source gutter");
        TEST_ASSERT(err.find("^") != string::npos, "underline caret");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_diag_format");

    return 0;
}
