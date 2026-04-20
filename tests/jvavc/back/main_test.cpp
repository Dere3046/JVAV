#include <cstdio>

extern int test_parser_main();
extern int test_encoder_main();
extern int test_integration_main();

extern int g_tests_passed;
extern int g_tests_failed;

int main() {
    printf("Running JVAVC tests...\n");
    test_parser_main();
    test_encoder_main();
    test_integration_main();
    printf("\n%d tests passed, %d failed\n", g_tests_passed, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}
