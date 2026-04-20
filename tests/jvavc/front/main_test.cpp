#include <cstdio>

extern int test_lexer_main();
extern int test_parser_main();
extern int test_sema_main();
extern int test_codegen_main();
extern int test_integration_main();

extern int g_tests_passed;
extern int g_tests_failed;

int main() {
    printf("Running JVAVC Front tests...\n");
    test_lexer_main();
    test_parser_main();
    test_sema_main();
    test_codegen_main();
    test_integration_main();
    printf("\n%d tests passed, %d failed\n", g_tests_passed, g_tests_failed);
    return g_tests_failed > 0 ? 1 : 0;
}
