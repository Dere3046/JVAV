#include "test_utils.hpp"
#include <cstdlib>
#include <fstream>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std;

static int run_cmd(const char *cmd) {
    return system_exit_code(system(cmd));
}

static bool read_output(const string &filename, string &out) {
    ifstream f(filename);
    if (!f) return false;
    out = string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    return true;
}

static bool run_file_cases(const string& caseDir) {
    if (!fs::exists(caseDir)) return true;
    for (const auto& entry : fs::directory_iterator(caseDir)) {
        if (entry.is_directory()) {
            string subDir = entry.path().string();
            string subName = entry.path().filename().string();
            for (const auto& subEntry : fs::directory_iterator(subDir)) {
                if (!subEntry.is_regular_file()) continue;
                string path = subEntry.path().string();
                if (path.size() < 4 || path.substr(path.size() - 4) != ".jvl") continue;
                string name = subEntry.path().stem().string();
                if (name != "main") continue;
                string expectedPath = subDir + "/expected";
                string outFile = subName + ".out";
                string errFile = subName + ".err";
                string binFile = subName + ".bin";
                string jvavFile = subName + ".jvav";
                string compErrFile = subName + "_comp.err";

                test_header(("integration_case_" + subName).c_str());
                int ret = run_cmd((string(JVAVC_FRONT_EXE) + " " + path + " " + jvavFile + " > " + compErrFile + " 2>&1").c_str());
                if (ret != 0) {
                    if (fs::exists(expectedPath)) {
                        string out, expected;
                        TEST_ASSERT(read_file(compErrFile, out), "read compiler output");
                        TEST_ASSERT(read_file(expectedPath, expected), "read expected");
                        TEST_ASSERT(out == expected, "error output mismatch");
                        remove(compErrFile.c_str());
                        test_passed(("integration_case_" + subName).c_str());
                        continue;
                    }
                    TEST_ASSERT(ret == 0, "front compile failed");
                }
                remove(compErrFile.c_str());
                ret = run_cmd((string(JVAVC_BACK_EXE) + " " + jvavFile + " " + binFile).c_str());
                TEST_ASSERT(ret == 0, "back compile failed");
                ret = run_cmd((string(JVM_EXE) + " " + binFile + " > " + outFile + " 2> " + errFile).c_str());
                TEST_ASSERT(ret == 0, "execution failed");

                string out, err;
                TEST_ASSERT(read_file(outFile, out), "read stdout");
                TEST_ASSERT(read_file(errFile, err), "read stderr");
                TEST_ASSERT(err.empty(), "stderr should be empty");

                if (fs::exists(expectedPath)) {
                    string expected;
                    TEST_ASSERT(read_file(expectedPath, expected), "read expected");
                    TEST_ASSERT(out == expected, "output mismatch");
                }

                remove(jvavFile.c_str());
                remove(binFile.c_str());
                remove(outFile.c_str());
                remove(errFile.c_str());
                test_passed(("integration_case_" + subName).c_str());
            }
            continue;
        }
        if (!entry.is_regular_file()) continue;
        string path = entry.path().string();
        if (path.size() < 4 || path.substr(path.size() - 4) != ".jvl") continue;

        string name = entry.path().stem().string();
        string base = entry.path().parent_path().string();
        string expectedPath = base + "/" + name + ".expected";
        string outFile = name + ".out";
        string errFile = name + ".err";
        string binFile = name + ".bin";
        string jvavFile = name + ".jvav";
        string compErrFile = name + "_comp.err";

        test_header(("integration_case_" + name).c_str());
        int ret = run_cmd((string(JVAVC_FRONT_EXE) + " " + path + " " + jvavFile + " > " + compErrFile + " 2>&1").c_str());
        if (ret != 0) {
            if (fs::exists(expectedPath)) {
                string out, expected;
                TEST_ASSERT(read_file(compErrFile, out), "read compiler output");
                TEST_ASSERT(read_file(expectedPath, expected), "read expected");
                TEST_ASSERT(out == expected, "error output mismatch");
                remove(compErrFile.c_str());
                remove(jvavFile.c_str());
                test_passed(("integration_case_" + name).c_str());
                continue;
            }
            TEST_ASSERT(ret == 0, "front compile failed");
        }
        remove(compErrFile.c_str());
        ret = run_cmd((string(JVAVC_BACK_EXE) + " " + jvavFile + " " + binFile).c_str());
        TEST_ASSERT(ret == 0, "back compile failed");
        ret = run_cmd((string(JVM_EXE) + " " + binFile + " > " + outFile + " 2> " + errFile).c_str());
        TEST_ASSERT(ret == 0, "execution failed");

        string out, err;
        TEST_ASSERT(read_file(outFile, out), "read stdout");
        TEST_ASSERT(read_file(errFile, err), "read stderr");
        TEST_ASSERT(err.empty(), "stderr should be empty");

        // Normalize CRLF to LF for cross-platform comparison
        size_t pos = 0;
        while ((pos = out.find("\r\n", pos)) != string::npos) {
            out.replace(pos, 2, "\n");
            pos++;
        }

        if (fs::exists(expectedPath)) {
            string expected;
            TEST_ASSERT(read_file(expectedPath, expected), "read expected");
            TEST_ASSERT(out == expected, "output mismatch");
        }

        remove(jvavFile.c_str());
        remove(binFile.c_str());
        remove(outFile.c_str());
        remove(errFile.c_str());
        test_passed(("integration_case_" + name).c_str());
    }
    return true;
}

