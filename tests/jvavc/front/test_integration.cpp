#include "test_utils.hpp"
#include <cstdlib>
#include <fstream>
#include <cstdio>

static int run_cmd(const char *cmd) {
    return system(cmd);
}

int test_integration_main() {
    test_header("integration_putint");
    {
        const char* src = R"(
func main(): int {
    putint(42);
    return 0;
}
)";
        std::ofstream("test_putint.jvl") << src;
        int ret = run_cmd("jvavc\\front\\jvavc-front.exe test_putint.jvl test_putint.jvav");
        TEST_ASSERT(ret == 0, "front compile failed");
        ret = run_cmd("jvavc\\back\\jvavc.exe test_putint.jvav test_putint.bin");
        TEST_ASSERT(ret == 0, "back compile failed");
        ret = run_cmd("jvm\\jvm.exe test_putint.bin > test_putint.out 2>&1");
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
        int ret = run_cmd("jvavc\\front\\jvavc-front.exe test_imp_main.jvl test_imp_main.jvav");
        TEST_ASSERT(ret == 0, "front compile failed");
        ret = run_cmd("jvavc\\back\\jvavc.exe test_imp_main.jvav test_imp_main.bin");
        TEST_ASSERT(ret == 0, "back compile failed");
        ret = run_cmd("jvm\\jvm.exe test_imp_main.bin > test_imp_main.out 2>&1");
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

    return 0;
}
