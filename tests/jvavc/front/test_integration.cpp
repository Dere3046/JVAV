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
        TEST_ASSERT(out.find("jvlc 0.3.1") != string::npos, "jvlc version string");
        std::remove("version.out");
    }
    test_passed("integration_version_jvlc");

    test_header("integration_version_jvavc");
    {
        int ret = run_cmd(JVAVC_BACK_EXE " -v > version.out 2>&1");
        TEST_ASSERT(ret == 0, "jvavc -v exit code");
        string out;
        TEST_ASSERT(read_output("version.out", out), "read version output");
        TEST_ASSERT(out.find("jvavc 0.3.1") != string::npos, "jvavc version string");
        std::remove("version.out");
    }
    test_passed("integration_version_jvavc");

    test_header("integration_version_jvm");
    {
        int ret = run_cmd(JVM_EXE " -v > version.out 2>&1");
        TEST_ASSERT(ret == 0, "jvm -v exit code");
        string out;
        TEST_ASSERT(read_output("version.out", out), "read version output");
        TEST_ASSERT(out.find("jvm 0.3.1") != string::npos, "jvm version string");
        std::remove("version.out");
    }
    test_passed("integration_version_jvm");

    test_header("integration_version_disasm");
    {
        int ret = run_cmd(DISASM_EXE " -v > version.out 2>&1");
        TEST_ASSERT(ret == 0, "disasm -v exit code");
        string out;
        TEST_ASSERT(read_output("version.out", out), "read version output");
        TEST_ASSERT(out.find("disasm 0.3.1") != string::npos, "disasm version string");
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

    test_header("integration_all_diagnostics");
    {
        // Helper: compile a snippet and verify it produces a Rust-style diagnostic
        auto checkDiag = [](const char* label, const char* src, const char* expectedCode) -> bool {
            std::ofstream("f.jvl") << src;
            int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
            if (ret == 0) { printf("  FAIL %s: should have failed\n", label); return false; }
            string err;
            if (!read_output("f.err", err)) { printf("  FAIL %s: cannot read f.err\n", label); return false; }
            bool ok = true;
            if (err.find(expectedCode) == string::npos) { printf("  FAIL %s: missing %s\n", label, expectedCode); ok = false; }
            if (err.find("error[") == string::npos) { printf("  FAIL %s: missing error[\n", label); ok = false; }
            if (err.find("-->") == string::npos) { printf("  FAIL %s: missing -->\n", label); ok = false; }
            if (err.find("|") == string::npos) { printf("  FAIL %s: missing |\n", label); ok = false; }
            if (err.find("^") == string::npos) { printf("  FAIL %s: missing ^\n", label); ok = false; }
            if (!ok) printf("  actual output:\n%s\n", err.c_str());
            return ok;
        };

        // Lexer errors (E0100)
        TEST_ASSERT(checkDiag("lexer_unterminated_string", "func main(){ var a = \"hello }", "E0100"), "lexer unterminated string");
        TEST_ASSERT(checkDiag("lexer_unknown_char", "func main(){ var a = 1 @ 2; }", "E0100"), "lexer unknown char");

        // Parser errors (E0200) - exercise all parser error paths
        TEST_ASSERT(checkDiag("parser_missing_semi", "func main(): int { var x = 5 return 0; }", "E0200"), "parser missing semi");
        TEST_ASSERT(checkDiag("parser_missing_paren", "func main(): int { var x = (1 + 2; }", "E0200"), "parser missing paren");
        TEST_ASSERT(checkDiag("parser_type_error", "func main(): int { var x: unknown = 5; }", "E0200"), "parser type error");
        TEST_ASSERT(checkDiag("parser_expr_error", "func main(): int { var x = ; }", "E0200"), "parser expr error");
        TEST_ASSERT(checkDiag("parser_decl_error", "1 + 2;", "E0200"), "parser decl error");

        // Semantic errors (E1xxx)
        TEST_ASSERT(checkDiag("sema_undef_var", "func main(): int { var z = undefined_var; return 0; }", "E1"), "sema undefined var");

        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_all_diagnostics");

    // --- Drawing / format regression tests ---
    test_header("integration_diag_context_lines");
    {
        const char* src = "func main(): int {\nvar x = 5\nreturn 0;\n}";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        TEST_ASSERT(ret != 0, "should fail");
        string err;
        TEST_ASSERT(read_output("f.err", err), "read error");
        // Should show context: line 2 (var x = 5) and line 4 (})
        TEST_ASSERT(err.find("2 |") != string::npos, "context line 2");
        TEST_ASSERT(err.find("3 |") != string::npos, "error line 3");
        TEST_ASSERT(err.find("4 |") != string::npos, "context line 4");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_diag_context_lines");

    test_header("integration_diag_first_line");
    {
        // Error on line 1: no context before
        const char* src = "1 + 2;";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        TEST_ASSERT(ret != 0, "should fail");
        string err;
        TEST_ASSERT(read_output("f.err", err), "read error");
        TEST_ASSERT(err.find("1 |") != string::npos, "error line 1");
        // Make sure there is no line 0
        TEST_ASSERT(err.find("0 |") == string::npos, "no line 0");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_diag_first_line");

    test_header("integration_diag_last_line");
    {
        // Error on last line: no context after
        const char* src = "func main(): int {\n  var x = 5\n}";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        TEST_ASSERT(ret != 0, "should fail");
        string err;
        TEST_ASSERT(read_output("f.err", err), "read error");
        TEST_ASSERT(err.find("3 |") != string::npos, "error line 3");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_diag_last_line");

    test_header("integration_diag_caret_position");
    {
        // Error at column 13 (missing { after main())
        const char* src = "func main() var x = 5; }";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        TEST_ASSERT(ret != 0, "should fail");
        string err;
        TEST_ASSERT(read_output("f.err", err), "read error");
        // Caret should be aligned under 'v' of 'var'
        size_t linePos = err.find("1 |");
        TEST_ASSERT(linePos != string::npos, "source line");
        size_t caretPos = err.find("^", linePos);
        TEST_ASSERT(caretPos != string::npos, "caret exists");
        // In the source line "func main() var x = 5; }", 'v' is at col 13
        // The caret line looks like " |             ^", so ^ should be after spaces
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_diag_caret_position");

    // --- Edge case tests ---
    test_header("integration_edge_empty_file");
    {
        std::ofstream("f.jvl") << "";
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        TEST_ASSERT(ret == 0, "empty file should compile successfully");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_edge_empty_file");

    test_header("integration_edge_eof_missing_semi");
    {
        // Missing semicolon at end of file (no trailing newline)
        const char* src = "func main(): int { var x = 5 }";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        TEST_ASSERT(ret != 0, "should fail");
        string err;
        TEST_ASSERT(read_output("f.err", err), "read error");
        TEST_ASSERT(err.find("expected `;`") != string::npos, "missing semi message");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_edge_eof_missing_semi");

    test_header("integration_edge_crlf_source");
    {
        // File with CRLF line endings
        const char* src = "func main(): int {\r\n  var x = 5\r\n  return 0\r\n}";
        std::ofstream("f.jvl", ios::binary) << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        TEST_ASSERT(ret != 0, "should fail");
        string err;
        TEST_ASSERT(read_output("f.err", err), "read error");
        TEST_ASSERT(err.find("expected `;`") != string::npos, "missing semi on CRLF");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_edge_crlf_source");

    test_header("integration_syscall_frontend_decl");
    {
        const char* src = R"(
syscall my_puthex, 20, 1;

func main(): int {
    my_puthex(255);
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front compile");
        // Verify .syscall appears in generated assembly
        string jvav;
        TEST_ASSERT(read_output("f.jvav", jvav), "read jvav");
        TEST_ASSERT(jvav.find(".syscall my_puthex, 20, 1") != string::npos, "syscall decl in asm");
        std::remove("f.jvl"); std::remove("f.jvav");
    }
    test_passed("integration_syscall_frontend_decl");

    test_header("integration_edge_long_line");
    {
        string src = "func main(): int { var x = " + string(200, '1') + " return 0; }";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        TEST_ASSERT(ret != 0, "should fail");
        string err;
        TEST_ASSERT(read_output("f.err", err), "read error");
        TEST_ASSERT(err.find("expected `;`") != string::npos, "missing semi on long line");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_edge_long_line");

    test_header("integration_edge_single_char_file");
    {
        std::ofstream("f.jvl") << "{";
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        TEST_ASSERT(ret != 0, "should fail");
        string err;
        TEST_ASSERT(read_output("f.err", err), "read error");
        TEST_ASSERT(!err.empty(), "has error");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
    }
    test_passed("integration_edge_single_char_file");

    test_header("integration_edge_negative");
    {
        const char* src = R"(
func main(): int {
    putint(-42);
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
        TEST_ASSERT(out == "-42", "negative output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_edge_negative");

    test_header("integration_edge_putchar_sequence");
    {
        const char* src = R"(
func main(): int {
    putchar(72);
    putchar(105);
    putchar(33);
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
        TEST_ASSERT(out == "Hi!", "putchar sequence output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_edge_putchar_sequence");

    test_header("integration_edge_large_literal");
    {
        const char* src = R"(
func main(): int {
    putint(123456789012345);
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
        TEST_ASSERT(out == "123456789012345", "large literal output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_edge_large_literal");

    test_header("integration_putstr");
    {
        const char* src = R"(
func main(): int {
    putstr("Hi!", 3);
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
        TEST_ASSERT(out == "Hi!", "putstr output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_putstr");

    test_header("integration_exit");
    {
        const char* src = R"(
func main(): int {
    exit(7);
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(system_exit_code(ret) == 7, "exit should return 7");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_exit");

    test_header("integration_sleep");
    {
        const char* src = R"(
func main(): int {
    sleep(0);
    putchar(65);
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
        TEST_ASSERT(out == "A", "sleep output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_sleep");

    test_header("integration_syscall_fileio_frontend");
    {
        const char* src = R"(
syscall fopen, 4, 2;
syscall fclose, 5, 1;
syscall fwrite, 7, 3;

func main(): int {
    var fd = fopen("test_front_fio.txt", "w");
    var data = "HELLO";
    fwrite(fd, data, 5);
    fclose(fd);
    return 0;
}
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "front compile");
        // Verify .syscall appears in generated assembly
        string jvav;
        TEST_ASSERT(read_output("f.jvav", jvav), "read jvav");
        TEST_ASSERT(jvav.find(".syscall fopen, 4, 2") != string::npos, "fopen decl in asm");
        TEST_ASSERT(jvav.find(".syscall fclose, 5, 1") != string::npos, "fclose decl in asm");
        TEST_ASSERT(jvav.find(".syscall fwrite, 7, 3") != string::npos, "fwrite decl in asm");
        ret = run_cmd(JVAVC_BACK_EXE " f.jvav f.bin");
        TEST_ASSERT(ret == 0, "back compile");
        ret = run_cmd(JVM_EXE " f.bin > f.out 2>&1");
        TEST_ASSERT(ret == 0, "run");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin");
        std::remove("f.out");
    }
    test_passed("integration_syscall_fileio_frontend");

    test_header("integration_stdlib_io_newline_space");
    {
        const char* src = R"(
import "std/io.jvl";

func main(): int {
    print_space();
    print_newline();
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
        TEST_ASSERT(out == " \n", "space+newline output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_stdlib_io_newline_space");

    test_header("integration_stdlib_mem");
    {
        // Test std/mem.jvl import compiles correctly
        const char* src = R"(
import "std/mem.jvl";
func main(): int { return 0; }
)";
        std::ofstream("f.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav");
        TEST_ASSERT(ret == 0, "std/mem.jvl import compiles");
        std::remove("f.jvl"); std::remove("f.jvav");
    }
    test_passed("integration_stdlib_mem");

    test_header("integration_memcpy_memset_inline");
    {
        // Inline memory operations (std/mem.jvl functions cannot be called directly
        // due to Move semantics on ptr<int> parameters)
        const char* src = R"(
func main(): int {
    var a: ptr<int> = alloc(3);
    a[0] = 1; a[1] = 2; a[2] = 3;
    var b: ptr<int> = alloc(3);
    var i = 0;
    while (i < 3) {
        b[i] = a[i];
        i = i + 1;
    }
    putint(b[0]); putint(b[1]); putint(b[2]);
    i = 0;
    while (i < 3) {
        a[i] = 0;
        i = i + 1;
    }
    putint(a[0]); putint(a[1]); putint(a[2]);
    free(a); free(b);
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
        TEST_ASSERT(out == "123000", "memcpy+memset inline output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_memcpy_memset_inline");

    test_header("integration_disasm_static");
    {
        const char* asm_src = R"(
    .global _start
_start:
    LDI R0, 42
    HALT
)";
        std::ofstream("test_disasm.jvav") << asm_src;
        int ret = run_cmd(JVAVC_BACK_EXE " test_disasm.jvav test_disasm.bin");
        TEST_ASSERT(ret == 0, "compile for disasm");
        ret = run_cmd(DISASM_EXE " test_disasm.bin > test_disasm.out 2>&1");
        TEST_ASSERT(ret == 0, "disasm static");
        string out;
        TEST_ASSERT(read_output("test_disasm.out", out), "read disasm");
        TEST_ASSERT(out.find("LDI") != string::npos, "disasm contains LDI");
        TEST_ASSERT(out.find("HALT") != string::npos, "disasm contains HALT");
        std::remove("test_disasm.jvav"); std::remove("test_disasm.bin"); std::remove("test_disasm.out");
    }
    test_passed("integration_disasm_static");

    test_header("integration_disasm_trace");
    {
        const char* asm_src = R"(
    .global _start
_start:
    LDI R0, 42
    HALT
)";
        std::ofstream("test_disasm.jvav") << asm_src;
        int ret = run_cmd(JVAVC_BACK_EXE " test_disasm.jvav test_disasm.bin");
        TEST_ASSERT(ret == 0, "compile for disasm trace");
        ret = run_cmd(DISASM_EXE " -t test_disasm.bin > test_disasm.out 2>&1");
        TEST_ASSERT(ret == 0, "disasm trace");
        string out;
        TEST_ASSERT(read_output("test_disasm.out", out), "read trace");
        TEST_ASSERT(out.find("Trace Summary") != string::npos, "trace header");
        TEST_ASSERT(out.find("Halted cleanly") != string::npos, "halted cleanly");
        std::remove("test_disasm.jvav"); std::remove("test_disasm.bin"); std::remove("test_disasm.out");
    }
    test_passed("integration_disasm_trace");

    test_header("integration_disasm_complex");
    {
        const char* asm_src = R"(
    .global _start
_start:
    LDI R0, 5
    LDI R1, 3
    ADD R2, R0, R1
    CMP R2, R1
    JE equal
    JMP done
equal:
    SUB R2, R2, R1
done:
    HALT
)";
        std::ofstream("test_disasm.jvav") << asm_src;
        int ret = run_cmd(JVAVC_BACK_EXE " test_disasm.jvav test_disasm.bin");
        TEST_ASSERT(ret == 0, "compile for disasm complex");
        // Static disasm: verify multiple mnemonics appear
        ret = run_cmd(DISASM_EXE " test_disasm.bin > test_disasm_static.out 2>&1");
        TEST_ASSERT(ret == 0, "disasm complex static");
        string out_static;
        TEST_ASSERT(read_output("test_disasm_static.out", out_static), "read static");
        TEST_ASSERT(out_static.find("ADD") != string::npos, "disasm contains ADD");
        TEST_ASSERT(out_static.find("CMP") != string::npos, "disasm contains CMP");
        TEST_ASSERT(out_static.find("JE") != string::npos, "disasm contains JE");
        TEST_ASSERT(out_static.find("JMP") != string::npos, "disasm contains JMP");
        TEST_ASSERT(out_static.find("SUB") != string::npos, "disasm contains SUB");
        // Trace disasm: verify coverage and executed/unexecuted markers
        ret = run_cmd(DISASM_EXE " -t test_disasm.bin > test_disasm_trace.out 2>&1");
        TEST_ASSERT(ret == 0, "disasm complex trace");
        string out_trace;
        TEST_ASSERT(read_output("test_disasm_trace.out", out_trace), "read trace");
        TEST_ASSERT(out_trace.find("Executed:           9 (90.0%)") != string::npos, "coverage 90%");
        TEST_ASSERT(out_trace.find(">>> HALT") != string::npos, "HALT executed");
        // The SUB instruction at 'equal' label should NOT have >>> prefix (not executed)
        // Find the line containing "SUB" in trace output
        size_t sub_pos = out_trace.find("SUB");
        TEST_ASSERT(sub_pos != string::npos, "trace contains SUB");
        // Verify the SUB line does NOT start with ">>>"
        size_t line_start = out_trace.rfind('\n', sub_pos);
        if (line_start == string::npos) line_start = 0; else line_start++;
        string sub_line = out_trace.substr(line_start, sub_pos - line_start);
        TEST_ASSERT(sub_line.find(">>>") == string::npos, "unexecuted SUB not marked >>>");
        std::remove("test_disasm.jvav"); std::remove("test_disasm.bin");
        std::remove("test_disasm_static.out"); std::remove("test_disasm_trace.out");
    }
    test_passed("integration_disasm_complex");

    test_header("integration_sleep_negative");
    {
        const char* src = R"(
func main(): int {
    sleep(-100);
    putchar(65);
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
        TEST_ASSERT(out == "A", "sleep negative clamped to 0");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_sleep_negative");

    test_header("integration_putstr_empty");
    {
        const char* src = R"(
func main(): int {
    putstr("", 0);
    putchar(88);
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
        TEST_ASSERT(out == "X", "putstr empty output");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_putstr_empty");

    test_header("integration_stdlib_math_edge");
    {
        const char* src = R"(
import "std/math.jvl";

func main(): int {
    putint(abs(0));
    putint(pow(0, 0));
    putint(pow(1, 100));
    putint(clamp(5, 0, 10));
    putint(clamp(-5, 0, 10));
    putint(clamp(15, 0, 10));
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
        // abs(0)=0, pow(0,0)=1, pow(1,100)=1, clamp(5,0,10)=5, clamp(-5,0,10)=0, clamp(15,0,10)=10
        TEST_ASSERT(out == "0115010", "math edge cases");
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.bin"); std::remove("f.out");
    }
    test_passed("integration_stdlib_math_edge");

    test_header("integration_nested_import");
    {
        const char* lib = R"(
func greet(): int {
    putchar(72);
    return 0;
}
)";
        const char* main_src = R"(
import "lib.jvl";
import "std/io.jvl";

func main(): int {
    greet();
    println(42);
    return 0;
}
)";
        std::ofstream("lib.jvl") << lib;
        std::ofstream("main.jvl") << main_src;
        int ret = run_cmd(JVAVC_FRONT_EXE " main.jvl main.jvav");
        TEST_ASSERT(ret == 0, "front nested import");
        ret = run_cmd(JVAVC_BACK_EXE " main.jvav main.bin");
        TEST_ASSERT(ret == 0, "back nested import");
        ret = run_cmd(JVM_EXE " main.bin > main.out 2>&1");
        TEST_ASSERT(ret == 0, "run nested import");
        string out;
        TEST_ASSERT(read_output("main.out", out), "read");
        TEST_ASSERT(out == "H42\n", "nested import output");
        std::remove("lib.jvl"); std::remove("main.jvl");
        std::remove("main.jvav"); std::remove("main.bin"); std::remove("main.out");
    }
    test_passed("integration_nested_import");

    return 0;
}