int test_integration_main() {
    run_file_cases("tests/cases/front");
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
        TEST_ASSERT(out.find("jvlc 0.4.0") != string::npos, "jvlc version string");
        std::remove("version.out");
    }
    test_passed("integration_version_jvlc");

    test_header("integration_version_jvavc");
    {
        int ret = run_cmd(JVAVC_BACK_EXE " -v > version.out 2>&1");
        TEST_ASSERT(ret == 0, "jvavc -v exit code");
        string out;
        TEST_ASSERT(read_output("version.out", out), "read version output");
        TEST_ASSERT(out.find("jvavc 0.4.0") != string::npos, "jvavc version string");
        std::remove("version.out");
    }
    test_passed("integration_version_jvavc");

    test_header("integration_version_jvm");
    {
        int ret = run_cmd(JVM_EXE " -v > version.out 2>&1");
        TEST_ASSERT(ret == 0, "jvm -v exit code");
        string out;
        TEST_ASSERT(read_output("version.out", out), "read version output");
        TEST_ASSERT(out.find("jvm 0.4.0") != string::npos, "jvm version string");
        std::remove("version.out");
    }
    test_passed("integration_version_jvm");

    test_header("integration_version_disasm");
    {
        int ret = run_cmd(DISASM_EXE " -v > version.out 2>&1");
        TEST_ASSERT(ret == 0, "disasm -v exit code");
        string out;
        TEST_ASSERT(read_output("version.out", out), "read version output");
        TEST_ASSERT(out.find("disasm 0.4.0") != string::npos, "disasm version string");
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

    // --- Data-driven diagnostic tests ---
    struct DiagCase {
        const char* name;
        const char* src;
        const char* expectedBody;  // exact message body (without dynamic error code)
        const char* expectedHelp;  // optional exact help text
        bool useSnapshot;          // if true, match entire diagnostic output via snapshot
    };

    auto runDiagCase = [](const DiagCase& c) -> bool {
        test_header(c.name);
        std::ofstream("f.jvl") << c.src;
        int ret = run_cmd(JVAVC_FRONT_EXE " f.jvl f.jvav > f.err 2>&1");
        if (ret == 0) {
            printf("  FAIL %s: should have failed\n", c.name);
            std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
            test_failed(c.name, "expected compilation failure");
            return false;
        }
        string err;
        if (!read_output("f.err", err)) {
            printf("  FAIL %s: cannot read f.err\n", c.name);
            std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
            test_failed(c.name, "cannot read error output");
            return false;
        }

        if (c.useSnapshot) {
            bool ok = check_snapshot(err, c.name);
            std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
            if (!ok) { test_failed(c.name, "snapshot mismatch"); return false; }
            test_passed(c.name);
            return true;
        }

        bool ok = true;
        // Verify Rust-style format markers
        if (err.find("error[") == string::npos) { printf("  FAIL %s: missing error[\n", c.name); ok = false; }
        if (err.find("-->") == string::npos) { printf("  FAIL %s: missing -->\n", c.name); ok = false; }
        if (err.find("|") == string::npos) { printf("  FAIL %s: missing |\n", c.name); ok = false; }
        if (err.find("^") == string::npos) { printf("  FAIL %s: missing ^\n", c.name); ok = false; }

        // Exact body match (without dynamic error code)
        if (c.expectedBody && err.find(c.expectedBody) == string::npos) {
            printf("  FAIL %s: missing body '%s'\n", c.name, c.expectedBody);
            ok = false;
        }
        // Exact help match
        if (c.expectedHelp && err.find(c.expectedHelp) == string::npos) {
            printf("  FAIL %s: missing help '%s'\n", c.name, c.expectedHelp);
            ok = false;
        }

        if (!ok) printf("  actual output:\n%s\n", err.c_str());
        std::remove("f.jvl"); std::remove("f.jvav"); std::remove("f.err");
        if (!ok) { test_failed(c.name, "diagnostic mismatch"); return false; }
        test_passed(c.name);
        return true;
    };

    static DiagCase diagCases[] = {
        // Lexer errors
        {"diag_lexer_unterminated_string", "func main(){ var a = \"hello }", "unterminated string literal", nullptr, false},
        {"diag_lexer_unknown_char", "func main(){ var a = 1 @ 2; }", "unknown character", nullptr, false},

        // Parser errors
        {"diag_parser_missing_semi", "func main(): int { var x = 5 return 0; }", "expected `;`", nullptr, false},
        {"diag_parser_missing_paren", "func main(): int { var x = (1 + 2; }", "expected `)`", nullptr, false},
        {"diag_parser_type_error", "func main(): int { var x: unknown = 5; }", "unknown type `unknown`", nullptr, false},
        {"diag_parser_expr_error", "func main(): int { var x = ; }", "unexpected token", nullptr, false},
        {"diag_parser_decl_error", "1 + 2;", "expected a declaration", nullptr, false},

        // Semantic errors
        {"diag_sema_undef_var", "func main(): int { var z = undefined_var; return 0; }", "cannot find value `undefined_var` in this scope", "declare 'undefined_var' before use", false},
        {"diag_sema_use_after_move", "func main(): int { var p: ptr<int> = alloc(1); var q = p; p[0]=1; return 0; }", "use of moved value `p`", "reassign 'p' or use a borrow (&p) instead", false},
        {"diag_sema_borrow_after_move", "func main(): int { var p: ptr<int> = alloc(1); var q = p; var r = &p; return 0; }", "cannot borrow moved value `p`", "reassign 'p' before borrowing", false},

        // Format regression tests (snapshots)
        {"diag_format_undef_var", "func main(): int {\n    var z = undefined_var;\n    return 0;\n}", nullptr, nullptr, true},
        {"diag_context_lines", "func main(): int {\nvar x = 5\nreturn 0;\n}", nullptr, nullptr, true},
        {"diag_first_line", "1 + 2;", nullptr, nullptr, true},
        {"diag_last_line", "func main(): int {\n  var x = 5\n}", nullptr, nullptr, true},
        {"diag_caret_position", "func main(): int var x = 5; }", nullptr, nullptr, true},
    };

    for (const auto& c : diagCases) {
        runDiagCase(c);
    }

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
        TEST_ASSERT(ret == 7, "exit should return 7");
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

    test_header("integration_syscall_return_int");
    {
        const char* src = R"(
syscall myopen, 4, 2;
syscall myclose, 5, 1;

func main(): int {
    var path = "test_return.txt";
    var mode = "wb";
    var fd: int = myopen(path, mode);
    putint(fd);
    if (fd > 0) {
        myclose(fd);
    }
    return 0;
}
)";
        std::ofstream("test_sc_ret.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " test_sc_ret.jvl test_sc_ret.jvav");
        TEST_ASSERT(ret == 0, "front compile failed");
        ret = run_cmd(JVAVC_BACK_EXE " test_sc_ret.jvav test_sc_ret.bin");
        TEST_ASSERT(ret == 0, "back compile failed");
        ret = run_cmd(JVM_EXE " test_sc_ret.bin > test_sc_ret.out 2>&1");
        TEST_ASSERT(ret == 0, "execution failed");
        string out;
        TEST_ASSERT(read_output("test_sc_ret.out", out), "read output");
        // fd should be > 0 (1 or more), printed as integer
        TEST_ASSERT(out == "1", "fd should be 1");
        std::remove("test_sc_ret.jvl"); std::remove("test_sc_ret.jvav");
        std::remove("test_sc_ret.bin"); std::remove("test_sc_ret.out");
        std::remove("test_return.txt");
    }
    test_passed("integration_syscall_return_int");

    test_header("integration_import_dup_cross_dir");
    {
        // Create directory structure: dir_a/c.jvl, dir_b/c.jvl
        // But we simulate cross-dir import by using different relative paths
        // A imports "sub/c.jvl" from "dir_a/", B imports "sub/c.jvl" from "dir_b/"
        // main imports A and B. C should only be emitted once.
#ifdef _WIN32
        system("mkdir dir_a 2>nul");
        system("mkdir dir_b 2>nul");
        system("mkdir dir_a\\sub 2>nul");
        system("mkdir dir_b\\sub 2>nul");
#else
        system("mkdir -p dir_a/sub dir_b/sub");
#endif
        const char* c_src = R"(
func helper(): int {
    return 42;
}
)";
        const char* a_src = R"(
import "sub/c.jvl";

func get_a(): int {
    return helper();
}
)";
        const char* b_src = R"(
import "sub/c.jvl";

func get_b(): int {
    return helper();
}
)";
        const char* main_src = R"(
import "dir_a/a.jvl";
import "dir_b/b.jvl";

func main(): int {
    putint(get_a());
    putint(get_b());
    return 0;
}
)";
        std::ofstream("dir_a/sub/c.jvl") << c_src;
        std::ofstream("dir_a/a.jvl") << a_src;
        std::ofstream("dir_b/sub/c.jvl") << c_src;
        std::ofstream("dir_b/b.jvl") << b_src;
        std::ofstream("main_dup.jvl") << main_src;
        int ret = run_cmd(JVAVC_FRONT_EXE " main_dup.jvl main_dup.jvav");
        TEST_ASSERT(ret == 0, "front cross-dir dup import");
        ret = run_cmd(JVAVC_BACK_EXE " main_dup.jvav main_dup.bin");
        TEST_ASSERT(ret == 0, "back cross-dir dup import");
        ret = run_cmd(JVM_EXE " main_dup.bin > main_dup.out 2>&1");
        TEST_ASSERT(ret == 0, "run cross-dir dup import");
        string out;
        TEST_ASSERT(read_output("main_dup.out", out), "read");
        TEST_ASSERT(out == "4242", "cross-dir dup import output");
        std::remove("dir_a/sub/c.jvl"); std::remove("dir_a/a.jvl");
        std::remove("dir_b/sub/c.jvl"); std::remove("dir_b/b.jvl");
        std::remove("main_dup.jvl"); std::remove("main_dup.jvav");
        std::remove("main_dup.bin"); std::remove("main_dup.out");
#ifdef _WIN32
        system("rmdir /s /q dir_a 2>nul");
        system("rmdir /s /q dir_b 2>nul");
#else
        system("rm -rf dir_a dir_b");
#endif
    }
    test_passed("integration_import_dup_cross_dir");

    test_header("integration_stdlib_file");
    {
        // Use absolute path to std/file.jvl since we're in the test working dir
        const char* src = R"(
import "std/file.jvl";
import "std/io.jvl";

func main(): int {
    var path = "test_file_io.txt";
    var mode = "wb";
    var fd = fopen(path, mode);
    if (fd == 0) {
        println(999);
        return 1;
    }
    var data = alloc(3);
    data[0] = 88; data[1] = 89; data[2] = 90;   // XYZ
    fwrite(fd, data, 3);
    fclose(fd);
    free(data);

    // Read back
    var rmode = "rb";
    var rfd = fopen(path, rmode);
    if (rfd == 0) {
        println(998);
        return 1;
    }
    var buf = alloc(3);
    fread(rfd, buf, 3);
    putchar(buf[0]);
    putchar(buf[1]);
    putchar(buf[2]);
    fclose(rfd);
    free(buf);
    return 0;
}
)";
        std::ofstream("test_file.jvl") << src;
        int ret = run_cmd(JVAVC_FRONT_EXE " test_file.jvl test_file.jvav");
        TEST_ASSERT(ret == 0, "front stdlib file compile");
        ret = run_cmd(JVAVC_BACK_EXE " test_file.jvav test_file.bin");
        TEST_ASSERT(ret == 0, "back stdlib file compile");
        ret = run_cmd(JVM_EXE " test_file.bin > test_file.out 2>&1");
        TEST_ASSERT(ret == 0, "run stdlib file");
        string out;
        TEST_ASSERT(read_output("test_file.out", out), "read");
        TEST_ASSERT(out == "XYZ", "file io output");
        std::remove("test_file.jvl"); std::remove("test_file.jvav");
        std::remove("test_file.bin"); std::remove("test_file.out");
        std::remove("test_file_io.txt");
    }
    test_passed("integration_stdlib_file");

    test_header("integration_jvm_load_error_msg");
    {
        int ret = run_cmd(JVM_EXE " nonexistent_file_xyz.bin > jvm_err.out 2>&1");
        // jvm returns 1 on failure
        TEST_ASSERT(ret == 1, "jvm should fail");
        string out;
        TEST_ASSERT(read_output("jvm_err.out", out), "read");
        TEST_ASSERT(out.find("nonexistent_file_xyz.bin") != string::npos, "error should contain filename");
        std::remove("jvm_err.out");
    }
    test_passed("integration_jvm_load_error_msg");

    // --- JIT tests ---
#ifdef _WIN32
    static const char* jit_env = "set JVAV_JIT=1 && ";
#else
    static const char* jit_env = "JVAV_JIT=1 ";
#endif

    test_header("jit_putint");
    {
        const char* src = R"(
func main(): int {
    putint(42);
    return 0;
}
)";
        std::ofstream("jit_putint.jvl") << src;
        string cmd = string(jit_env) + string(JVAVC_FRONT_EXE) + " jit_putint.jvl jit_putint.jvav";
        int ret = run_cmd(cmd.c_str());
        TEST_ASSERT(ret == 0, "front");
        cmd = string(jit_env) + string(JVAVC_BACK_EXE) + " jit_putint.jvav jit_putint.bin";
        ret = run_cmd(cmd.c_str());
        TEST_ASSERT(ret == 0, "back");
        cmd = string(jit_env) + string(JVM_EXE) + " jit_putint.bin > jit_putint.out 2>&1";
        ret = run_cmd(cmd.c_str());
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("jit_putint.out", out), "read");
        TEST_ASSERT(out == "42", "putint output");
        std::remove("jit_putint.jvl"); std::remove("jit_putint.jvav");
        std::remove("jit_putint.bin"); std::remove("jit_putint.out");
    }
    test_passed("jit_putint");

    test_header("jit_ret0");
    {
        const char* src = R"(
func main(): int { return 0; }
)";
        std::ofstream("jit_ret0.jvl") << src;
        string cmd = string(jit_env) + string(JVAVC_FRONT_EXE) + " jit_ret0.jvl jit_ret0.jvav";
        int ret = run_cmd(cmd.c_str());
        TEST_ASSERT(ret == 0, "front");
        cmd = string(jit_env) + string(JVAVC_BACK_EXE) + " jit_ret0.jvav jit_ret0.bin";
        ret = run_cmd(cmd.c_str());
        TEST_ASSERT(ret == 0, "back");
        cmd = string(jit_env) + string(JVM_EXE) + " jit_ret0.bin > jit_ret0.out 2>&1";
        ret = run_cmd(cmd.c_str());
        TEST_ASSERT(ret == 0, "run");
        std::remove("jit_ret0.jvl"); std::remove("jit_ret0.jvav");
        std::remove("jit_ret0.bin"); std::remove("jit_ret0.out");
    }
    test_passed("jit_ret0");

    test_header("jit_hello");
    {
        const char* src = R"(
func main(): int {
    putchar(72); putchar(105); putchar(33);
    return 0;
}
)";
        std::ofstream("jit_hello.jvl") << src;
        string cmd = string(jit_env) + string(JVAVC_FRONT_EXE) + " jit_hello.jvl jit_hello.jvav";
        int ret = run_cmd(cmd.c_str());
        TEST_ASSERT(ret == 0, "front");
        cmd = string(jit_env) + string(JVAVC_BACK_EXE) + " jit_hello.jvav jit_hello.bin";
        ret = run_cmd(cmd.c_str());
        TEST_ASSERT(ret == 0, "back");
        cmd = string(jit_env) + string(JVM_EXE) + " jit_hello.bin > jit_hello.out 2>&1";
        ret = run_cmd(cmd.c_str());
        TEST_ASSERT(ret == 0, "run");
        string out;
        TEST_ASSERT(read_output("jit_hello.out", out), "read");
        TEST_ASSERT(out == "Hi!", "hello output");
        std::remove("jit_hello.jvl"); std::remove("jit_hello.jvav");
        std::remove("jit_hello.bin"); std::remove("jit_hello.out");
    }
    test_passed("jit_hello");

    return 0;
}
